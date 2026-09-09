#pragma once

#include <random>

#include "ml/MlTypes.h"

namespace ml {

class MlDataProvider;

// §3 设备健康度与故障预测：扣分制健康度 + 故障风险概率
class HealthAssessmentEngine
{
public:
    explicit HealthAssessmentEngine(MlDataProvider* provider);
    void setSeed(quint32 seed);

    ChargerHealth assess(int chargerId);
    QList<ChargerHealth> assessAll();

private:
    MlDataProvider* m_provider;
    std::mt19937 m_rng;
};

} // namespace ml
