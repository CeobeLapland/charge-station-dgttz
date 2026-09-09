#include "ml/MlEngine.h"

#include <QJsonArray>

namespace ml {

static QJsonObject forecastPointToJson(const QDateTime& time, double predicted,
                                       double upper, double lower)
{
    QJsonObject o;
    o[QStringLiteral("time")] = time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    o[QStringLiteral("predicted")] = predicted;
    o[QStringLiteral("upper")] = upper;
    o[QStringLiteral("lower")] = lower;
    return o;
}

MlEngine::MlEngine()
    : m_load(&m_provider)
    , m_health(&m_provider)
    , m_recommend(&m_provider)
    , m_wait(&m_provider, &m_load)
    , m_demand(&m_provider)
    , m_dispatch(&m_provider, &m_load)
    , m_risk(&m_provider)
    , m_whatif(&m_provider)
    , m_review(&m_provider)
    , m_assistant(&m_provider, &m_load, &m_health)
{
}

bool MlEngine::openDatabase(const QString& path)
{
    return m_provider.open(path);
}

void MlEngine::closeDatabase()
{
    m_provider.close();
}

bool MlEngine::isOpen() const
{
    return m_provider.isOpen();
}

MlDataProvider* MlEngine::provider()
{
    return &m_provider;
}

QJsonObject MlEngine::forecast(int stationId, int horizonHours)
{
    QJsonObject out;
    if (!isOpen()) {
        out[QStringLiteral("code")] = 5001;
        return out;
    }
    QDateTime start = QDateTime::currentDateTime();
    start.setTime(QTime(start.time().hour(), 0, 0));

    const LoadForecast result = m_load.predict(stationId, horizonHours, start);

    QJsonArray history;
    const auto actual = m_provider.stationHourlyLoad(stationId, start.addDays(-1));
    for (const auto& p : actual) {
        QJsonObject o;
        o[QStringLiteral("time")] = p.time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        o[QStringLiteral("power")] = p.power;
        history.append(o);
    }

    QJsonArray forecastArr;
    for (int i = 0; i < result.predicted.size(); ++i) {
        forecastArr.append(forecastPointToJson(result.times.value(i),
                                               result.predicted[i],
                                               result.upper.value(i),
                                               result.lower.value(i)));
    }

    out[QStringLiteral("code")] = 0;
    out[QStringLiteral("station_id")] = stationId;
    out[QStringLiteral("horizon")] = result.horizon;
    out[QStringLiteral("generated_at")] = result.generatedAt.toString(
        QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    out[QStringLiteral("history")] = history;
    out[QStringLiteral("forecast")] = forecastArr;
    out[QStringLiteral("fallback")] = result.fallback;
    return out;
}

QJsonObject MlEngine::recommend(double longitude, double latitude,
                                const QString& category)
{
    QJsonObject out;
    if (!isOpen()) {
        out[QStringLiteral("code")] = 5001;
        return out;
    }
    const QString weather = m_provider.weatherCondition(QString());
    const auto ranks = m_recommend.rankStations(longitude, latitude, category, weather);

    QJsonArray arr;
    for (const auto& r : ranks) {
        QJsonObject o;
        o[QStringLiteral("station_id")] = r.stationId;
        o[QStringLiteral("station_name")] = r.stationName;
        o[QStringLiteral("score")] = r.score;
        QJsonObject breakdown;
        for (auto it = r.featureBreakdown.begin(); it != r.featureBreakdown.end(); ++it)
            breakdown[it.key()] = it.value();
        o[QStringLiteral("breakdown")] = breakdown;
        arr.append(o);
    }
    out[QStringLiteral("code")] = 0;
    out[QStringLiteral("category")] = category;
    out[QStringLiteral("weather")] = weather;
    out[QStringLiteral("recommendation")] = arr;
    return out;
}

QJsonObject MlEngine::waitEstimate(int stationId)
{
    QJsonObject out;
    if (!isOpen()) {
        out[QStringLiteral("code")] = 5001;
        return out;
    }
    const WaitEstimate w = m_wait.estimate(stationId, QDateTime::currentDateTime());
    out[QStringLiteral("code")] = 0;
    out[QStringLiteral("station_id")] = w.stationId;
    out[QStringLiteral("queue_length")] = w.queueLength;
    out[QStringLiteral("current_minutes")] = w.currentMinutes;
    out[QStringLiteral("after_10_minutes")] = w.after10Minutes;
    out[QStringLiteral("after_20_minutes")] = w.after20Minutes;
    out[QStringLiteral("avg_remaining_minutes")] = w.avgRemainingMinutes;
    out[QStringLiteral("suggestion")] = w.suggestion;
    out[QStringLiteral("fallback")] = w.fallback;
    return out;
}

QJsonObject MlEngine::faultRisk()
{
    QJsonObject out;
    if (!isOpen()) {
        out[QStringLiteral("code")] = 5001;
        return out;
    }
    QJsonArray arr;
    for (const auto& h : m_health.assessAll()) {
        QJsonObject o;
        o[QStringLiteral("charger_id")] = h.chargerId;
        o[QStringLiteral("station_id")] = h.stationId;
        o[QStringLiteral("health_score")] = h.healthScore;
        o[QStringLiteral("fault_risk")] = h.faultRisk;
        o[QStringLiteral("risk_level")] = h.riskLevel;
        QJsonObject penalties;
        for (auto it = h.penaltyBreakdown.begin(); it != h.penaltyBreakdown.end(); ++it)
            penalties[it.key()] = it.value();
        o[QStringLiteral("penalties")] = penalties;
        arr.append(o);
    }
    out[QStringLiteral("code")] = 0;
    out[QStringLiteral("risks")] = arr;
    return out;
}

QJsonObject MlEngine::whatIf(const QJsonObject& scenario)
{
    QJsonObject out;
    if (!isOpen()) {
        out[QStringLiteral("code")] = 5001;
        return out;
    }
    const int stationId = scenario.value(QStringLiteral("station_id")).toInt();
    const int addChargers = scenario.value(QStringLiteral("add_chargers")).toInt();
    const double priceDelta = scenario.value(QStringLiteral("price_delta")).toDouble();
    const double failureScale = scenario.value(QStringLiteral("failure_scale")).toDouble();
    const double trafficDelta = scenario.value(QStringLiteral("traffic_delta")).toDouble();

    const WhatIfImpact r = m_whatif.simulate(stationId, addChargers, priceDelta,
                                             failureScale, trafficDelta);
    QJsonObject impact;
    impact[QStringLiteral("avg_wait_min")] = r.avgWaitMin;
    impact[QStringLiteral("peak_utilization")] = r.peakUtilization;
    impact[QStringLiteral("daily_orders")] = r.dailyOrders;
    impact[QStringLiteral("daily_revenue")] = r.dailyRevenue;
    QJsonObject clipped;
    clipped[QStringLiteral("add_chargers")] = r.addChargers;
    clipped[QStringLiteral("price_delta")] = r.priceDelta;
    clipped[QStringLiteral("failure_scale")] = r.failureScale;
    clipped[QStringLiteral("traffic_delta")] = r.trafficDelta;
    out[QStringLiteral("code")] = 0;
    out[QStringLiteral("station_id")] = r.stationId;
    out[QStringLiteral("impact")] = impact;
    out[QStringLiteral("clipped_params")] = clipped;
    out[QStringLiteral("fallback")] = r.fallback;
    return out;
}

QJsonObject MlEngine::assistantQuery(const QString& question)
{
    QJsonObject out;
    if (!isOpen()) {
        out[QStringLiteral("code")] = 5001;
        return out;
    }
    const AssistantReply reply = m_assistant.answerQuery(question, QDateTime::currentDateTime());
    out[QStringLiteral("code")] = 0;
    out[QStringLiteral("answer")] = reply.answer;
    out[QStringLiteral("intent")] = reply.intent;
    out[QStringLiteral("fallback")] = reply.fallback;
    return out;
}

QJsonObject MlEngine::demandHeatmap()
{
    QJsonObject out;
    if (!isOpen()) {
        out[QStringLiteral("code")] = 5001;
        return out;
    }
    const DemandForecast result = m_demand.forecast(QDateTime::currentDateTime());
    QJsonArray heatmap;
    for (const auto& c : result.heatmap) {
        QJsonObject o;
        o[QStringLiteral("dow")] = c.dow;
        o[QStringLiteral("hour")] = c.hour;
        o[QStringLiteral("area")] = c.area;
        o[QStringLiteral("demand")] = c.demand;
        o[QStringLiteral("surge")] = c.surge;
        o[QStringLiteral("delta")] = c.delta;
        heatmap.append(o);
    }
    QJsonArray top;
    for (const auto& w : result.topSurgeWindows) {
        QJsonObject o;
        o[QStringLiteral("window")] = w.window;
        o[QStringLiteral("demand")] = w.demand;
        o[QStringLiteral("recommendation")] = w.recommendation;
        top.append(o);
    }
    out[QStringLiteral("code")] = 0;
    out[QStringLiteral("heatmap")] = heatmap;
    out[QStringLiteral("top_surge_windows")] = top;
    return out;
}

QJsonObject MlEngine::dispatchCheck()
{
    QJsonObject out;
    if (!isOpen()) {
        out[QStringLiteral("code")] = 5001;
        return out;
    }
    QJsonArray arr;
    for (const auto& d : m_dispatch.checkAll(QDateTime::currentDateTime())) {
        QJsonObject o;
        o[QStringLiteral("station_id")] = d.stationId;
        o[QStringLiteral("triggered")] = d.triggered;
        o[QStringLiteral("trigger_reasons")] = QJsonArray::fromStringList(d.triggerReasons);
        o[QStringLiteral("actions")] = QJsonArray::fromStringList(d.actions);
        o[QStringLiteral("idle_rate")] = d.idleRate;
        o[QStringLiteral("queue_length")] = d.queueLength;
        o[QStringLiteral("forecast_load_kw")] = d.forecastLoadKw;
        o[QStringLiteral("rated_capacity_kw")] = d.ratedCapacityKw;
        arr.append(o);
    }
    out[QStringLiteral("code")] = 0;
    out[QStringLiteral("decisions")] = arr;
    return out;
}

QJsonObject MlEngine::riskUsers()
{
    QJsonObject out;
    if (!isOpen()) {
        out[QStringLiteral("code")] = 5001;
        return out;
    }
    QJsonArray arr;
    for (const auto& r : m_risk.evaluateAll(QDateTime::currentDateTime())) {
        QJsonObject o;
        o[QStringLiteral("user_id")] = r.userId;
        o[QStringLiteral("risk_score")] = r.riskScore;
        o[QStringLiteral("level")] = r.level;
        o[QStringLiteral("hit_rules")] = QJsonArray::fromStringList(r.hitRules);
        o[QStringLiteral("action")] = r.action;
        arr.append(o);
    }
    out[QStringLiteral("code")] = 0;
    out[QStringLiteral("risk_users")] = arr;
    return out;
}

QJsonObject MlEngine::reviewTags(int stationId)
{
    QJsonObject out;
    if (!isOpen()) {
        out[QStringLiteral("code")] = 5001;
        return out;
    }
    const ReviewSummary s = m_review.analyze(stationId, QDate::currentDate());
    QJsonArray top;
    for (const auto& t : s.topTags) {
        QJsonObject o;
        o[QStringLiteral("tag")] = t.tag;
        o[QStringLiteral("freq")] = t.freq;
        o[QStringLiteral("mom")] = t.mom;
        top.append(o);
    }
    QJsonObject dims;
    for (auto it = s.dimAverages.begin(); it != s.dimAverages.end(); ++it)
        dims[it.key()] = it.value();
    out[QStringLiteral("code")] = 0;
    out[QStringLiteral("station_id")] = stationId;
    out[QStringLiteral("top_tags")] = top;
    out[QStringLiteral("dim_averages")] = dims;
    return out;
}

} // namespace ml
