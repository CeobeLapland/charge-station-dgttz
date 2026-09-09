#pragma once

#include "ml/MlTypes.h"

namespace ml {

class MlDataProvider;
class LoadForecastingEngine;

// §8 智能调度：触发条件判定 + 动作优先级 A4→A1→A3→A2
class DispatchEngine
{
public:
    DispatchEngine(MlDataProvider* provider, LoadForecastingEngine* forecastEngine);

    DispatchDecision checkStation(int stationId, const QDateTime& now);
    QList<DispatchDecision> checkAll(const QDateTime& now);

private:
    bool hasAlternativeStation(int stationId, const StationInfo& self);

    MlDataProvider* m_provider;
    LoadForecastingEngine* m_forecast;
};

} // namespace ml
