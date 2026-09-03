#pragma once
#include <QList>
#include <QString>
#include <optional>

// ============================================================
// StationDao — 充电站相关查询
// 对应协议消息 station.nearby / station.detail 的数据来源。
// ============================================================

// 电站列表视图: station 表字段 + 实时统计出的空闲桩数
struct StationView {
    int     id = 0;
    QString name;
    QString address;
    QString area;
    double  longitude = 0.0;
    double  latitude  = 0.0;
    int     totalChargers = 0;
    int     freeChargers  = 0;   // 由 charger 表按 status='idle' 现算, 不存库
    double  serviceFee    = 0.0; // 服务费(元/kWh)
};

// 电桩视图(站内明细用)
struct ChargerView {
    int     id = 0;
    QString code;     // 站内编号 A-001
    QString type;     // fast / slow
    double  power = 0.0;
    QString status;   // idle / charging / reserved / fault / offline / rebooting
    int     healthScore = 100;
};

namespace dao {

// 全部电站列表(空闲数实时统计)。距离计算/排序由服务端处理器完成。
QList<StationView> listStations();

// 按 id 查单座电站, 查不到返回 nullopt。
std::optional<StationView> findStationById(int stationId);

// 某站的全部电桩明细。
QList<ChargerView> listChargersOfStation(int stationId);

// 当前时段的电价(元/kWh): 先找该站专属 price_rule, 没有则用全站通用规则。
// 档位按小时判定: 0-8点谷 / 17-21点峰 / 其余平 (与 price_rule.time_range 一致)。
double currentPrice(int stationId);

}  // namespace dao
