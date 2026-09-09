#include "ml/ReviewTagAnalyzer.h"

#include <QMap>
#include <algorithm>

#include "ml/MlDataProvider.h"

namespace ml {

static constexpr int CURRENT_WINDOW_DAYS = 30;
static constexpr double MOM_EPSILON = 1.0; // §11.2 环比分母 ε，防除零

ReviewTagAnalyzer::ReviewTagAnalyzer(MlDataProvider* provider)
    : m_provider(provider)
{
}

ReviewSummary ReviewTagAnalyzer::analyze(int stationId, const QDate& today)
{
    ReviewSummary result;
    result.stationId = stationId;

    const QDate currentBegin = today.addDays(-CURRENT_WINDOW_DAYS);
    const QDate previousBegin = today.addDays(-2 * CURRENT_WINDOW_DAYS);

    const QStringList currentTags = m_provider->reviewTagsSince(stationId, currentBegin);
    const QStringList previousTags = m_provider->reviewTagsSince(stationId, previousBegin);

    QMap<QString, int> freq;
    for (const auto& t : currentTags)
        freq[t]++;

    QMap<QString, int> prevFreq;
    for (const auto& t : previousTags) {
        if (t >= currentBegin.toString(QStringLiteral("yyyy-MM-dd")))
            continue; // previous 窗口只统计再前 30 天
        prevFreq[t]++;
    }

    // §11.3 TOP5（按频次降序）
    QList<ReviewTagStat> stats;
    for (auto it = freq.begin(); it != freq.end(); ++it) {
        ReviewTagStat s;
        s.tag = it.key();
        s.freq = it.value();
        // §11.2 MoM = (本期 - 上期) / (上期 + ε)
        s.mom = (it.value() - prevFreq.value(it.key()))
                    / static_cast<double>(prevFreq.value(it.key()) + MOM_EPSILON);
        stats.append(s);
    }
    std::sort(stats.begin(), stats.end(),
              [](const ReviewTagStat& a, const ReviewTagStat& b) {
                  return a.freq > b.freq;
              });
    result.topTags = stats.mid(0, 5);

    // §11.4 五维评分均值（最近 30 天）
    result.dimAverages = m_provider->reviewDimAverages(stationId, currentBegin);
    return result;
}

} // namespace ml
