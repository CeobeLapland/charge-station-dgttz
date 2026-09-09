#include "ml/WaitTimeEngine.h"

#include <algorithm>

#include "ml/LoadForecastingEngine.h"
#include "ml/MlDataProvider.h"

namespace ml {

static constexpr double DEFAULT_REMAIN_MIN = 20.0; // 无历史时的默认剩余时长
static constexpr double FALLBACK_PER_QUEUE_MIN = 15.0; // §13.2 兜底：每队 15 分钟

WaitTimeEngine::WaitTimeEngine(MlDataProvider* provider,
                               LoadForecastingEngine* forecastEngine)
    : m_provider(provider)
    , m_forecast(forecastEngine)
{
}

double WaitTimeEngine::computeAvgRemaining(int stationId, const QDateTime& now)
{
    // 最近 7 天同小时时段的已完成订单，平均剩余时长 ≈ 平均充电时长 / 2
    const auto history = m_provider->orders(stationId, now.addDays(-7));
    double sum = 0.0;
    int cnt = 0;
    for (const auto& o : history) {
        if (o.status == QLatin1String("completed") && o.startTime.isValid()
                && o.startTime.time().hour() == now.time().hour()) {
            sum += o.durationMin / 2.0;
            cnt++;
        }
    }
    return cnt > 0 ? sum / cnt : DEFAULT_REMAIN_MIN;
}

WaitEstimate WaitTimeEngine::estimate(int stationId, const QDateTime& now)
{
    WaitEstimate result;
    result.stationId = stationId;
    result.queueLength = m_provider->waitingCount(stationId);

    double avgRemain = computeAvgRemaining(stationId, now);
    result.avgRemainingMinutes = avgRemain;
    result.fallback = avgRemain == DEFAULT_REMAIN_MIN;

    int current;
    if (result.queueLength == 0) {
        current = 0;
    } else if (m_provider->chargers(stationId).isEmpty()) {
        // §13.2 兜底：无电桩数据时按每队 15 分钟估算
        current = static_cast<int>(result.queueLength * FALLBACK_PER_QUEUE_MIN);
        result.fallback = true;
    } else {
        int idle = 0;
        for (const auto& c : m_provider->chargers(stationId)) {
            if (c.status == QLatin1String("idle"))
                idle++;
        }
        // §5.4：当前空闲桩 > 0 时无需等待
        current = idle > 0 ? 0
                           : static_cast<int>(result.queueLength * avgRemain);
    }
    result.currentMinutes = current;

    // 未来等待：负荷预测外推 + 衰减因子 η(10)=0.9 / η(20)=0.8
    const double currentLoad = m_provider->currentStationLoad(stationId);
    const auto forecast = m_forecast->predict(stationId, 1, now);
    const double forecastLoad = forecast.predicted.isEmpty()
                                    ? currentLoad : forecast.predicted.first();
    const double loadRatio = currentLoad > 0.001 ? forecastLoad / currentLoad : 1.0;
    result.after10Minutes = static_cast<int>(current * loadRatio * 0.9);
    result.after20Minutes = static_cast<int>(current * loadRatio * 0.8);

    if (current <= 5)       result.suggestion = QStringLiteral("建议现在前往");
    else if (current <= 15) result.suggestion = QStringLiteral("建议 10 分钟后前往");
    else if (current <= 30) result.suggestion = QStringLiteral("建议 20 分钟后前往");
    else                    result.suggestion = QStringLiteral("建议更换充电站");
    return result;
}

} // namespace ml
