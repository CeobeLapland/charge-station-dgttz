#include "ml/RecommendationEngine.h"

#include <algorithm>
#include <cmath>

#include "ml/MlDataProvider.h"

namespace ml {

static constexpr double DISTANCE_DECAY_KM = 5.0; // d0
static constexpr double PRICE_ALPHA = 2.0;

RecommendationEngine::RecommendationEngine(MlDataProvider* provider)
    : m_provider(provider)
{
}

RecommendationEngine::Weights RecommendationEngine::getWeights(const QString& category) const
{
    // §4.2.2 权重配置
    if (category == QLatin1String("fastest"))  return {0.20, 0.10, 0.50, 0.20};
    if (category == QLatin1String("cheapest")) return {0.15, 0.55, 0.15, 0.15};
    return {0.30, 0.30, 0.20, 0.20}; // balanced
}

static double distanceKm(double lon1, double lat1, double lon2, double lat2)
{
    const double dx = (lon1 - lon2) * 95.0;  // 纬度 39.98° 附近 1° 经度 ≈ 85km，取保守值
    const double dy = (lat1 - lat2) * 111.0;
    return std::sqrt(dx * dx + dy * dy);
}

QList<StationScore> RecommendationEngine::rankStations(double userLongitude,
                                                       double userLatitude,
                                                       const QString& category,
                                                       const QString& weather)
{
    const Weights w = getWeights(category);
    QList<StationScore> results;

    for (const auto& station : m_provider->stations()) {
        const QList<ChargerInfo> stationChargers = m_provider->chargers(station.id);
        int idle = 0;
        double healthSum = 0.0;
        for (const auto& c : stationChargers) {
            if (c.status == QLatin1String("idle"))
                idle++;
            healthSum += c.healthScore;
        }
        const int total = stationChargers.isEmpty() ? station.totalChargers
                                                    : stationChargers.size();
        // §13.2 兜底：电桩数据缺失时用温和默认值，保证排序仍可用
        const double idleRate = total > 0 ? static_cast<double>(idle) / total : 0.5;
        const double avgHealth = !stationChargers.isEmpty()
                                     ? healthSum / stationChargers.size() / 100.0 : 0.8;

        // §4.2.1 特征函数
        const double fd = std::exp(-distanceKm(userLongitude, userLatitude,
                                               station.longitude, station.latitude)
                                   / DISTANCE_DECAY_KM);
        const double fp = 1.0 / (1.0 + PRICE_ALPHA * station.serviceFee);
        const double fw = idleRate;
        const double fh = avgHealth;

        double score = w.d * fd + w.p * fp + w.w * fw + w.h * fh;

        // §4.2.3 天气修正
        if (weather == QLatin1String("rain") && station.isSheltered())
            score *= 1.15;
        else if (weather == QLatin1String("extreme") && !station.isSheltered())
            score *= 0.50;

        StationScore s;
        s.stationId = station.id;
        s.stationName = station.name;
        s.score = score;
        s.category = category;
        s.featureBreakdown[QStringLiteral("distance")] = fd;
        s.featureBreakdown[QStringLiteral("price")] = fp;
        s.featureBreakdown[QStringLiteral("wait")] = fw;
        s.featureBreakdown[QStringLiteral("health")] = fh;
        results.append(s);
    }

    std::sort(results.begin(), results.end(),
              [](const StationScore& a, const StationScore& b) {
                  return a.score > b.score;
              });
    return results;
}

} // namespace ml
