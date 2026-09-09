#include "ml/LoadForecastingEngine.h"

#include <algorithm>
#include <cmath>

#include "ml/MlDataProvider.h"

namespace ml {

// 附录 A 可调参数（实际取值，见 README）
static constexpr int HISTORY_DAYS = 7;      // D ∈ [3,14]
static constexpr int SMOOTH_K = 1;          // k ∈ [0,3]
static constexpr double GROWTH_MIN = 0.0;   // g ∈ [0,0.1]
static constexpr double GROWTH_MAX = 0.1;
static constexpr double NOISE_SIGMA = 0.05; // σ_ε ∈ [0.01,0.1]
static constexpr double RESIDUAL_FLOOR = 0.02;
static constexpr double Z_90 = 1.645;       // 90% 置信区间
static constexpr double MIN_COVERAGE = 0.7; // 缺失率 >30% 走兜底（§13.3）

LoadForecastingEngine::LoadForecastingEngine(MlDataProvider* provider)
    : m_provider(provider)
    , m_rng(42) // 固定默认种子，保证 demo 可复现
{
}

void LoadForecastingEngine::setSeed(quint32 seed)
{
    m_rng.seed(seed);
}

double LoadForecastingEngine::weatherMultiplier(const StationInfo& station,
                                                const QString& weather) const
{
    // §6 修正系数表：室外站 / 室内（有雨棚或地下停车）站
    const bool sheltered = station.isSheltered();
    if (weather == QLatin1String("cloudy")) return sheltered ? 0.98 : 0.95;
    if (weather == QLatin1String("rain"))   return sheltered ? 1.18 : 0.89;
    if (weather == QLatin1String("hot"))    return 0.95;
    if (weather == QLatin1String("extreme")) return sheltered ? 0.75 : 0.60;
    return 1.0; // sunny
}

QVector<double> LoadForecastingEngine::fallbackTemplate(int horizonHours,
                                                        const QDateTime& startTime,
                                                        LoadForecast* out)
{
    // §13.2 兜底：固定日周期模板（早 8 晚 18 双峰）
    QVector<double> base;
    for (int i = 0; i < horizonHours; ++i) {
        const int h = startTime.addSecs(i * 3600).time().hour();
        const double morning = 25.0 * std::exp(-std::pow((h - 8) / 2.5, 2));
        const double evening = 35.0 * std::exp(-std::pow((h - 18) / 3.0, 2));
        base.append(20.0 + morning + evening);
        out->times.append(startTime.addSecs(i * 3600));
    }
    out->fallback = true;
    return base;
}

LoadForecast LoadForecastingEngine::predict(int stationId, int horizonHours,
                                            const QDateTime& startTime)
{
    if (horizonHours <= 1) horizonHours = 1;
    else if (horizonHours <= 6) horizonHours = 6;
    else horizonHours = 24;

    LoadForecast result;
    result.stationId = stationId;
    result.horizon = horizonHours;
    result.generatedAt = QDateTime::currentDateTime();

    const QDateTime windowStart = startTime.addDays(-HISTORY_DAYS);
    const QList<LoadPoint> history = m_provider->stationHourlyLoad(stationId, windowStart);

    StationInfo station;
    for (const auto& s : m_provider->stations()) {
        if (s.id == stationId) {
            station = s;
            break;
        }
    }

    QVector<double> base;
    double sigmaR = RESIDUAL_FLOOR;
    double growth = 0.0;
    double weatherGamma = 1.0;

    // §13.3 数据质量检查：历史不足 3 天或缺失率 >30% → 兜底模板
    const bool enoughData = m_provider->countHistoryDays(stationId) >= 3
        && history.size() >= static_cast<int>(HISTORY_DAYS * 24 * MIN_COVERAGE);
    if (!enoughData) {
        base = fallbackTemplate(horizonHours, startTime, &result);
        sigmaR = 0.08;
    } else {
        // 步骤 1：按"星期几 × 小时"分桶聚合
        double matrix[7][24] = {};
        int count[7][24] = {};
        for (const auto& p : history) {
            if (p.time >= startTime)
                continue;
            const int dow = p.time.date().dayOfWeek() - 1;
            const int hour = p.time.time().hour();
            matrix[dow][hour] += p.power;
            count[dow][hour]++;
        }
        double hourMean[24] = {};
        double totalMean = 0.0;
        int totalCnt = 0;
        for (int h = 0; h < 24; ++h) {
            double sum = 0.0;
            int cnt = 0;
            for (int d = 0; d < 7; ++d) {
                if (count[d][h] > 0) {
                    matrix[d][h] /= count[d][h];
                    sum += matrix[d][h];
                    cnt++;
                }
            }
            hourMean[h] = cnt > 0 ? sum / cnt : 0.0;
            totalMean += sum;
            totalCnt += cnt;
        }
        totalMean = totalCnt > 0 ? totalMean / totalCnt : 0.0;
        for (int d = 0; d < 7; ++d) {
            for (int h = 0; h < 24; ++h) {
                if (count[d][h] == 0)
                    matrix[d][h] = hourMean[h] > 0.001 ? hourMean[h] : totalMean;
            }
        }

        // 步骤 2：k=1 中心移动平均平滑
        double smoothed[7][24] = {};
        for (int d = 0; d < 7; ++d) {
            for (int h = 0; h < 24; ++h) {
                double sum = 0.0;
                for (int i = -SMOOTH_K; i <= SMOOTH_K; ++i)
                    sum += matrix[d][(h + i + 24) % 24];
                smoothed[d][h] = sum / (2 * SMOOTH_K + 1);
            }
        }

        // 历史预测残差标准差 σ_r（相对残差）
        QVector<double> residuals;
        for (const auto& p : history) {
            if (p.time >= startTime)
                continue;
            const int dow = p.time.date().dayOfWeek() - 1;
            const int hour = p.time.time().hour();
            const double ref = smoothed[dow][hour];
            if (ref > 0.5)
                residuals.append((p.power - ref) / ref);
        }
        if (residuals.size() >= 2) {
            double mean = 0.0;
            for (double r : residuals) mean += r;
            mean /= residuals.size();
            double var = 0.0;
            for (double r : residuals) var += (r - mean) * (r - mean);
            sigmaR = std::max(std::sqrt(var / residuals.size()), RESIDUAL_FLOOR);
        }

        // 步骤 3：日增长率 g（附录 A：clip 到 [0, 0.1]）
        double todaySum = 0.0, weekAgoSum = 0.0;
        for (int h = 0; h < 24; ++h) {
            todaySum += matrix[6][h];
            weekAgoSum += matrix[0][h];
        }
        if (weekAgoSum > 0.001)
            growth = std::clamp((todaySum - weekAgoSum) / (7.0 * weekAgoSum),
                                GROWTH_MIN, GROWTH_MAX);

        // §6 天气修正
        weatherGamma = weatherMultiplier(station, m_provider->weatherCondition(station.area));

        // 步骤 4：趋势外推
        const int startDow = startTime.date().dayOfWeek() - 1;
        const int startHour = startTime.time().hour();
        std::normal_distribution<double> noise(1.0, NOISE_SIGMA);
        for (int i = 0; i < horizonHours; ++i) {
            const int targetDow = (startDow + (startHour + i) / 24) % 7;
            const int targetHour = (startHour + i) % 24;
            const int dayOffset = (startHour + i) / 24;
            const double trend = std::pow(1.0 + growth, dayOffset);
            const double pred = smoothed[targetDow][targetHour] * trend
                                    * weatherGamma * noise(m_rng);
            base.append(pred);
            result.times.append(startTime.addSecs(i * 3600));
        }
    }

    // 置信区间：1.645·σ_r
    for (int i = 0; i < base.size(); ++i) {
        const double pred = std::max(0.0, base[i]);
        result.predicted.append(pred);
        result.upper.append(std::max(0.0, pred * (1.0 + Z_90 * sigmaR)));
        result.lower.append(std::max(0.0, pred * (1.0 - Z_90 * sigmaR)));
    }
    return result;
}

} // namespace ml
