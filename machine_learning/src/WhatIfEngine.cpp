#include "ml/WhatIfEngine.h"

#include <algorithm>
#include <cmath>

#include "ml/MlDataProvider.h"

namespace ml {

// §10.1 参数合法范围
static constexpr double PRICE_DELTA_MIN = -0.5, PRICE_DELTA_MAX = 0.5;
static constexpr double FAILURE_MIN = 0.0, FAILURE_MAX = 0.5;
static constexpr double TRAFFIC_MIN = -0.3, TRAFFIC_MAX = 0.3;

WhatIfEngine::WhatIfEngine(MlDataProvider* provider)
    : m_provider(provider)
{
}

WhatIfImpact WhatIfEngine::simulate(int stationId, int addChargers, double priceDelta,
                                    double failureScale, double trafficDelta)
{
    WhatIfImpact result;
    result.stationId = stationId;
    result.fallback = false;

    // 参数 clip 到文档给定范围
    result.priceDelta = std::clamp(priceDelta, PRICE_DELTA_MIN, PRICE_DELTA_MAX);
    result.failureScale = std::clamp(failureScale, FAILURE_MIN, FAILURE_MAX);
    result.trafficDelta = std::clamp(trafficDelta, TRAFFIC_MIN, TRAFFIC_MAX);

    const int totalChargers = m_provider->chargers(stationId).size();
    // 增配桩数量下限：保证分母 N_total + add ≥ 1
    result.addChargers = std::max(addChargers, -(totalChargers - 1));

    const double queue = m_provider->waitingCount(stationId);
    // 历史同期平均剩余时长 ≈ 平均充电时长 / 2
    const double avgRemain = m_provider->avgChargeDurationMin(stationId) / 2.0;
    const double capacity = totalChargers + result.addChargers;
    const double dPeak = m_provider->peakHourlyOrderCount(stationId,
                                                          QDateTime::currentDateTime().addDays(-14));
    const double avgDailyOrders = m_provider->avgDailyOrders(stationId, 14);
    const double avgEnergy = m_provider->avgOrderEnergy(stationId);
    const double avgPricePerKwh = [&] {
        const double e = m_provider->avgOrderEnergy(stationId);
        const double pay = m_provider->avgOrderPayAmount(stationId);
        return e > 0.001 ? pay / e : 1.2;
    }();

    if (totalChargers <= 0 || avgDailyOrders <= 0.0) {
        // §13：无数据时给出中性推演并标记兜底
        result.avgWaitMin = 0.0;
        result.peakUtilization = 0.0;
        result.dailyOrders = 0.0;
        result.dailyRevenue = 0.0;
        result.fallback = true;
        return result;
    }

    // §10.2 推演模型
    result.avgWaitMin = queue * avgRemain / capacity
                            * (1.0 + result.failureScale)
                            * (1.0 - 0.3 * result.priceDelta);
    result.peakUtilization = dPeak * (1.0 + result.trafficDelta) / capacity
                                 / (1.0 - result.failureScale);
    result.dailyOrders = avgDailyOrders * (1.0 + result.trafficDelta)
                             * (1.0 - 0.2 * result.priceDelta);
    result.dailyRevenue = result.dailyOrders
                              * (avgPricePerKwh + result.priceDelta) * avgEnergy;
    return result;
}

} // namespace ml
