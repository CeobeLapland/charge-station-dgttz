#pragma once

#include "ml/MlTypes.h"

namespace ml {

class MlDataProvider;

// §7 用户需求预测：星期×小时×区域聚合 + 需求激增检测
class DemandForecastEngine
{
public:
    explicit DemandForecastEngine(MlDataProvider* provider);

    DemandForecast forecast(const QDateTime& now);

private:
    MlDataProvider* m_provider;
};

} // namespace ml
