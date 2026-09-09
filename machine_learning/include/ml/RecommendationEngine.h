#pragma once

#include "ml/MlTypes.h"

namespace ml {

class MlDataProvider;

// §4 智能推荐引擎：多特征加权评分（balanced / fastest / cheapest）+ 天气修正
class RecommendationEngine
{
public:
    explicit RecommendationEngine(MlDataProvider* provider);

    QList<StationScore> rankStations(double userLongitude, double userLatitude,
                                     const QString& category, const QString& weather);

private:
    struct Weights { double d, p, w, h; };
    Weights getWeights(const QString& category) const;

    MlDataProvider* m_provider;
};

} // namespace ml
