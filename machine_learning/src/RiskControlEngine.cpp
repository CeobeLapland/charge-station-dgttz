#include "ml/RiskControlEngine.h"

#include <algorithm>

#include "ml/MlDataProvider.h"

namespace ml {

// 附录 A：预约次数阈值 10（可调 5–20），取消率阈值 0.6（可调 0.5–0.8）
static constexpr int RESERVE_COUNT_24H = 10;
static constexpr double CANCEL_RATE_THRESHOLD = 0.6;
static constexpr int SHORT_CHARGE_MIN = 5;
static constexpr int SHORT_CHARGE_TIMES_PER_DAY = 3;

RiskControlEngine::RiskControlEngine(MlDataProvider* provider)
    : m_provider(provider)
{
}

UserRisk RiskControlEngine::evaluate(int userId, const QDateTime& now)
{
    UserRisk result;
    result.userId = userId;

    // 规则 1：24h 内预约 ≥10 且取消率 ≥0.6
    const int total = m_provider->reservationCountSince(userId, now.addDays(-1));
    const int cancelled = m_provider->cancelledReservationCountSince(userId, now.addDays(-1));
    const double cancelRate = total > 0 ? static_cast<double>(cancelled) / total : 0.0;
    result.rule1HighFreqCancel = total >= RESERVE_COUNT_24H
                                     && cancelRate >= CANCEL_RATE_THRESHOLD;

    // 规则 2：当天 <5 分钟短时充电 ≥3 次（疑似刷单）
    int shortCount = 0;
    for (const auto& o : m_provider->allOrders(now.addDays(-1))) {
        if (o.userId == userId && o.durationMin > 0 && o.durationMin < SHORT_CHARGE_MIN
                && o.startTime.date() == now.date())
            shortCount++;
    }
    result.rule2ShortCharge = shortCount >= SHORT_CHARGE_TIMES_PER_DAY;

    // §9.2 风险评分（Risk3 为预留占位，计 0）
    result.riskScore = std::clamp(50.0 * (result.rule1HighFreqCancel ? 1 : 0)
                                      + 30.0 * (result.rule2ShortCharge ? 1 : 0),
                                  0.0, 100.0);
    if (result.rule1HighFreqCancel)
        result.hitRules.append(QStringLiteral("rule1_high_freq_cancel"));
    if (result.rule2ShortCharge)
        result.hitRules.append(QStringLiteral("rule2_short_charge"));

    if (result.riskScore <= 40) {
        result.level = QStringLiteral("low");
        result.action = QStringLiteral("无需处置");
    } else if (result.riskScore <= 70) {
        result.level = QStringLiteral("medium");
        result.action = QStringLiteral("限制预约次数（每日上限 5 次）");
    } else {
        result.level = QStringLiteral("high");
        result.action = QStringLiteral("冻结预约功能 24 小时，同步管理端");
    }
    return result;
}

QList<UserRisk> RiskControlEngine::evaluateAll(const QDateTime& now)
{
    QList<UserRisk> out;
    for (int uid : m_provider->userIds()) {
        UserRisk r = evaluate(uid, now);
        if (!r.hitRules.isEmpty())
            out.append(r);
    }
    std::sort(out.begin(), out.end(),
              [](const UserRisk& a, const UserRisk& b) {
                  return a.riskScore > b.riskScore;
              });
    return out;
}

} // namespace ml
