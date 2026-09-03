#include "StationDao.h"

#include <QSqlQuery>
#include <QTime>
#include <QVariant>

namespace {

const char *kSelectStation =
    "SELECT s.id, s.name, s.address, s.area, s.longitude, s.latitude, "
    "       s.total_chargers, s.service_fee, "
    "       (SELECT COUNT(*) FROM charger c "
    "         WHERE c.station_id = s.id AND c.status = 'idle') AS free_cnt "
    "FROM station s ";

StationView rowToStation(const QSqlQuery &q)
{
    StationView v;
    v.id            = q.value(0).toInt();
    v.name          = q.value(1).toString();
    v.address       = q.value(2).toString();
    v.area          = q.value(3).toString();
    v.longitude     = q.value(4).toDouble();
    v.latitude      = q.value(5).toDouble();
    v.totalChargers = q.value(6).toInt();
    v.serviceFee    = q.value(7).toDouble();
    v.freeChargers  = q.value(8).toInt();
    return v;
}

}  // namespace

namespace dao {

QList<StationView> listStations()
{
    // 子查询现算空闲桩数 —— "指标不建表"的落地:
    // 空闲数随桩状态实时变化, 存库必然过期, 所以每次都从 charger 表数出来。
    QList<StationView> list;
    QSqlQuery q;
    q.exec(QString(kSelectStation) + QStringLiteral("ORDER BY s.id"));
    while (q.next())
        list.append(rowToStation(q));
    return list;
}

std::optional<StationView> findStationById(int stationId)
{
    QSqlQuery q;
    q.prepare(QString(kSelectStation) + QStringLiteral("WHERE s.id = ?"));
    q.addBindValue(stationId);
    if (!q.exec() || !q.next())
        return std::nullopt;
    return rowToStation(q);
}

QList<ChargerView> listChargersOfStation(int stationId)
{
    QList<ChargerView> list;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT id, code, type, power, status, health_score "
        "FROM charger WHERE station_id = ? ORDER BY code"));
    q.addBindValue(stationId);
    q.exec();
    while (q.next()) {
        ChargerView c;
        c.id          = q.value(0).toInt();
        c.code        = q.value(1).toString();
        c.type        = q.value(2).toString();
        c.power       = q.value(3).toDouble();
        c.status      = q.value(4).toString();
        c.healthScore = q.value(5).toInt();
        list.append(c);
    }
    return list;
}

double currentPrice(int stationId)
{
    // 1. 按当前小时判定档位(与种子数据的 price_rule.time_range 划分一致)
    const int hour = QTime::currentTime().hour();
    QString level = QStringLiteral("flat");
    if (hour < 8)                      level = QStringLiteral("valley");
    else if (hour >= 17 && hour < 21)  level = QStringLiteral("peak");

    // 2. 专属站规则优先, 其次全站通用(station_id 为空)规则
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT price FROM price_rule "
        "WHERE level = ? AND (station_id = ? OR station_id IS NULL) "
        "ORDER BY station_id IS NULL ASC LIMIT 1"));   // 非空(专属)排前面
    q.addBindValue(level);
    q.addBindValue(stationId);
    if (q.exec() && q.next())
        return q.value(0).toDouble();
    return 0.7;   // 库里连通用规则都没有时的兜底价
}

}  // namespace dao
