#include "AdminDao.h"

#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

namespace {

QString nowStr()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

StationFull rowToStation(const QSqlQuery &q)
{
    StationFull s;
    s.id             = q.value(0).toInt();
    s.name           = q.value(1).toString();
    s.address        = q.value(2).toString();
    s.area           = q.value(3).toString();
    s.longitude      = q.value(4).toDouble();
    s.latitude       = q.value(5).toDouble();
    s.totalChargers  = q.value(6).toInt();
    s.serviceFee     = q.value(7).toDouble();
    s.parkingFee     = q.value(8).toDouble();
    s.businessHours  = q.value(9).toString();
    s.facilitiesJson = q.value(10).toString();
    s.ownerType      = q.value(11).toString();
    s.hasSwap        = q.value(12).toInt();
    s.freeChargers   = q.value(13).toInt();   // 实时 COUNT
    const int online = q.value(14).toInt();   // 实时在线台数(非 offline/fault)
    const int total  = q.value(15).toInt();   // 实时总台数
    // online_rate 不信任表里的存量字段, 实时算(见 server/AGENTS.md 的约定)
    s.onlineRate = (total > 0) ? (online * 100.0 / total) : 100.0;
    s.totalChargers = (total > 0) ? total : s.totalChargers;
    return s;
}

// 电站查询: 表字段 + 三个实时统计子查询
const char *kSelectStation =
    "SELECT s.id, s.name, s.address, s.area, s.longitude, s.latitude, s.total_chargers, "
    "       s.service_fee, s.parking_fee, s.business_hours, s.facilities, s.owner_type, s.has_swap, "
    "       (SELECT COUNT(*) FROM charger c WHERE c.station_id=s.id AND c.status='idle'), "
    "       (SELECT COUNT(*) FROM charger c WHERE c.station_id=s.id "
    "                                        AND c.status NOT IN ('offline','fault')), "
    "       (SELECT COUNT(*) FROM charger c WHERE c.station_id=s.id) "
    "FROM station s ";

ChargerFull rowToCharger(const QSqlQuery &q)
{
    ChargerFull c;
    c.id                  = q.value(0).toInt();
    c.code                = q.value(1).toString();
    c.stationId           = q.value(2).toInt();
    c.type                = q.value(3).toString();
    c.power               = q.value(4).toDouble();
    c.status              = q.value(5).toString();
    c.voltage             = q.value(6).toDouble();
    c.current             = q.value(7).toDouble();
    c.temperature         = q.value(8).toDouble();
    c.faultCode           = q.value(9).toString();
    c.commStatus          = q.value(10).toString();
    c.healthScore         = q.value(11).toInt();
    c.totalChargeCount    = q.value(12).toInt();
    c.totalChargeDuration = q.value(13).toInt();
    c.createdTime         = q.value(14).toString();
    c.stationName         = q.value(15).toString();
    return c;
}

const char *kSelectCharger =
    "SELECT c.id, c.code, c.station_id, c.type, c.power, c.status, c.voltage, c.current, "
    "       c.temperature, c.fault_code, c.comm_status, c.health_score, "
    "       c.total_charge_count, c.total_charge_duration, c.created_time, s.name "
    "FROM charger c JOIN station s ON s.id = c.station_id ";

UserRow rowToUserRow(const QSqlQuery &q)
{
    UserRow u;
    u.id            = q.value(0).toInt();
    u.phone         = q.value(1).toString();
    u.nickname      = q.value(2).toString();
    u.avatarPath    = q.value(3).toString();
    u.balance       = q.value(4).toDouble();
    u.points        = q.value(5).toInt();
    u.level         = q.value(6).toString();
    u.status        = q.value(7).toString();
    u.registerTime  = q.value(8).toString();
    u.lastLoginTime = q.value(9).toString();
    return u;
}

const char *kSelectUserRow =
    "SELECT id, phone, nickname, avatar_path, balance, points, level, status, "
    "       register_time, IFNULL(last_login_time,'') "
    "FROM user ";

DeviceLogRow rowToLog(const QSqlQuery &q)
{
    DeviceLogRow l;
    l.id        = q.value(0).toInt();
    l.chargerId = q.value(1).toInt();
    l.action    = q.value(2).toString();
    l.op        = q.value(3).toString();
    l.opTime    = q.value(4).toString();
    l.result    = q.value(5).toString();
    return l;
}

}  // namespace

namespace dao {

// ============================ 认证 ============================
std::optional<AdminAccount> adminLogin(const QString &account, const QString &password)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT id, account FROM admin WHERE account = ? AND password = ?"));
    q.addBindValue(account);
    q.addBindValue(password);
    if (!q.exec() || !q.next())
        return std::nullopt;
    AdminAccount a;
    a.id      = q.value(0).toInt();
    a.account = q.value(1).toString();
    return a;
}

// ============================ 统计 ============================
RevenueSummary revenue(int days)
{
    if (days <= 0) days = 7;
    RevenueSummary r;

    // 1) 把有订单的日子查出来放进 map
    QMap<QString, double> byDate;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT substr(settle_time,1,10) AS d, ROUND(SUM(pay_amount),2) "
        "FROM charging_order WHERE status='completed' AND settle_time IS NOT NULL "
        "GROUP BY d"));
    if (q.exec())
        while (q.next())
            byDate.insert(q.value(0).toString(), q.value(1).toDouble());

    // 2) 从 (今天-days+1) 到今天逐日铺开, 没有订单的补 0 —— 前端折线才不会断
    const QDate today = QDate::currentDate();
    for (int i = days - 1; i >= 0; --i) {
        const QString d = today.addDays(-i).toString(QStringLiteral("yyyy-MM-dd"));
        r.trend.append(RevenuePoint{d, byDate.value(d, 0.0)});
    }
    r.today = byDate.value(today.toString(QStringLiteral("yyyy-MM-dd")), 0.0);

    // 3) 本月 / 累计
    QSqlQuery m;
    m.prepare(QStringLiteral(
        "SELECT IFNULL(ROUND(SUM(pay_amount),2),0) FROM charging_order "
        "WHERE status='completed' AND substr(settle_time,1,7) = ?"));
    m.addBindValue(today.toString(QStringLiteral("yyyy-MM")));
    if (m.exec() && m.next()) r.month = m.value(0).toDouble();

    QSqlQuery t;
    if (t.exec(QStringLiteral(
            "SELECT IFNULL(ROUND(SUM(pay_amount),2),0) FROM charging_order WHERE status='completed'"))
        && t.next())
        r.total = t.value(0).toDouble();

    return r;
}

StatusDistribution chargerStatusDistribution()
{
    StatusDistribution d;
    QSqlQuery q;
    if (!q.exec(QStringLiteral("SELECT status, COUNT(*) FROM charger GROUP BY status")))
        return d;
    while (q.next()) {
        const QString s = q.value(0).toString();
        const int     n = q.value(1).toInt();
        d.total += n;
        if      (s == QStringLiteral("charging"))  d.charging  = n;
        else if (s == QStringLiteral("idle"))      d.idle      = n;
        else if (s == QStringLiteral("fault"))     d.fault     = n;
        else if (s == QStringLiteral("offline"))   d.offline   = n;
        else if (s == QStringLiteral("reserved"))  d.reserved  = n;
        else if (s == QStringLiteral("rebooting")) d.rebooting = n;
    }
    return d;
}

QList<RiskRow> faultRisks(int limit)
{
    if (limit <= 0) limit = 5;
    QList<RiskRow> out;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT id, health_score FROM charger ORDER BY health_score ASC, id ASC LIMIT ?"));
    q.addBindValue(limit);
    if (!q.exec()) return out;
    while (q.next()) {
        RiskRow r;
        r.chargerId   = q.value(0).toInt();
        r.healthScore = q.value(1).toInt();
        r.riskLevel   = r.healthScore < 60 ? QStringLiteral("高")
                      : r.healthScore < 80 ? QStringLiteral("中")
                                           : QStringLiteral("低");
        r.suggestion  = r.healthScore < 60 ? QStringLiteral("建议尽快派单检修")
                      : r.healthScore < 80 ? QStringLiteral("建议远程重启并观察")
                                           : QStringLiteral("状态良好, 定期巡检即可");
        out.append(r);
    }
    return out;
}

// ============================ 列表 ============================
QList<StationFull> listStationsFull()
{
    QList<StationFull> out;
    QSqlQuery q;
    if (!q.exec(QString::fromLatin1(kSelectStation) + QStringLiteral("ORDER BY s.id")))
        return out;
    while (q.next()) out.append(rowToStation(q));
    return out;
}

std::optional<StationFull> findStationFullById(int stationId)
{
    QSqlQuery q;
    q.prepare(QString::fromLatin1(kSelectStation) + QStringLiteral("WHERE s.id = ?"));
    q.addBindValue(stationId);
    if (!q.exec() || !q.next())
        return std::nullopt;
    return rowToStation(q);
}

QList<ChargerFull> listChargers(int stationId)
{
    QList<ChargerFull> out;
    QSqlQuery q;
    if (stationId > 0) {
        q.prepare(QString::fromLatin1(kSelectCharger)
                  + QStringLiteral("WHERE c.station_id = ? ORDER BY c.id"));
        q.addBindValue(stationId);
    } else {
        q.prepare(QString::fromLatin1(kSelectCharger) + QStringLiteral("ORDER BY c.id"));
    }
    if (!q.exec()) return out;
    while (q.next()) out.append(rowToCharger(q));
    return out;
}

QList<UserRow> listUsers(const QString &keyword)
{
    QList<UserRow> out;
    QSqlQuery q;
    if (keyword.trimmed().isEmpty()) {
        q.prepare(QString::fromLatin1(kSelectUserRow) + QStringLiteral("ORDER BY id"));
    } else {
        // 参数化的模糊查询: % 拼在绑定值里, 不拼进 SQL 文本(防注入)
        q.prepare(QString::fromLatin1(kSelectUserRow)
                  + QStringLiteral("WHERE phone LIKE ? OR nickname LIKE ? ORDER BY id"));
        const QString like = QStringLiteral("%") + keyword.trimmed() + QStringLiteral("%");
        q.addBindValue(like);
        q.addBindValue(like);
    }
    if (!q.exec()) return out;
    while (q.next()) out.append(rowToUserRow(q));
    return out;
}

std::optional<UserRow> findUserRowById(int userId)
{
    QSqlQuery q;
    q.prepare(QString::fromLatin1(kSelectUserRow) + QStringLiteral("WHERE id = ?"));
    q.addBindValue(userId);
    if (!q.exec() || !q.next())
        return std::nullopt;
    return rowToUserRow(q);
}

QList<DeviceLogRow> listDeviceLogs(int chargerId)
{
    QList<DeviceLogRow> out;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT id, charger_id, action, operator, op_time, result FROM device_log "
        "WHERE charger_id = ? ORDER BY op_time DESC, id DESC"));
    q.addBindValue(chargerId);
    if (!q.exec()) return out;
    while (q.next()) out.append(rowToLog(q));
    return out;
}

// ============================ 写操作 ============================
std::optional<DeviceLogRow> chargerAction(int chargerId, const QString &action,
                                          const QString &opAccount)
{
    // 目标状态: restart → rebooting, pause → offline
    const QString newStatus = (action == QStringLiteral("restart"))
                                  ? QStringLiteral("rebooting")
                                  : QStringLiteral("offline");

    QSqlQuery exists;
    exists.prepare(QStringLiteral("SELECT id FROM charger WHERE id = ?"));
    exists.addBindValue(chargerId);
    if (!exists.exec() || !exists.next())
        return std::nullopt;                       // 4001 电桩不存在

    QSqlDatabase::database().transaction();

    QSqlQuery up;
    up.prepare(QStringLiteral("UPDATE charger SET status = ? WHERE id = ?"));
    up.addBindValue(newStatus);
    up.addBindValue(chargerId);
    if (!up.exec()) {
        QSqlDatabase::database().rollback();
        return std::nullopt;
    }

    QSqlQuery ins;
    ins.prepare(QStringLiteral(
        "INSERT INTO device_log(charger_id, action, operator, op_time, result) "
        "VALUES(?, ?, ?, ?, 'success')"));
    ins.addBindValue(chargerId);
    ins.addBindValue(action);
    ins.addBindValue(opAccount);
    ins.addBindValue(nowStr());
    if (!ins.exec()) {
        QSqlDatabase::database().rollback();
        return std::nullopt;
    }
    const int logId = ins.lastInsertId().toInt();

    QSqlDatabase::database().commit();

    DeviceLogRow l;
    l.id        = logId;
    l.chargerId = chargerId;
    l.action    = action;
    l.op        = opAccount;
    l.opTime    = nowStr();
    l.result    = QStringLiteral("success");
    return l;
}

std::optional<UserRow> setUserStatus(int userId, const QString &status)
{
    if (status != QStringLiteral("normal") && status != QStringLiteral("frozen"))
        return std::nullopt;
    QSqlQuery up;
    up.prepare(QStringLiteral("UPDATE user SET status = ? WHERE id = ?"));
    up.addBindValue(status);
    up.addBindValue(userId);
    if (!up.exec() || up.numRowsAffected() == 0)
        return std::nullopt;
    return findUserRowById(userId);
}

std::optional<StationFull> addStation(const StationFull &s)
{
    QSqlQuery ins;
    ins.prepare(QStringLiteral(
        "INSERT INTO station(name, address, area, longitude, latitude, total_chargers, "
        "                    online_rate, service_fee, parking_fee, business_hours, "
        "                    facilities, owner_type, has_swap) "
        "VALUES(?,?,?,?,?,?,100,?,?,?,?,?,?)"));
    ins.addBindValue(s.name);
    ins.addBindValue(s.address);
    ins.addBindValue(s.area);
    ins.addBindValue(s.longitude);
    ins.addBindValue(s.latitude);
    ins.addBindValue(s.totalChargers);
    ins.addBindValue(s.serviceFee);
    ins.addBindValue(s.parkingFee);
    ins.addBindValue(s.businessHours.isEmpty() ? QStringLiteral("00:00-24:00") : s.businessHours);
    ins.addBindValue(s.facilitiesJson.isEmpty() ? QStringLiteral("[]") : s.facilitiesJson);
    ins.addBindValue(s.ownerType.isEmpty() ? QStringLiteral("self_run") : s.ownerType);
    ins.addBindValue(s.hasSwap);
    if (!ins.exec())
        return std::nullopt;
    return findStationFullById(ins.lastInsertId().toInt());
}

}  // namespace dao
