(function () {
  "use strict";

  const now = new Date();
  const timeAt = (offsetMinutes) => {
    const date = new Date(now.getTime() + offsetMinutes * 60000);
    return date.toISOString();
  };

  window.ScreenMockSnapshot = {
    metrics: {
      today_revenue: 18260.5,
      today_orders: 386,
      charging_count: 48,
      online_rate: 96.2,
      revenue_change_pct: 12.8,
      orders_change_pct: 8.4
    },
    stations: [
      { id: 1, name: "软件园快充站", longitude: 123.43, latitude: 41.77, load_rate: 88, idle_chargers: 2, total_chargers: 18, today_revenue: 5260 },
      { id: 2, name: "浑南中心站", longitude: 123.46, latitude: 41.72, load_rate: 66, idle_chargers: 6, total_chargers: 20, today_revenue: 4380 },
      { id: 3, name: "奥体交通枢纽站", longitude: 123.44, latitude: 41.74, load_rate: 54, idle_chargers: 9, total_chargers: 24, today_revenue: 3915 },
      { id: 4, name: "青年大街充电站", longitude: 123.42, latitude: 41.79, load_rate: 35, idle_chargers: 11, total_chargers: 16, today_revenue: 2780 },
      { id: 5, name: "机场路合作站", longitude: 123.50, latitude: 41.69, load_rate: 73, idle_chargers: 4, total_chargers: 15, today_revenue: 1925 }
    ],
    load_series: {
      actual: [-35, -30, -25, -20, -15, -10, -5, 0].map((offset, index) => ({
        timestamp: timeAt(offset),
        value_kw: [610, 655, 690, 720, 765, 810, 842, 875][index]
      })),
      forecast: [0, 5, 10, 15, 20, 25, 30].map((offset, index) => ({
        timestamp: timeAt(offset),
        value_kw: [875, 902, 930, 955, 938, 910, 886][index],
        lower_kw: [842, 866, 890, 914, 898, 871, 848][index],
        upper_kw: [908, 938, 970, 996, 978, 949, 924][index]
      }))
    },
    utilization_rank: [
      { station_id: 1, station_name: "软件园快充站", utilization_rate: 88 },
      { station_id: 5, station_name: "机场路合作站", utilization_rate: 73 },
      { station_id: 2, station_name: "浑南中心站", utilization_rate: 66 },
      { station_id: 3, station_name: "奥体交通枢纽站", utilization_rate: 54 },
      { station_id: 4, station_name: "青年大街充电站", utilization_rate: 35 }
    ],
    alarms: [
      { id: 101, station_id: 1, station_name: "软件园快充站", charger_id: 7, type: "overheat", level: "critical", occur_time: timeAt(-3) },
      { id: 102, station_id: 5, station_name: "机场路合作站", charger_id: 3, type: "comm_abnormal", level: "warning", occur_time: timeAt(-7) },
      { id: 103, station_id: 2, station_name: "浑南中心站", charger_id: null, type: "power_drop", level: "info", occur_time: timeAt(-12) }
    ],
    user_growth: [
      { date: "08-27", new_users: 42 },
      { date: "08-28", new_users: 51 },
      { date: "08-29", new_users: 47 },
      { date: "08-30", new_users: 68 },
      { date: "08-31", new_users: 73 },
      { date: "09-01", new_users: 81 },
      { date: "09-02", new_users: 96 }
    ],
    energy_by_price_level: {
      valley: 3820,
      flat: 5160,
      peak: 2740
    },
    events: [
      { id: "evt-1", event_time: timeAt(-1), text: "用户 138****8241 在软件园快充站 A03 开始充电" },
      { id: "evt-2", event_time: timeAt(-3), text: "软件园快充站 A07 发生设备温度告警" },
      { id: "evt-3", event_time: timeAt(-5), text: "用户 189****4421 完成充电并结算" },
      { id: "evt-4", event_time: timeAt(-8), text: "机场路合作站当前负载达到 73%" },
      { id: "evt-5", event_time: timeAt(-11), text: "浑南中心站 B12 从离线状态恢复" },
      { id: "evt-6", event_time: timeAt(-15), text: "全网未来 1 小时负荷预测已更新" }
    ]
  };
}());
