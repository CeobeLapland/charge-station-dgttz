#include "ml/AssistantEngine.h"

#include <algorithm>

#include "ml/HealthAssessmentEngine.h"
#include "ml/LoadForecastingEngine.h"
#include "ml/MlDataProvider.h"

namespace ml {

AssistantEngine::AssistantEngine(MlDataProvider* provider,
                                 LoadForecastingEngine* forecastEngine,
                                 HealthAssessmentEngine* healthEngine)
    : m_provider(provider)
    , m_forecast(forecastEngine)
    , m_health(healthEngine)
{
}

QString AssistantEngine::queryRevenue(const QDate& today)
{
    const double todayRevenue = m_provider->revenueOn(today);
    const double yesterdayRevenue = m_provider->revenueOn(today.addDays(-1));
    double trend = 0.0;
    QString trendWord = QStringLiteral("持平");
    if (yesterdayRevenue > 0.001) {
        trend = (todayRevenue - yesterdayRevenue) / yesterdayRevenue * 100.0;
        trendWord = trend >= 0 ? QStringLiteral("增长") : QStringLiteral("下降");
    }
    return QStringLiteral("今日营收 ¥%1，较昨日 %2%3%")
        .arg(todayRevenue, 0, 'f', 2)
        .arg(trendWord)
        .arg(std::abs(trend), 0, 'f', 1);
}

QString AssistantEngine::queryBusiestStation(const QDate& today)
{
    Q_UNUSED(today);
    int count = 0;
    QString name;
    const int stationId = m_provider->busiestStationToday(&count, &name);
    if (stationId <= 0)
        return QStringLiteral("今日暂无订单记录");
    return QStringLiteral("最繁忙的站点是 %1，今日已处理 %2 笔订单")
        .arg(name).arg(count);
}

QString AssistantEngine::queryFaultCount()
{
    const int faultCount = m_provider->faultChargerCount();
    int highRisk = 0;
    for (const auto& c : m_provider->chargers()) {
        if (c.status != QLatin1String("fault"))
            continue;
        // 复用健康度评分口径统计高风险台数
        const double risk = (100.0 - c.healthScore) / 100.0 * 0.8;
        if (risk >= 0.6)
            highRisk++;
    }
    return QStringLiteral("当前共有 %1 台设备处于故障状态，其中高风险 %2 台")
        .arg(faultCount).arg(highRisk);
}

QString AssistantEngine::queryUserGrowth(const QDate& today)
{
    return QStringLiteral("今日新增用户 %1 人，累计注册用户 %2 人")
        .arg(m_provider->newUserCount(today))
        .arg(m_provider->totalUserCount());
}

QString AssistantEngine::queryTonightLoad(const QDateTime& now)
{
    int count = 0;
    QString name;
    int stationId = m_provider->busiestStationToday(&count, &name);
    if (stationId <= 0)
        stationId = m_provider->stations().isEmpty() ? 0 : m_provider->stations().first().id;
    if (stationId <= 0)
        return QStringLiteral("暂无可用的站点数据");

    // 预测今晚 18:00-22:00 高峰负荷
    QDateTime start = now;
    start.setTime(QTime(18, 0));
    if (start < now)
        start = start.addDays(1);
    const auto forecast = m_forecast->predict(stationId, 24, start);
    double peakLoad = 0.0;
    for (int i = 0; i < 5 && i < forecast.predicted.size(); ++i)
        peakLoad = std::max(peakLoad, forecast.predicted[i]);
    return QStringLiteral("预测今晚 18:00-22:00 高峰负荷 %1 kW（%2），建议提前调配电力")
        .arg(peakLoad, 0, 'f', 1).arg(name);
}

AssistantReply AssistantEngine::answerQuery(const QString& question, const QDateTime& now)
{
    AssistantReply reply;
    const QString q = question.toLower();

    if (q.contains(QStringLiteral("营收")) || q.contains(QStringLiteral("收入"))) {
        reply.intent = QStringLiteral("revenue");
        reply.answer = queryRevenue(now.date());
    } else if (q.contains(QStringLiteral("最忙")) || q.contains(QStringLiteral("繁忙"))) {
        reply.intent = QStringLiteral("busiest_station");
        reply.answer = queryBusiestStation(now.date());
    } else if (q.contains(QStringLiteral("故障")) || q.contains(QStringLiteral("坏"))) {
        reply.intent = QStringLiteral("fault_count");
        reply.answer = queryFaultCount();
    } else if (q.contains(QStringLiteral("用户增长")) || q.contains(QStringLiteral("新增用户"))) {
        reply.intent = QStringLiteral("user_growth");
        reply.answer = queryUserGrowth(now.date());
    } else if (q.contains(QStringLiteral("负荷")) || q.contains(QStringLiteral("预测"))
               || q.contains(QStringLiteral("今晚"))) {
        reply.intent = QStringLiteral("load_forecast");
        reply.answer = queryTonightLoad(now);
    } else {
        reply.intent = QStringLiteral("fallback");
        reply.fallback = true;
        reply.answer = QStringLiteral(
            "抱歉，暂不支持该问题，请尝试询问：今日营收、设备故障、用户增长等");
    }
    return reply;
}

} // namespace ml
