#include "WorkOrderDao.h"

#include <QDateTime>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace {

QString nowStr()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

// 见 AdminDao 里同名函数的说明: 缺失的 JSON key 会变成 null QString → SQL NULL
QString nz(const QString &s)
{
    return s.isNull() ? QString::fromLatin1("") : s;
}

const char *kSelectWorkOrder =
    "SELECT w.id, IFNULL(w.user_id,0), IFNULL(w.station_id,0), IFNULL(w.charger_id,0), "
    "       w.type, w.priority, w.title, w.description, w.status, w.handler, w.result, "
    "       w.create_time, IFNULL(w.handle_time,''), "
    "       IFNULL((SELECT s.name FROM station s WHERE s.id=w.station_id),'') "
    "FROM work_order w ";

WorkOrderRow rowToWorkOrder(const QSqlQuery &q)
{
    WorkOrderRow w;
    w.id          = q.value(0).toInt();
    w.userId      = q.value(1).toInt();
    w.stationId   = q.value(2).toInt();
    w.chargerId   = q.value(3).toInt();
    w.type        = q.value(4).toString();
    w.priority    = q.value(5).toString();
    w.title       = q.value(6).toString();
    w.description = q.value(7).toString();
    w.status      = q.value(8).toString();
    w.handler     = q.value(9).toString();
    w.result      = q.value(10).toString();
    w.createTime  = q.value(11).toString();
    w.handleTime  = q.value(12).toString();
    w.stationName = q.value(13).toString();
    return w;
}

// schema.sql 的 CHECK 只允许这五种, 传错会被数据库直接拒绝, 所以在这里先纠正
QString normalizeType(const QString &t)
{
    static const QStringList kTypes{
        QStringLiteral("user_complaint"), QStringLiteral("device_fault"),
        QStringLiteral("refund"), QStringLiteral("maintenance"),
        QStringLiteral("abnormal_order")};
    return kTypes.contains(t) ? t : QStringLiteral("user_complaint");
}

QString normalizeStatus(const QString &s)
{
    static const QStringList kStatuses{
        QStringLiteral("pending"), QStringLiteral("processing"),
        QStringLiteral("completed"), QStringLiteral("closed")};
    if (s.trimmed().isEmpty())
        return QStringLiteral("completed");
    return kStatuses.contains(s) ? s : QStringLiteral("completed");
}

std::optional<WorkOrderRow> findWorkOrderAny(int workOrderId)
{
    QSqlQuery q;
    q.prepare(QString::fromLatin1(kSelectWorkOrder) + QStringLiteral("WHERE w.id=?"));
    q.addBindValue(workOrderId);
    if (!q.exec() || !q.next()) return std::nullopt;
    return rowToWorkOrder(q);
}

}  // namespace

namespace dao {

std::optional<WorkOrderRow> createWorkOrder(int userId, const QString &type,
                                            int stationId, int chargerId,
                                            const QString &title, const QString &description)
{
    const QString t = normalizeType(type);
    // 设备故障优先级默认高一些, 用户投诉走 medium
    const QString priority = (t == QStringLiteral("device_fault"))
                                 ? QStringLiteral("high") : QStringLiteral("medium");

    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO work_order(type, priority, user_id, station_id, charger_id, "
        "  title, description, status, create_time) VALUES(?,?,?,?,?,?,?,'pending',?)"));
    q.addBindValue(t);
    q.addBindValue(priority);
    q.addBindValue(userId > 0 ? QVariant(userId) : QVariant());
    q.addBindValue(stationId > 0 ? QVariant(stationId) : QVariant());
    q.addBindValue(chargerId > 0 ? QVariant(chargerId) : QVariant());
    q.addBindValue(title.trimmed().isEmpty() ? QStringLiteral("用户反馈") : title.trimmed());
    q.addBindValue(nz(description));
    q.addBindValue(nowStr());
    if (!q.exec())
        return std::nullopt;
    return findWorkOrder(userId, q.lastInsertId().toInt());
}

QList<WorkOrderRow> listWorkOrders(int userId)
{
    QList<WorkOrderRow> out;
    QSqlQuery q;
    q.prepare(QString::fromLatin1(kSelectWorkOrder)
              + QStringLiteral("WHERE w.user_id=? ORDER BY w.id DESC"));
    q.addBindValue(userId);
    if (!q.exec()) return out;
    while (q.next()) out.append(rowToWorkOrder(q));
    return out;
}

QList<WorkOrderRow> listAllWorkOrders(const QString &status)
{
    QList<WorkOrderRow> out;
    QSqlQuery q;
    const QString s = status.trimmed();
    if (s.isEmpty()) {
        q.prepare(QString::fromLatin1(kSelectWorkOrder)
                  + QStringLiteral("ORDER BY CASE w.status "
                                   "WHEN 'pending' THEN 0 WHEN 'processing' THEN 1 "
                                   "WHEN 'completed' THEN 2 ELSE 3 END, w.id DESC"));
    } else {
        q.prepare(QString::fromLatin1(kSelectWorkOrder)
                  + QStringLiteral("WHERE w.status=? ORDER BY w.id DESC"));
        q.addBindValue(normalizeStatus(s));
    }
    if (!q.exec()) return out;
    while (q.next()) out.append(rowToWorkOrder(q));
    return out;
}

std::optional<WorkOrderRow> findWorkOrder(int userId, int workOrderId)
{
    QSqlQuery q;
    q.prepare(QString::fromLatin1(kSelectWorkOrder) + QStringLiteral("WHERE w.id=?"));
    q.addBindValue(workOrderId);
    if (!q.exec() || !q.next()) return std::nullopt;
    const WorkOrderRow w = rowToWorkOrder(q);
    if (w.userId != userId) return std::nullopt;    // 不是你的工单 → 当作不存在
    return w;
}

std::optional<WorkOrderRow> handleWorkOrder(int workOrderId, const QString &handler,
                                            const QString &status, const QString &result)
{
    if (workOrderId <= 0)
        return std::nullopt;

    const QString nextStatus = normalizeStatus(status);
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE work_order SET status=?, handler=?, result=?, handle_time=? WHERE id=?"));
    q.addBindValue(nextStatus);
    q.addBindValue(nz(handler));
    q.addBindValue(nz(result));
    q.addBindValue(nowStr());
    q.addBindValue(workOrderId);
    if (!q.exec() || q.numRowsAffected() <= 0)
        return std::nullopt;
    return findWorkOrderAny(workOrderId);
}

}  // namespace dao
