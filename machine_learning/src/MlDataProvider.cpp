#include "ml/MlDataProvider.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

namespace ml {

static const QString ML_CONN_NAME = QStringLiteral("ml_engine");

static QSqlDatabase mlDb()
{
    return QSqlDatabase::database(ML_CONN_NAME);
}

bool MlDataProvider::open(const QString& path)
{
    if (QSqlDatabase::contains(ML_CONN_NAME))
        QSqlDatabase::removeDatabase(ML_CONN_NAME);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), ML_CONN_NAME);
    db.setDatabaseName(path);
    if (!db.open()) {
        m_lastError = db.lastError().text();
        return false;
    }
    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
    m_lastError.clear();
    return true;
}

void MlDataProvider::close()
{
    {
        QSqlDatabase db = mlDb();
        if (db.isValid() && db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(ML_CONN_NAME);
}

bool MlDataProvider::isOpen() const
{
    return mlDb().isValid() && mlDb().isOpen();
}

QString MlDataProvider::lastError() const
{
    return m_lastError;
}

QList<StationInfo> MlDataProvider::stations()
{
    QList<StationInfo> out;
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT id, name, area, longitude, latitude, service_fee, facilities,"
        " total_chargers FROM station WHERE status='active' ORDER BY id"));
    if (!q.exec()) return out;
    while (q.next()) {
        StationInfo s;
        s.id = q.value(0).toInt();
        s.name = q.value(1).toString();
        s.area = q.value(2).toString();
        s.longitude = q.value(3).toDouble();
        s.latitude = q.value(4).toDouble();
        s.serviceFee = q.value(5).toDouble();
        s.facilitiesJson = q.value(6).toString();
        s.totalChargers = q.value(7).toInt();
        out.append(s);
    }
    return out;
}

QList<ChargerInfo> MlDataProvider::chargers(int stationId)
{
    QList<ChargerInfo> out;
    QSqlQuery q(mlDb());
    if (stationId > 0) {
        q.prepare(QStringLiteral(
            "SELECT id, station_id, code, type, power, status, temperature,"
            " comm_status, health_score FROM charger WHERE station_id=? ORDER BY id"));
        q.addBindValue(stationId);
    } else {
        q.prepare(QStringLiteral(
            "SELECT id, station_id, code, type, power, status, temperature,"
            " comm_status, health_score FROM charger ORDER BY id"));
    }
    if (!q.exec()) return out;
    while (q.next()) {
        ChargerInfo c;
        c.id = q.value(0).toInt();
        c.stationId = q.value(1).toInt();
        c.code = q.value(2).toString();
        c.type = q.value(3).toString();
        c.powerKw = q.value(4).toDouble();
        c.status = q.value(5).toString();
        c.temperature = q.value(6).toDouble();
        c.commStatus = q.value(7).toString();
        c.healthScore = q.value(8).toInt();
        out.append(c);
    }
    return out;
}

ChargerInfo MlDataProvider::charger(int chargerId)
{
    for (const auto& c : chargers()) {
        if (c.id == chargerId)
            return c;
    }
    return {};
}

bool MlDataProvider::updateChargerHealth(int chargerId, int healthScore)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral("UPDATE charger SET health_score=? WHERE id=?"));
    q.addBindValue(healthScore);
    q.addBindValue(chargerId);
    return q.exec();
}

QList<LoadPoint> MlDataProvider::stationHourlyLoad(int stationId, const QDateTime& since)
{
    QList<LoadPoint> out;
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT measure_time, SUM(power_kw) FROM charging_measure"
        " WHERE station_id=? AND measure_time>=?"
        " GROUP BY substr(measure_time,1,13) ORDER BY measure_time"));
    q.addBindValue(stationId);
    q.addBindValue(since.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (!q.exec()) return out;
    while (q.next()) {
        LoadPoint p;
        p.time = QDateTime::fromString(q.value(0).toString(),
                                       QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        p.power = q.value(1).toDouble();
        out.append(p);
    }
    return out;
}

QList<double> MlDataProvider::chargerHourlyPower(int chargerId, const QDateTime& since)
{
    QList<double> out;
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT measure_time, AVG(power_kw) FROM charging_measure"
        " WHERE charger_id=? AND measure_time>=?"
        " GROUP BY substr(measure_time,1,13) ORDER BY measure_time"));
    q.addBindValue(chargerId);
    q.addBindValue(since.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (!q.exec()) return out;
    while (q.next())
        out.append(q.value(1).toDouble());
    return out;
}

double MlDataProvider::currentStationLoad(int stationId)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT SUM(power_kw) FROM charging_measure WHERE station_id=?"
        " AND measure_time>=datetime('now','localtime','-1 hour')"));
    q.addBindValue(stationId);
    if (!q.exec() || !q.next())
        return 0.0;
    return q.value(0).toDouble();
}

int MlDataProvider::countHistoryDays(int stationId)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT COUNT(DISTINCT substr(measure_time,1,10)) FROM charging_measure"
        " WHERE station_id=?"));
    q.addBindValue(stationId);
    if (!q.exec() || !q.next())
        return 0;
    return q.value(0).toInt();
}

static QList<OrderBrief> readOrders(QSqlQuery& q)
{
    QList<OrderBrief> out;
    const QString fmt = QStringLiteral("yyyy-MM-dd HH:mm:ss");
    while (q.next()) {
        OrderBrief o;
        o.orderId = q.value(0).toInt();
        o.userId = q.value(1).toInt();
        o.stationId = q.value(2).toInt();
        o.chargerId = q.value(3).toInt();
        o.status = q.value(4).toString();
        o.startTime = QDateTime::fromString(q.value(5).toString(), fmt);
        o.durationMin = q.value(6).toInt();
        o.energyKwh = q.value(7).toDouble();
        o.payAmount = q.value(8).toDouble();
        o.createTime = QDateTime::fromString(q.value(9).toString(), fmt);
        out.append(o);
    }
    return out;
}

QList<OrderBrief> MlDataProvider::orders(int stationId, const QDateTime& since)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT id, user_id, station_id, charger_id, status, start_time,"
        " duration_min, energy_kwh, pay_amount, create_time FROM charging_order"
        " WHERE station_id=? AND start_time>=? ORDER BY start_time"));
    q.addBindValue(stationId);
    q.addBindValue(since.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (!q.exec()) return {};
    return readOrders(q);
}

QList<OrderBrief> MlDataProvider::allOrders(const QDateTime& since)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT id, user_id, station_id, charger_id, status, start_time,"
        " duration_min, energy_kwh, pay_amount, create_time FROM charging_order"
        " WHERE start_time>=? ORDER BY start_time"));
    q.addBindValue(since.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (!q.exec()) return {};
    return readOrders(q);
}

double MlDataProvider::avgChargeDurationMin(int stationId)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT AVG(duration_min) FROM charging_order"
        " WHERE station_id=? AND duration_min>0"));
    q.addBindValue(stationId);
    if (!q.exec() || !q.next())
        return 0.0;
    return q.value(0).toDouble();
}

double MlDataProvider::avgOrderEnergy(int stationId)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT AVG(energy_kwh) FROM charging_order"
        " WHERE station_id=? AND energy_kwh>0"));
    q.addBindValue(stationId);
    if (!q.exec() || !q.next())
        return 0.0;
    return q.value(0).toDouble();
}

double MlDataProvider::avgOrderPayAmount(int stationId)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT AVG(pay_amount) FROM charging_order"
        " WHERE station_id=? AND pay_amount>0"));
    q.addBindValue(stationId);
    if (!q.exec() || !q.next())
        return 0.0;
    return q.value(0).toDouble();
}

double MlDataProvider::avgDailyOrders(int stationId, int days)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM charging_order WHERE station_id=?"
        " AND start_time>=datetime('now','localtime',?)"));
    q.addBindValue(stationId);
    q.addBindValue(QStringLiteral("-%1 days").arg(days));
    if (!q.exec() || !q.next())
        return 0.0;
    return q.value(0).toDouble() / days;
}

int MlDataProvider::peakHourlyOrderCount(int stationId, const QDateTime& since)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM charging_order WHERE station_id=? AND start_time>=?"
        " GROUP BY substr(start_time,1,13) ORDER BY COUNT(*) DESC LIMIT 1"));
    q.addBindValue(stationId);
    q.addBindValue(since.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (!q.exec() || !q.next())
        return 0;
    return q.value(0).toInt();
}

int MlDataProvider::waitingCount(int stationId)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM reservation WHERE station_id=? AND status='waiting'"));
    q.addBindValue(stationId);
    if (!q.exec() || !q.next())
        return 0;
    return q.value(0).toInt();
}

int MlDataProvider::reservationCountSince(int userId, const QDateTime& since)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM reservation WHERE user_id=? AND reserve_time>=?"));
    q.addBindValue(userId);
    q.addBindValue(since.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (!q.exec() || !q.next())
        return 0;
    return q.value(0).toInt();
}

int MlDataProvider::cancelledReservationCountSince(int userId, const QDateTime& since)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM reservation WHERE user_id=? AND reserve_time>=?"
        " AND status='cancelled'"));
    q.addBindValue(userId);
    q.addBindValue(since.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (!q.exec() || !q.next())
        return 0;
    return q.value(0).toInt();
}

QString MlDataProvider::weatherCondition(const QString& area)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT condition FROM weather WHERE area=? ORDER BY update_time DESC LIMIT 1"));
    q.addBindValue(area);
    if (q.exec() && q.next())
        return q.value(0).toString();
    // 回退到全局天气行
    QSqlQuery g(mlDb());
    g.prepare(QStringLiteral(
        "SELECT condition FROM weather WHERE area='' ORDER BY update_time DESC LIMIT 1"));
    if (g.exec() && g.next())
        return g.value(0).toString();
    return QStringLiteral("sunny");
}

int MlDataProvider::alarmCount(int chargerId, int days)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM alarm WHERE charger_id=?"
        " AND occur_time>=datetime('now','localtime',?)"));
    q.addBindValue(chargerId);
    q.addBindValue(QStringLiteral("-%1 days").arg(days));
    if (!q.exec() || !q.next())
        return 0;
    return q.value(0).toInt();
}

int MlDataProvider::faultChargerCount()
{
    QSqlQuery q(mlDb());
    q.exec(QStringLiteral("SELECT COUNT(*) FROM charger WHERE status='fault'"));
    if (!q.next())
        return 0;
    return q.value(0).toInt();
}

int MlDataProvider::newUserCount(const QDate& date)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM user WHERE substr(register_time,1,10)=?"));
    q.addBindValue(date.toString(QStringLiteral("yyyy-MM-dd")));
    if (!q.exec() || !q.next())
        return 0;
    return q.value(0).toInt();
}

int MlDataProvider::totalUserCount()
{
    QSqlQuery q(mlDb());
    q.exec(QStringLiteral("SELECT COUNT(*) FROM user"));
    if (!q.next())
        return 0;
    return q.value(0).toInt();
}

QList<int> MlDataProvider::userIds()
{
    QList<int> out;
    QSqlQuery q(mlDb());
    q.exec(QStringLiteral("SELECT id FROM user ORDER BY id"));
    while (q.next())
        out.append(q.value(0).toInt());
    return out;
}

double MlDataProvider::revenueOn(const QDate& date)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT IFNULL(SUM(pay_amount),0) FROM charging_order"
        " WHERE substr(start_time,1,10)=? AND status='completed'"));
    q.addBindValue(date.toString(QStringLiteral("yyyy-MM-dd")));
    if (!q.exec() || !q.next())
        return 0.0;
    return q.value(0).toDouble();
}

int MlDataProvider::busiestStationToday(int* orderCount, QString* stationName)
{
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT o.station_id, s.name, COUNT(*) FROM charging_order o"
        " JOIN station s ON s.id=o.station_id"
        " WHERE substr(o.start_time,1,10)=date('now','localtime')"
        " GROUP BY o.station_id ORDER BY COUNT(*) DESC LIMIT 1"));
    if (!q.exec() || !q.next())
        return 0;
    if (orderCount) *orderCount = q.value(2).toInt();
    if (stationName) *stationName = q.value(1).toString();
    return q.value(0).toInt();
}

QStringList MlDataProvider::reviewTagsSince(int stationId, const QDate& since)
{
    QStringList out;
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT tags FROM review WHERE station_id=? AND status='normal'"
        " AND substr(create_time,1,10)>=?"));
    q.addBindValue(stationId);
    q.addBindValue(since.toString(QStringLiteral("yyyy-MM-dd")));
    if (!q.exec()) return out;
    while (q.next()) {
        const QJsonArray arr = QJsonDocument::fromJson(q.value(0).toString().toUtf8()).array();
        for (const auto& v : arr)
            out.append(v.toString());
    }
    return out;
}

QMap<QString, double> MlDataProvider::reviewDimAverages(int stationId, const QDate& since)
{
    QMap<QString, double> out;
    QSqlQuery q(mlDb());
    q.prepare(QStringLiteral(
        "SELECT AVG(speed_score), AVG(device_score), AVG(parking_score),"
        " AVG(hygiene_score), AVG(service_score), COUNT(*)"
        " FROM review WHERE station_id=? AND status='normal'"
        " AND substr(create_time,1,10)>=?"));
    q.addBindValue(stationId);
    q.addBindValue(since.toString(QStringLiteral("yyyy-MM-dd")));
    if (!q.exec() || !q.next() || q.value(5).toInt() == 0)
        return out;
    out[QStringLiteral("speed")] = q.value(0).toDouble();
    out[QStringLiteral("device")] = q.value(1).toDouble();
    out[QStringLiteral("parking")] = q.value(2).toDouble();
    out[QStringLiteral("hygiene")] = q.value(3).toDouble();
    out[QStringLiteral("service")] = q.value(4).toDouble();
    return out;
}

} // namespace ml
