#pragma once
#include <QList>
#include <QString>

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

namespace dao {

// 全部电站列表(空闲数实时统计)。距离排序等逻辑后续在服务端按需实现。
QList<StationView> listStations();

}  // namespace dao
