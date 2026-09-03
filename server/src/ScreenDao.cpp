#include "ScreenDao.h"

#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QMap>
#include <QSqlQuery>
#include <QVariant>
#include <algorithm>

namespace {

QString todayStr()    { return QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")); }
QString yesterdayStr(){ return QDate::currentDate().addDays(-1).toString(QStringLiteral("yyyy-MM-dd")); }
QString hhmmssNow()   { return QTime::currentTime().toString(QStringLiteral("HH:mm:ss")); }

// 今天已经过去了多少分钟
int minutesElapsedToday()
{
    return qMax(1, QTime(0, 0).secsTo(QTime::currentTime()) / 60);
}

double scalar(const QString &sql, const QVariantList &binds = {})
{
    QSqlQuery q;
    q.prepare(sql);
    for (const auto &b : binds) q.addBindValue(b);
    if (!q.exec() || !q.next()) return 0.0;
    return q.value(0).toDouble();
}

// (今 - 昨同期) / 昨同期 × 100; 昨同期为 0 时返回 0
double changePct(double todayVal, double yesterdayVal)
{
    if (qFuzzyIsNull(yesterdayVal)) return 0.0;
    return std::round((todayVal - yesterdayVal) / yesterdayVal * 1000.0) / 10.0;
}

}  // namespace

namespace dao {

ScreenMetrics screenMetrics()
{
    ScreenMetrics m;
    const QString today = todayStr(), yest = yesterdayStr(), nowT = hhmmssNow();

    m.todayRevenue = scalar(
        QStringLiteral("SELECT IFNULL(ROUND(SUM(pay_amount),2),0) FROM charging_order "
                       "WHERE status='completed' AND substr(settle_time,1,10)=?"),
        {today});

    m.todayOrders = static_cast<int>(scalar(
        QStringLiteral("SELECT COUNT(*) FROM charging_order WHERE substr(create_time,1,10)=?"),
        {today}));

    m.chargingCount = static_cast<int>(scalar(
        QStringLiteral("SELECT COUNT(*) FROM charger WHERE status='charging'")));

    const double online = scalar(
        QStringLiteral("SELECT COUNT(*) FROM charger WHERE status NOT IN ('offline','fault')"));
    const double total = scalar(QStringLiteral("SELECT COUNT(*) FROM charger"));
    m.onlineRate = (total > 0) ? std::round(online / total * 1000.0) / 10.0 : 100.0;

    // 昨日"同期" = 昨天 00:00 到昨天的此刻, 这样比较才公平(不能拿今天半天比昨天一整天)
    const double yRevenue = scalar(
        QStringLiteral("SELECT IFNULL(ROUND(SUM(pay_amount),2),0) FROM charging_order "
                       "WHERE status='completed' AND substr(settle_time,1,10)=? "
                       "AND substr(settle_time,12,8) <= ?"),
        {yest, nowT});
    const double yOrders = scalar(
        QStringLiteral("SELECT COUNT(*) FROM charging_order "
                       "WHERE substr(create_time,1,10)=? AND substr(create_time,12,8) <= ?"),
        {yest, nowT});

    m.revenueChangePct = changePct(m.todayRevenue, yRevenue);
    m.ordersChangePct  = changePct(m.todayOrders, yOrders);
    return m;
}

QList<ScreenStation> screenStations()
{
    QList<ScreenStation> out;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT s.id, s.name, s.longitude, s.latitude, "
        "  (SELECT COUNT(*) FROM charger c WHERE c.station_id=s.id AND c.status='idle'), "
        "  (SELECT COUNT(*) FROM charger c WHERE c.station_id=s.id AND c.status='charging'), "
        "  (SELECT COUNT(*) FROM charger c WHERE c.station_id=s.id), "
        "  (SELECT IFNULL(ROUND(SUM(o.pay_amount),2),0) FROM charging_order o "
        "     WHERE o.station_id=s.id AND o.status='completed' AND substr(o.settle_time,1,10)=?) "
        "FROM station s ORDER BY s.id"));
    q.addBindValue(todayStr());
    if (!q.exec()) return out;
    while (q.next()) {
        ScreenStation s;
        s.id            = q.value(0).toInt();
        s.name          = q.value(1).toString();
        s.longitude     = q.value(2).toDouble();
        s.latitude      = q.value(3).toDouble();
        s.idleChargers  = q.value(4).toInt();
        const int charging = q.value(5).toInt();
        s.totalChargers = q.value(6).toInt();
        s.todayRevenue  = q.value(7).toDouble();
        s.loadRate = (s.totalChargers > 0)
                         ? std::round(charging * 1000.0 / s.totalChargers) / 10.0 : 0.0;
        out.append(s);
    }
    return out;
}

QList<LoadPoint> loadSeriesActual(int hours)
{
    if (hours <= 0) hours = 24;
    QList<LoadPoint> out;
    QSqlQuery q;
    // charging_measure是每桩每15分钟一条
    q.prepare(QStringLiteral(
        "SELECT measure_time, ROUND(SUM(power_kw),2) FROM charging_measure "
        "WHERE measure_time >= datetime('now','localtime',?) "
        "GROUP BY measure_time ORDER BY measure_time"));
    q.addBindValue(QStringLiteral("-%1 hours").arg(hours));
    if (!q.exec()) return out;
    while (q.next())
        out.append(LoadPoint{q.value(0).toString(), q.value(1).toDouble()});
    return out;
}

QList<UtilizationRow> utilizationRank(int limit)
{
    if (limit <= 0) limit = 10;
    QList<UtilizationRow> out;
    const int elapsed = minutesElapsedToday();
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT s.id, s.name, "
        "  IFNULL((SELECT SUM(o.duration_min) FROM charging_order o "
        "            WHERE o.station_id=s.id AND substr(o.create_time,1,10)=?),0), "
        "  (SELECT COUNT(*) FROM charger c WHERE c.station_id=s.id) "
        "FROM station s"));
    q.addBindValue(todayStr());
    if (!q.exec()) return out;
    while (q.next()) {
        UtilizationRow r;
        r.stationId   = q.value(0).toInt();
        r.stationName = q.value(1).toString();
        const double usedMin  = q.value(2).toDouble();
        const int    chargers = q.value(3).toInt();
        const double capacity = static_cast<double>(chargers) * elapsed;
        r.utilizationRate = (capacity > 0)
            ? std::min(100.0, std::round(usedMin / capacity * 1000.0) / 10.0) : 0.0;
        out.append(r);
    }
    std::sort(out.begin(), out.end(), [](const UtilizationRow &a, const UtilizationRow &b) {
        return a.utilizationRate > b.utilizationRate;   // 降序
    });
    if (out.size() > limit) out = out.mid(0, limit);
    return out;
}

QList<AlarmRow> recentAlarms(int limit)
{
    if (limit <= 0) limit = 20;
    QList<AlarmRow> out;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT a.id, a.station_id, s.name, IFNULL(a.charger_id,0), a.type, a.level, "
        "       a.status, a.occur_time "
        "FROM alarm a JOIN station s ON s.id=a.station_id "
        "ORDER BY a.occur_time DESC, a.id DESC LIMIT ?"));
    q.addBindValue(limit);
    if (!q.exec()) return out;
    while (q.next()) {
        AlarmRow a;
        a.id          = q.value(0).toInt();
        a.stationId   = q.value(1).toInt();
        a.stationName = q.value(2).toString();
        a.chargerId   = q.value(3).toInt();
        a.type        = q.value(4).toString();
        a.level       = q.value(5).toString();
        a.status      = q.value(6).toString();
        a.occurTime   = q.value(7).toString();
        out.append(a);
    }
    return out;
}

QList<UserGrowthPoint> userGrowth(int days)
{
    if (days <= 0) days = 7;
    QMap<QString, int> byDate;
    QSqlQuery q;
    if (q.exec(QStringLiteral(
            "SELECT substr(register_time,1,10) d, COUNT(*) FROM user GROUP BY d")))
        while (q.next())
            byDate.insert(q.value(0).toString(), q.value(1).toInt());

    QList<UserGrowthPoint> out;
    const QDate today = QDate::currentDate();
    for (int i = days - 1; i >= 0; --i) {
        const QString d = today.addDays(-i).toString(QStringLiteral("yyyy-MM-dd"));
        out.append(UserGrowthPoint{d, byDate.value(d, 0)});   // 缺失的日子补 0
    }
    return out;
}

EnergyByLevel energyByPriceLevel()
{
    EnergyByLevel e;
    QSqlQuery q;

    q.prepare(QStringLiteral(
        "SELECT price_level, ROUND(SUM(energy_kwh),2) FROM charging_order "
        "WHERE status='completed' AND settle_time >= datetime('now','localtime','-7 days') "
        "GROUP BY price_level"));
    if (!q.exec()) return e;
    while (q.next()) {
        const QString lv = q.value(0).toString();
        const double  v  = q.value(1).toDouble();
        if      (lv == QStringLiteral("valley")) e.valley = v;
        else if (lv == QStringLiteral("peak"))   e.peak   = v;
        else                                     e.flat   = v;
    }
    return e;
}

QList<EventRow> recentEvents(int limit)
{
    if (limit <= 0) limit = 20;
    QList<EventRow> out;
    // 从今日订单的三个时间点生成事件流 手机号中间四位打码后再拼文案
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT * FROM ("
        "  SELECT o.id AS oid, 'order_created' AS et, o.create_time AS t, "
        "         u.phone AS ph, s.name AS sn, c.code AS cc "
        "    FROM charging_order o JOIN user u ON u.id=o.user_id "
        "    JOIN station s ON s.id=o.station_id JOIN charger c ON c.id=o.charger_id "
        "   WHERE substr(o.create_time,1,10)=? "
        "  UNION ALL "
        "  SELECT o.id, 'order_started', o.start_time, u.phone, s.name, c.code "
        "    FROM charging_order o JOIN user u ON u.id=o.user_id "
        "    JOIN station s ON s.id=o.station_id JOIN charger c ON c.id=o.charger_id "
        "   WHERE o.start_time IS NOT NULL AND substr(o.start_time,1,10)=? "
        "  UNION ALL "
        "  SELECT o.id, 'order_completed', o.settle_time, u.phone, s.name, c.code "
        "    FROM charging_order o JOIN user u ON u.id=o.user_id "
        "    JOIN station s ON s.id=o.station_id JOIN charger c ON c.id=o.charger_id "
        "   WHERE o.status='completed' AND substr(o.settle_time,1,10)=? "
        ") ORDER BY t DESC LIMIT ?"));
    q.addBindValue(todayStr());
    q.addBindValue(todayStr());
    q.addBindValue(todayStr());
    q.addBindValue(limit);
    if (!q.exec()) return out;
    while (q.next()) {
        const int     orderId = q.value(0).toInt();
        const QString et      = q.value(1).toString();
        const QString t       = q.value(2).toString();
        const QString phone   = q.value(3).toString();
        const QString station = q.value(4).toString();
        const QString code    = q.value(5).toString();

        const QString masked = phone.size() == 11
            ? phone.left(3) + QStringLiteral("****") + phone.right(4)
            : QStringLiteral("用户");

        QString text;
        if (et == QStringLiteral("order_created"))
            text = QStringLiteral("用户 %1 在 %2 %3 预约充电").arg(masked, station, code);
        else if (et == QStringLiteral("order_started"))
            text = QStringLiteral("用户 %1 在 %2 %3 开始充电").arg(masked, station, code);
        else
            text = QStringLiteral("用户 %1 在 %2 完成充电并结算").arg(masked, station);

        out.append(EventRow{QStringLiteral("evt-o%1-%2").arg(orderId).arg(et), t, et, text});
    }
    return out;
}

}  // namespace dao
