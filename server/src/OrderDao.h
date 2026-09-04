#pragma once
#include <QList>
#include <QString>
#include <optional>

// ============================================================
// OrderDao — 充电订单全流程
// 对应协议消息 order.create / start / finish / settle / cancel / list / detail
//
// 订单状态机(charging_order.status, 与 schema.sql 的 CHECK 一致):
//
//   reserved ──start──> charging ──finish──> pending_settle ──settle──> completed
//      │
//      └──cancel──> cancelled
//
//   每一步都校验"当前状态是否允许该操作", 不允许一律返回 2003。
//
// 业务规则(写不进表约束, 只能在这里):
//   · 一个用户同时只能有一个进行中订单(reserved/charging/pending_settle) → 2001
//   · 电桩必须是 idle 才能下单                                          → 3002
//   · 起充余额门槛 10 元; 结算时余额不足                                 → 2002
//   · 订单必须属于当前登录用户, 否则一律按"不存在"处理                    → 4001
//     (不区分"不存在"和"不是你的", 避免泄露别人的订单 id 是否存在)
// ============================================================

struct OrderView {
    int     id = 0, userId = 0, stationId = 0, chargerId = 0, vehicleId = 0;
    QString stationName, chargerCode;
    QString status;
    double  startSoc = 0, targetSoc = 0, endSoc = 0;
    QString startTime, endTime, createTime, settleTime;
    QString priceLevel;
    int     durationMin = 0;
    double  energyKwh = 0, amount = 0, discountAmount = 0, payAmount = 0;
    int     pointsUsed = 0, pointsEarned = 0;
};

struct TimelineRow {
    int     id = 0;
    QString node, label, eventTime, detail;
};

// 业务失败时回填的错误码与说明(0 = 没有错误)
struct OpError {
    int     code = 0;
    QString message;
};

namespace dao {

// 起充余额门槛(元)。低于这个数不允许开始充电。
constexpr double kMinStartBalance = 10.0;
// 积分抵扣汇率: 100 积分 = 1 元
constexpr int    kPointsPerYuan   = 100;

// 创建订单(预约)。成功后电桩置 reserved。
std::optional<OrderView> createOrder(int userId, int stationId, int chargerId, OpError *err);

// 开始充电。startSoc < 0 时按车辆默认起始电量 20% 处理。
std::optional<OrderView> startOrder(int userId, int orderId, double startSoc, OpError *err);

// 结束充电, 转 pending_settle 并算出电量/金额。
// endSoc >= 0 时按 SOC 差算电量(演示/仿真用); endSoc < 0 时按实际时长 × 功率估算。
std::optional<OrderView> finishOrder(int userId, int orderId, double endSoc, OpError *err);

// 结算: 扣余额 + 发积分 + 记两张流水 + 改订单 + 写时间轴, 全部在一个事务里。
// pointsUsed 为本次使用的积分(100 分抵 1 元), 传 0 表示不抵扣。
std::optional<OrderView> settleOrder(int userId, int orderId, int pointsUsed, OpError *err);

// 取消订单(仅 reserved 状态可取消), 释放电桩。
std::optional<OrderView> cancelOrder(int userId, int orderId, OpError *err);

// 我的订单。status 为空表示全部。
QList<OrderView> listOrders(int userId, const QString &status);

// 订单详情(会校验归属)。
std::optional<OrderView> findOrder(int userId, int orderId);

// 订单时间轴。
QList<TimelineRow> timelineOf(int orderId);

// 仿真器需要的参数: 桩功率、车辆电池容量、当前单价(分时电价 + 服务费)。
struct SimParams {
    double powerKw = 0, batteryKwh = 60, unitPrice = 0;
    bool   ok = false;
};
SimParams simParamsOf(int orderId);

// ---- 充电仿真写入(由主线程调用, SQL 仍然只在 DAO 层) ----
// 刷新电桩的实时电气参数(管理端数字孪生面板用)。
void updateChargerElectrics(int chargerId, double voltage, double current, double temperature);
// 往 charging_measure 落一条时序点(大屏负荷曲线用)。
void insertMeasure(int chargerId, int stationId, const QString &time,
                   double powerKw, double soc, double energyDelta, double temperature);

// 当前余额/积分(结算后回给客户端刷新用)。
double userBalance(int userId);
int    userPoints(int userId);

}  // namespace dao
