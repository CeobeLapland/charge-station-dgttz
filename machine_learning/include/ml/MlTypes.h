#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

namespace ml {

// 星期索引（0=周一）到中文名
inline QString dayOfWeekName(int dow)
{
    static const char* const NAMES[] = {"周一", "周二", "周三", "周四", "周五", "周六", "周日"};
    return (dow >= 0 && dow < 7) ? QString::fromUtf8(NAMES[dow]) : QString();
}

// 历史/预测负荷点（小时级）
struct LoadPoint {
    QDateTime time;
    double power = 0.0;
};

// 充电订单摘要（ML 各算法共用）
struct OrderBrief {
    int orderId = 0;
    int userId = 0;
    int stationId = 0;
    int chargerId = 0;
    QString status;
    QDateTime startTime;
    int durationMin = 0;
    double energyKwh = 0.0;
    double payAmount = 0.0;
    QDateTime createTime;
};

struct StationInfo {
    int id = 0;
    QString name;
    QString area;
    double longitude = 0.0;
    double latitude = 0.0;
    double serviceFee = 0.0;
    QString facilitiesJson;
    int totalChargers = 0;

    bool hasFacility(const QString& name) const
    {
        const QJsonArray arr = QJsonDocument::fromJson(facilitiesJson.toUtf8()).array();
        return arr.contains(QJsonValue(name));
    }
    // 室内/有雨棚：雨天加分、极端天气不减分的口径
    bool isSheltered() const { return hasFacility(QStringLiteral("rain_shelter"))
                                      || hasFacility(QStringLiteral("underground_parking")); }
};

struct ChargerInfo {
    int id = 0;
    int stationId = 0;
    QString code;
    QString type;
    double powerKw = 0.0;
    QString status;
    double temperature = 0.0;
    QString commStatus;
    int healthScore = 100;
};

// §2 负荷预测输出
struct LoadForecast {
    int stationId = 0;
    int horizon = 0;
    QVector<QDateTime> times;
    QVector<double> predicted;
    QVector<double> upper;
    QVector<double> lower;
    QDateTime generatedAt;
    bool fallback = false;
};

// §3 设备健康度与故障风险
struct ChargerHealth {
    int chargerId = 0;
    int stationId = 0;
    int healthScore = 100;
    double faultRisk = 0.0;
    QString riskLevel;
    QMap<QString, double> penaltyBreakdown;
};

// §4 推荐评分
struct StationScore {
    int stationId = 0;
    QString stationName;
    double score = 0.0;
    QString category;
    QMap<QString, double> featureBreakdown;
    bool fallback = false;
};

// §5 等待时间预估
struct WaitEstimate {
    int stationId = 0;
    int queueLength = 0;
    int currentMinutes = 0;
    int after10Minutes = 0;
    int after20Minutes = 0;
    double avgRemainingMinutes = 0.0;
    QString suggestion;
    bool fallback = false;
};

// §7 需求热力图
struct DemandHeatCell {
    int dow = 0;
    int hour = 0;
    QString area;
    double demand = 0.0;
    bool surge = false;
    double delta = 0.0;
};

struct SurgeWindow {
    QString window;
    double demand = 0.0;
    QString recommendation;
};

struct DemandForecast {
    QVector<DemandHeatCell> heatmap;
    QVector<SurgeWindow> topSurgeWindows;
};

// §8 调度决策
struct DispatchDecision {
    int stationId = 0;
    bool triggered = false;
    QStringList triggerReasons;
    QStringList actions;
    double idleRate = 0.0;
    int queueLength = 0;
    double forecastLoadKw = 0.0;
    double ratedCapacityKw = 0.0;
};

// §9 用户风控
struct UserRisk {
    int userId = 0;
    bool rule1HighFreqCancel = false;
    bool rule2ShortCharge = false;
    double riskScore = 0.0;
    QString level;
    QString action;
    QStringList hitRules;
};

// §10 what-if 推演结果
struct WhatIfImpact {
    int stationId = 0;
    int addChargers = 0;
    double priceDelta = 0.0;
    double failureScale = 0.0;
    double trafficDelta = 0.0;
    double avgWaitMin = 0.0;
    double peakUtilization = 0.0;
    double dailyOrders = 0.0;
    double dailyRevenue = 0.0;
    bool fallback = false;
};

// §11 评价标签分析
struct ReviewTagStat {
    QString tag;
    int freq = 0;
    double mom = 0.0;
};

struct ReviewSummary {
    int stationId = 0;
    QVector<ReviewTagStat> topTags;
    QMap<QString, double> dimAverages;
};

// §12 运营助手
struct AssistantReply {
    QString answer;
    QString intent;
    bool fallback = false;
};

} // namespace ml
