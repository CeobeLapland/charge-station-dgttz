#pragma once

#include "ml/MlTypes.h"

namespace ml {

class MlDataProvider;

// §9 智能风控：高频预约取消 + 异常短时充电 + 风险评分分级处置
class RiskControlEngine
{
public:
    explicit RiskControlEngine(MlDataProvider* provider);

    UserRisk evaluate(int userId, const QDateTime& now);
    QList<UserRisk> evaluateAll(const QDateTime& now);

private:
    MlDataProvider* m_provider;
};

} // namespace ml
