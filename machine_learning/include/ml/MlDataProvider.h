#pragma once

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>

#include "ml/MlTypes.h"

namespace ml {

// 数据库读取层：只负责忠实读数，数据不足的兜底逻辑在引擎层。
// 使用具名连接 "ml_engine"，不与 server 的默认连接冲突。
class MlDataProvider
{
public:
    bool open(const QString& path);
    void close();
    bool isOpen() const;
    QString lastError() const;

    // 站点与电桩
    QList<StationInfo> stations();
    QList<ChargerInfo> chargers(int stationId = -1);
    ChargerInfo charger(int chargerId);
    bool updateChargerHealth(int chargerId, int healthScore);

    // charging_measure
    QList<LoadPoint> stationHourlyLoad(int stationId, const QDateTime& since);
    QList<double> chargerHourlyPower(int chargerId, const QDateTime& since);
    double currentStationLoad(int stationId);
    int countHistoryDays(int stationId);

    // charging_order
    QList<OrderBrief> orders(int stationId, const QDateTime& since);
    QList<OrderBrief> allOrders(const QDateTime& since);
    double avgChargeDurationMin(int stationId);
    double avgOrderEnergy(int stationId);
    double avgOrderPayAmount(int stationId);
    double avgDailyOrders(int stationId, int days);
    int peakHourlyOrderCount(int stationId, const QDateTime& since);

    // reservation
    int waitingCount(int stationId);
    int reservationCountSince(int userId, const QDateTime& since);
    int cancelledReservationCountSince(int userId, const QDateTime& since);

    // weather / alarm / user / 营收
    QString weatherCondition(const QString& area);
    int alarmCount(int chargerId, int days);
    int faultChargerCount();
    int newUserCount(const QDate& date);
    int totalUserCount();
    QList<int> userIds();
    double revenueOn(const QDate& date);
    int busiestStationToday(int* orderCount, QString* stationName);

    // review
    QStringList reviewTagsSince(int stationId, const QDate& since);
    QMap<QString, double> reviewDimAverages(int stationId, const QDate& since);

private:
    QString m_lastError;
};

} // namespace ml
