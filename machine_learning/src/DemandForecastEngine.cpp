#include "ml/DemandForecastEngine.h"

#include <QMap>
#include <algorithm>

#include "ml/MlDataProvider.h"

namespace ml {

static constexpr double SURGE_THRESHOLD = 0.5; // Δ > 0.5 触发需求激增预警

DemandForecastEngine::DemandForecastEngine(MlDataProvider* provider)
    : m_provider(provider)
{
}

DemandForecast DemandForecastEngine::forecast(const QDateTime& now)
{
    // 最近 30 天全平台订单（仿真库实际覆盖约 15 天，按可用数据聚合）
    const auto orders = m_provider->allOrders(now.addDays(-30));

    QMap<int, StationInfo> stationById;
    for (const auto& s : m_provider->stations())
        stationById.insert(s.id, s);

    // D[dow][hour][area]
    QMap<int, QMap<int, QMap<QString, int>>> demand;
    for (const auto& o : orders) {
        if (!o.startTime.isValid())
            continue;
        const int dow = o.startTime.date().dayOfWeek() - 1;
        const int hour = o.startTime.time().hour();
        const QString area = stationById.value(o.stationId).area;
        demand[dow][hour][area]++;
    }

    DemandForecast result;

    // 区域×小时历史均值（不区分星期）
    QMap<QString, QMap<int, double>> hourAreaMean;
    QMap<QString, QMap<int, int>> hourAreaCnt;
    for (auto itD = demand.begin(); itD != demand.end(); ++itD) {
        for (auto itH = itD.value().begin(); itH != itD.value().end(); ++itH) {
            for (auto itA = itH.value().begin(); itA != itH.value().end(); ++itA) {
                hourAreaMean[itA.key()][itH.key()] += itA.value();
                hourAreaCnt[itA.key()][itH.key()]++;
            }
        }
    }
    for (auto itA = hourAreaMean.begin(); itA != hourAreaMean.end(); ++itA) {
        for (auto itH = itA.value().begin(); itH != itA.value().end(); ++itH) {
            const int cnt = hourAreaCnt[itA.key()][itH.key()];
            itH.value() = cnt > 0 ? itH.value() / cnt : 0.0;
        }
    }

    struct SurgeCell { DemandHeatCell cell; };
    QList<SurgeCell> surges;
    for (auto itD = demand.begin(); itD != demand.end(); ++itD) {
        for (auto itH = itD.value().begin(); itH != itD.value().end(); ++itH) {
            for (auto itA = itH.value().begin(); itA != itH.value().end(); ++itA) {
                DemandHeatCell cell;
                cell.dow = itD.key();
                cell.hour = itH.key();
                cell.area = itA.key();
                cell.demand = itA.value();
                const double base = hourAreaMean[cell.area][cell.hour];
                cell.delta = base > 0.001 ? (cell.demand - base) / base : 0.0;
                cell.surge = cell.delta > SURGE_THRESHOLD;
                result.heatmap.append(cell);
                if (cell.surge)
                    surges.append({cell});
            }
        }
    }

    std::sort(surges.begin(), surges.end(),
              [](const SurgeCell& a, const SurgeCell& b) {
                  return a.cell.demand > b.cell.demand;
              });
    const int topN = std::min(5, static_cast<int>(surges.size()));
    for (int i = 0; i < topN; ++i) {
        const auto& cell = surges[i].cell;
        SurgeWindow w;
        w.window = QStringLiteral("%1 %2:00-%3:00 %4")
                       .arg(dayOfWeekName(cell.dow))
                       .arg(cell.hour, 2, 10, QLatin1Char('0'))
                       .arg(cell.hour + 1, 2, 10, QLatin1Char('0'))
                       .arg(cell.area);
        const int suggest = std::max(2, static_cast<int>(std::ceil(cell.demand / 4.0)));
        w.demand = cell.demand;
        w.recommendation = QStringLiteral("建议在%1增配快充桩 %2 台").arg(cell.area).arg(suggest);
        result.topSurgeWindows.append(w);
    }
    return result;
}

} // namespace ml
