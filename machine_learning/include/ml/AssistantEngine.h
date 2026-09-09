#pragma once

#include "ml/MlTypes.h"

namespace ml {

class MlDataProvider;
class LoadForecastingEngine;
class HealthAssessmentEngine;

// §12 AI 运营助手：关键词意图匹配 + 真实数据查询填充模板
class AssistantEngine
{
public:
    AssistantEngine(MlDataProvider* provider, LoadForecastingEngine* forecastEngine,
                    HealthAssessmentEngine* healthEngine);

    AssistantReply answerQuery(const QString& question, const QDateTime& now);

private:
    QString queryRevenue(const QDate& today);
    QString queryBusiestStation(const QDate& today);
    QString queryFaultCount();
    QString queryUserGrowth(const QDate& today);
    QString queryTonightLoad(const QDateTime& now);

    MlDataProvider* m_provider;
    LoadForecastingEngine* m_forecast;
    HealthAssessmentEngine* m_health;
};

} // namespace ml
