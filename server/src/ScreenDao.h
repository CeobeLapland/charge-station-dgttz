#pragma once
#include <QList>
#include <QString>

// ============================================================
// ScreenDao — 数据大屏(只读)相关聚合查询
// 对应协议消息 screen.snapshot。
// 字段与口径见 screen/API_CONTRACT.md, 已在 spec-协议.md 定稿。
//
// 统一口径(服务端唯一定义, 管理端/大屏都以此为准):
//   today_*        = 今天 00:00:00 至此刻
//   today_orders   = 今日"创建"的订单数(按 create_time, 不论最终状态)
//   today_revenue  = 今日已结算订单(status='completed') 的 pay_amount 合计
//   online_rate    = 在线桩数 / 总桩数 × 100  (0-100, 非 0-1)
//                    在线 = status NOT IN ('offline','fault')
//   load_rate      = 该站 charging 桩数 / 该站总桩数 × 100
//   utilization    = 该站今日订单时长合计 / (桩数 × 今日已过分钟数) × 100, 上限 100
//   *_change_pct   = (今日值 - 昨日同期值) / 昨日同期值 × 100, 昨日同期为 0 时返回 0
//   时间一律本地时间, 格式 "yyyy-MM-dd HH:mm:ss"
// ============================================================

struct ScreenMetrics {
    double todayRevenue = 0;
    int    todayOrders  = 0;
    int    chargingCount = 0;
    double onlineRate = 100;      // 0-100
    double revenueChangePct = 0;
    double ordersChangePct  = 0;
};

struct ScreenStation {
    int     id = 0;
    QString name;
    double  longitude = 0, latitude = 0;
    double  loadRate = 0;         // 0-100
    int     idleChargers = 0, totalChargers = 0;
    double  todayRevenue = 0;
};

struct LoadPoint {
    QString timestamp;            // yyyy-MM-dd HH:mm:ss
    double  valueKw = 0;
};

struct UtilizationRow {
    int     stationId = 0;
    QString stationName;
    double  utilizationRate = 0;  // 0-100
};

struct AlarmRow {
    int     id = 0, stationId = 0;
    QString stationName;
    int     chargerId = 0;        // 0 = 站点级告警, 输出时转成 null
    QString type, level, status, occurTime;
};

struct UserGrowthPoint {
    QString date;                 // yyyy-MM-dd
    int     newUsers = 0;
};

struct EnergyByLevel {
    double valley = 0, flat = 0, peak = 0;
};

struct EventRow {
    QString id;                   // 稳定唯一, 形如 evt-o531-started
    QString eventTime, eventType, text;   // text 已脱敏(手机号中间四位打码)
};

namespace dao {

ScreenMetrics          screenMetrics();
QList<ScreenStation>   screenStations();
QList<LoadPoint>       loadSeriesActual(int hours);      // 近 hours 小时的实际负荷
QList<UtilizationRow>  utilizationRank(int limit);
QList<AlarmRow>        recentAlarms(int limit);
QList<UserGrowthPoint> userGrowth(int days);
EnergyByLevel          energyByPriceLevel();             // 近7日(见 .cpp 注释)
QList<EventRow>        recentEvents(int limit);          // 今日事件流(倒序)

}  // namespace dao
