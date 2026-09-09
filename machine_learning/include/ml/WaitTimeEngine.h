#pragma once

#include "ml/MlTypes.h"

namespace ml {

class MlDataProvider;
class LoadForecastingEngine;

// §5 等待时间预估：排队 × 历史同期平均剩余时长 + 负荷预测外推
class WaitTimeEngine
{
public:
    WaitTimeEngine(MlDataProvider* provider, LoadForecastingEngine* forecastEngine);

    WaitEstimate estimate(int stationId, const QDateTime& now);

private:
    double computeAvgRemaining(int stationId, const QDateTime& now);

    MlDataProvider* m_provider;
    LoadForecastingEngine* m_forecast;
};

} // namespace ml
