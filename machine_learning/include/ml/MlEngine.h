#pragma once

#include <QJsonObject>

#include "ml/AssistantEngine.h"
#include "ml/DemandForecastEngine.h"
#include "ml/DispatchEngine.h"
#include "ml/HealthAssessmentEngine.h"
#include "ml/LoadForecastingEngine.h"
#include "ml/MlDataProvider.h"
#include "ml/RecommendationEngine.h"
#include "ml/ReviewTagAnalyzer.h"
#include "ml/RiskControlEngine.h"
#include "ml/WaitTimeEngine.h"
#include "ml/WhatIfEngine.h"

namespace ml {

// 门面：组合全部引擎，对外提供与协议对齐的 QJsonObject 接口（键一律小写蛇形）
class MlEngine
{
public:
    MlEngine();

    bool openDatabase(const QString& path);
    void closeDatabase();
    bool isOpen() const;
    MlDataProvider* provider();

    // ml.forecast / push.forecast
    QJsonObject forecast(int stationId, int horizonHours);
    // station.recommend
    QJsonObject recommend(double longitude, double latitude, const QString& category);
    // 用户端等待时间（含于 station.detail 等场景）
    QJsonObject waitEstimate(int stationId);
    // admin.fault_risk
    QJsonObject faultRisk();
    // admin.whatif
    QJsonObject whatIf(const QJsonObject& scenario);
    // admin.assistant_query
    QJsonObject assistantQuery(const QString& question);
    // 大屏需求热力图
    QJsonObject demandHeatmap();
    // 大屏/管理端调度检查
    QJsonObject dispatchCheck();
    // admin 风控列表
    QJsonObject riskUsers();
    // 管理端评价标签分析
    QJsonObject reviewTags(int stationId);

private:
    MlDataProvider m_provider;
    LoadForecastingEngine m_load;
    HealthAssessmentEngine m_health;
    RecommendationEngine m_recommend;
    WaitTimeEngine m_wait;
    DemandForecastEngine m_demand;
    DispatchEngine m_dispatch;
    RiskControlEngine m_risk;
    WhatIfEngine m_whatif;
    ReviewTagAnalyzer m_review;
    AssistantEngine m_assistant;
};

} // namespace ml
