#include "ml/HealthAssessmentEngine.h"

#include <algorithm>
#include <cmath>

#include "ml/MlDataProvider.h"

namespace ml {

HealthAssessmentEngine::HealthAssessmentEngine(MlDataProvider* provider)
    : m_provider(provider)
    , m_rng(42)
{
}

void HealthAssessmentEngine::setSeed(quint32 seed)
{
    m_rng.seed(seed);
}

ChargerHealth HealthAssessmentEngine::assess(int chargerId)
{
    const ChargerInfo charger = m_provider->charger(chargerId);
    ChargerHealth result;
    result.chargerId = chargerId;
    result.stationId = charger.stationId;

    int score = 100;

    // §3.2.1(1) 温度惩罚（附录 A 阈值 50℃）
    double tempPenalty = 0.0;
    if (charger.temperature > 50.0) {
        if (charger.temperature <= 80.0)
            tempPenalty = 2.0 * (charger.temperature - 50.0);
        else
            tempPenalty = 60.0 + 5.0 * (charger.temperature - 80.0);
    }
    result.penaltyBreakdown[QStringLiteral("temperature")] = tempPenalty;
    score -= static_cast<int>(tempPenalty);

    // §3.2.1(2) 通信惩罚
    const double commPenalty = (charger.commStatus == QLatin1String("abnormal")) ? 15.0 : 0.0;
    result.penaltyBreakdown[QStringLiteral("communication")] = commPenalty;
    score -= static_cast<int>(commPenalty);

    // §3.2.1(3) 功率波动惩罚：20·min(CV,2)
    const QList<double> powers = m_provider->chargerHourlyPower(
        chargerId, QDateTime::currentDateTime().addDays(-1));
    double cv = 0.0;
    if (!powers.isEmpty()) {
        double sum = 0.0, sumSq = 0.0;
        for (double p : powers) {
            sum += p;
            sumSq += p * p;
        }
        const double n = static_cast<double>(powers.size());
        const double mean = sum / n;
        const double variance = std::max(0.0, sumSq / n - mean * mean);
        cv = mean > 0.001 ? std::sqrt(variance) / mean : 0.0;
    }
    const double powerPenalty = 20.0 * std::min(cv, 2.0);
    result.penaltyBreakdown[QStringLiteral("power_variation")] = powerPenalty;
    score -= static_cast<int>(powerPenalty);

    // §3.2.1(4) 历史异常惩罚：5×30 天内告警次数
    const double faultPenalty = 5.0 * m_provider->alarmCount(chargerId, 30);
    result.penaltyBreakdown[QStringLiteral("historical_faults")] = faultPenalty;
    score -= static_cast<int>(faultPenalty);

    score = std::clamp(score, 0, 100);
    result.healthScore = score;

    // §3.3 故障风险 R
    std::normal_distribution<double> jitter(0.0, 0.02);
    double risk = (100.0 - score) / 100.0 * 0.8 + jitter(m_rng);
    risk = std::clamp(risk, 0.0, 1.0);
    result.faultRisk = risk;
    if (risk < 0.3)      result.riskLevel = QStringLiteral("low");
    else if (risk < 0.6) result.riskLevel = QStringLiteral("medium");
    else                 result.riskLevel = QStringLiteral("high");

    // 聚合口径要求：健康度写回 charger 表
    m_provider->updateChargerHealth(chargerId, score);
    return result;
}

QList<ChargerHealth> HealthAssessmentEngine::assessAll()
{
    QList<ChargerHealth> out;
    for (const auto& c : m_provider->chargers())
        out.append(assess(c.id));
    // §3.5 高风险桩排在列表顶部
    std::sort(out.begin(), out.end(),
              [](const ChargerHealth& a, const ChargerHealth& b) {
                  return a.faultRisk > b.faultRisk;
              });
    return out;
}

} // namespace ml
