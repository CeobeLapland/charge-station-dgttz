#pragma once

#include <random>

#include "ml/MlTypes.h"

namespace ml {

class MlDataProvider;

// §2 充电负荷智能预测：周期性移动平均外推（PME）+ §6 天气修正 + §13.2 兜底模板
class LoadForecastingEngine
{
public:
    explicit LoadForecastingEngine(MlDataProvider* provider);
    void setSeed(quint32 seed);

    LoadForecast predict(int stationId, int horizonHours, const QDateTime& startTime);

private:
    QVector<double> fallbackTemplate(int horizonHours, const QDateTime& startTime,
                                     LoadForecast* out);
    double weatherMultiplier(const StationInfo& station, const QString& weather) const;

    MlDataProvider* m_provider;
    std::mt19937 m_rng;
};

} // namespace ml
