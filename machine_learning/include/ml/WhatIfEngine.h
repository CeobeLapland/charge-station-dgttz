#pragma once

#include "ml/MlTypes.h"

namespace ml {

class MlDataProvider;

// §10 what-if 决策仿真：四参数推演平均等待/峰值利用率/日订单/日营收
class WhatIfEngine
{
public:
    explicit WhatIfEngine(MlDataProvider* provider);

    WhatIfImpact simulate(int stationId, int addChargers, double priceDelta,
                          double failureScale, double trafficDelta);

private:
    MlDataProvider* m_provider;
};

} // namespace ml
