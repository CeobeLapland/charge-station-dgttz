#include "ml/DispatchEngine.h"

#include <cmath>

#include "ml/LoadForecastingEngine.h"
#include "ml/MlDataProvider.h"

namespace ml {

static constexpr double IDLE_RATE_THRESHOLD = 0.2;   // 空闲率 < 20%
static constexpr int QUEUE_THRESHOLD = 5;            // 排队 > 5 人
static constexpr double LOAD_RATIO_THRESHOLD = 0.9;  // 预测负荷 > 额定 90%

DispatchEngine::DispatchEngine(MlDataProvider* provider,
                               LoadForecastingEngine* forecastEngine)
    : m_provider(provider)
    , m_forecast(forecastEngine)
{
}

bool DispatchEngine::hasAlternativeStation(int stationId, const StationInfo& self)
{
    // A1 目标：附近 3km 内空闲率 > 50% 的站
    for (const auto& other : m_provider->stations()) {
        if (other.id == stationId)
            continue;
        const double dx = (other.longitude - self.longitude) * 95.0;
        const double dy = (other.latitude - self.latitude) * 111.0;
        if (std::sqrt(dx * dx + dy * dy) > 3.0)
            continue;
        const auto cs = m_provider->chargers(other.id);
        if (cs.isEmpty())
            continue;
        int idle = 0;
        for (const auto& c : cs) {
            if (c.status == QLatin1String("idle"))
                idle++;
        }
        if (static_cast<double>(idle) / cs.size() > 0.5)
            return true;
    }
    return false;
}

DispatchDecision DispatchEngine::checkStation(int stationId, const QDateTime& now)
{
    DispatchDecision result;
    result.stationId = stationId;

    StationInfo self;
    for (const auto& s : m_provider->stations()) {
        if (s.id == stationId) {
            self = s;
            break;
        }
    }

    const auto cs = m_provider->chargers(stationId);
    int idle = 0;
    double rated = 0.0;
    for (const auto& c : cs) {
        if (c.status == QLatin1String("idle"))
            idle++;
        rated += c.powerKw;
    }
    result.idleRate = cs.isEmpty() ? 1.0 : static_cast<double>(idle) / cs.size();
    result.ratedCapacityKw = rated;
    result.queueLength = m_provider->waitingCount(stationId);

    const auto forecast = m_forecast->predict(stationId, 1, now);
    result.forecastLoadKw = forecast.predicted.isEmpty() ? 0.0 : forecast.predicted.first();

    if (result.idleRate < IDLE_RATE_THRESHOLD)
        result.triggerReasons.append(QStringLiteral("idle_rate_below_20pct"));
    if (result.queueLength > QUEUE_THRESHOLD)
        result.triggerReasons.append(QStringLiteral("queue_over_5"));
    if (rated > 0.001 && result.forecastLoadKw > LOAD_RATIO_THRESHOLD * rated)
        result.triggerReasons.append(QStringLiteral("forecast_over_90pct_capacity"));

    result.triggered = !result.triggerReasons.isEmpty();
    if (!result.triggered)
        return result;

    // §8.3 动作优先级：A4 → A1 → A3 → A2
    result.actions.append(QStringLiteral("A4_trigger_operation_alarm")); // 必触发
    const bool a1 = hasAlternativeStation(stationId, self);
    if (a1)
        result.actions.append(QStringLiteral("A1_boost_alternative_station")); // 必触发
    result.actions.append(QStringLiteral("A3_double_points")); // 可选
    // A2：A1 效果不足（无替代站或排队严重）时发放定向优惠券
    if (!a1 || result.queueLength > 10)
        result.actions.append(QStringLiteral("A2_targeted_coupon"));
    return result;
}

QList<DispatchDecision> DispatchEngine::checkAll(const QDateTime& now)
{
    QList<DispatchDecision> out;
    for (const auto& s : m_provider->stations())
        out.append(checkStation(s.id, now));
    return out;
}

} // namespace ml
