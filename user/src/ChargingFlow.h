#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>

class ExploreData;
class UserData;

// 充电全流程状态机（预约 → 排队 → 扫码 → 充电 → 结算）。
//
// 职责划分（重要 —— 为将来接真服务端留空间）：
// ─── 服务端负责（当前由本类以 mock 模拟，落盘后用 WebSocket 消息替换）───
//   · reservation/queue：能否直接匹配空闲桩、排队序号、预计等待、到点/轮到通知
//     （对应消息：reservation.join / reservation.join_resp / push.reservation_notify /
//      reservation.cancel，队列安排由服务端做，客户端不自行排号）
//   · charging_order 状态流转：创建(reserved) → 开始(charging) → 完成(finish) → 结算(settle)
//     （对应：order.create / order.start / order.finish / order.settle）
//   · 分时计价、违约金/占位费、信用分、推送(order_progress / 异常 order_timeout 等)
// ─── 客户端负责（View/ViewModel，本类只持有并暴露给 QML）───
//   · 预约表单输入（预约时间/车辆/目标电量/快慢偏好）、扫码录入、页面导航
//   · 充电面板的动画与展示（进度条/数字跳动）
//
// 本类把所有「应由服务端做的决策」收敛到独立的私有 mock 方法里（见
// mockServer* 命名），每个方法头部标注它将来映射到哪条协议消息。这样接后端时
// 只需把对 mock 方法的调用替换为 backend.send(..., payload)，QML 层几乎不动。
class ChargingFlow : public QObject {
    Q_OBJECT

    // 流程阶段：idle / queued / scan_pending / charging / settle / cancelled
    Q_PROPERTY(QString phase READ phase NOTIFY stateChanged)
    // 当前流程上下文（树蛇形键，含排队/桩/进度/结算/违约字段，供页面绑定）
    Q_PROPERTY(QVariantMap flow READ flow NOTIFY stateChanged)
    // 当前「正在进行」的订单（充电中 charging / 待结算 pending_settle），供首页栏合并展示；
    // 未在充电时为空 map。示例阶段由本 mock 动态生成，接后端后替换为真实订单号。
    Q_PROPERTY(QVariantMap currentOrder READ currentOrder NOTIFY stateChanged)

public:
    explicit ChargingFlow(QObject* parent = nullptr);

    // 数据源注入：ExploreData 充当「种子 DB」，UserData 充当「用户域」。mock 服务端决策依赖它们。
    void setDataSources(ExploreData* explore, UserData* user) { m_explore = explore; m_user = user; }

    QString phase() const { return m_phase; }
    QVariantMap flow() const { return m_flow; }
    // 当前进行中订单（charging/pending_settle），供首页「正在进行」栏展示
    QVariantMap currentOrder() const;

    // —— 动作（每个对应未来一条消息，见头注释）——
    // 发起预约/充电：reserveType=immediate/timed，expectTime="HH:mm"（定时），vehicleId、targetSoc、speed=fast/slow
    Q_INVOKABLE void startCharge(int stationId, const QString& reserveType,
                                 const QString& expectTime, int vehicleId,
                                 int targetSoc, const QString& speed);
    // 取消预约/排队（reservation.cancel）
    Q_INVOKABLE void cancel();
    // 扫码确认启动：stationId 校验 + code 桩码一致性（order.start）
    Q_INVOKABLE bool confirmScan(int stationId, const QString& code);
    // 主动结束充电（order.finish）
    Q_INVOKABLE void finishCharging();
    // 结算（order.settle，couponId 可为 0）
    Q_INVOKABLE void settle(int couponId);
    // 返回首页/重置（不取消账单，仅清 UI 态）
    Q_INVOKABLE void reset();

    // —— 违约/异常演示（示例阶段人为触发，独立放行）——
    Q_INVOKABLE void simulateScanTimeout();   // 扫码超时 → no_show（违约金+信用分）
    Q_INVOKABLE void simulatePeel();          // 充电中途拔枪 → 异常转结算

signals:
    // 状态机整体变化（QML 据此重算全部绑定）
    void stateChanged();
    // 服务端推送：轮到你了/已匹配（push.reservation_notify），message 含桩号
    void reservationReady(const QString& message);
    // 服务端推送：异常提醒（push.order_progress / alarm），title + 文案
    void abnormal(const QString& title, const QString& sub);

private:
    // —— mock 服务端：预约/排队决策（reservation.join + 随机匹配）——
    void mockReserve(const QString& speed);
    bool mockTryMatch(const QString& speed);   // 尝试从空闲桩挑一根（按速度偏好），成功→scan_pending
    void mockAdvanceQueue();          // 排队轮候定时推进，轮到→通知用户
    void mockAssignCharger(const QVariantMap& c); // 选定空闲桩并回填 charger_id/code，进入 scan_pending
    // —— mock 服务端：预约扫码截止（push.order_timeout）——
    void mockStartScanDeadline();
    void mockScanTimeout();
    // —— mock 服务端：充电进度（push.order_progress / charging_measure）——
    void mockStartCharging();
    void mockProgressTick();
    void mockFinishCharging(bool userEnded);
    // —— mock 服务端：结算/占位/违约计价 ——
    void mockStartOccupy();
    void mockOccupyTick();
    void mockSettle(int couponId);
    void mockApplyPenalty(qreal penalty, int creditLoss, const QString& reason);

    // 当前时段电价（元/kWh，含服务费）：按 price_rule 的 time_range 取当前档
    double currentUnitPrice() const;

    void setPhase(const QString& p);
    void setFlow(const QString& key, const QVariant& v);

    ExploreData* m_explore = nullptr;
    UserData* m_user = nullptr;

    QString m_phase = QStringLiteral("idle");
    QVariantMap m_flow;

    int m_queueOrdinal = 0;      // 全局排队序号发生器（服务端安排）
    int m_orderSeq = 0;          // 订单序号发生器

    QTimer m_queueTimer;         // 队列轮候推进
    QTimer m_scanTimer;          // 扫码截止
    QTimer m_progressTimer;      // 充电进度
    QTimer m_occupyTimer;        // 占位计时
};