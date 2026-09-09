#pragma once

#include "ml/MlTypes.h"

namespace ml {

class MlDataProvider;

// §11 评价标签分析：标签频次 + 30 天环比 + TOP5 + 五维评分均值
class ReviewTagAnalyzer
{
public:
    explicit ReviewTagAnalyzer(MlDataProvider* provider);

    ReviewSummary analyze(int stationId, const QDate& today);

private:
    MlDataProvider* m_provider;
};

} // namespace ml
