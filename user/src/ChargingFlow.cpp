#include "ChargingFlow.h"

#include <QDateTime>
#include <QTime>

#include <utility>

#include "ExploreData.h"
#include "UserData.h"

// —— 违约 / 流程阈值（spec-充电全流程 第4节建议初值，示例阶段可加速演示）——
namespace {
constexpr int    kScanWindowSec = 300;        // 扫码保留窗口（示例加快，真实建议 10 分钟）
constexpr int    kQueueAdvanceSec = 4;       // 排队轮到推进间隔（演示用，真实由服务端安排）
constexpr int    kOccupyGraceSec = 120;       // 占位宽限（示例加速，真实建议 10 分钟）
constexpr qreal  kNoShowPenalty = 5.0;       // no_show 违约金（元）
constexpr int    kNoShowCreditLoss = 5;      // no_show 信用分扣减
constexpr int    kCancelFreeSec = 20;        // 主动取消免费窗口（占已匹配的保留窗口，剩余>此值免费）
constexpr qreal  kCancelLatePenalty = 5.0;   // 临近扫码截止取消违约金（元）
constexpr int    kCancelLateCreditLoss = 2;  // 临近取消信用扣减
constexpr double kStartSocDefault = 20.0;    // 起始电量（示例无车辆实时电量，用默认值）

// 便捷构造 QVariantMap（接线消息 payload 用）
QVariantMap S(const std::initializer_list<std::pair<QString, QVariant>>& list) {
    QVariantMap m;
    for (const auto& kv : list)
        m.insert(kv.first, kv.second);
    return m;
}
} // namespace

ChargingFlow::ChargingFlow(QObject* parent)
    : QObject(parent) {
    connect(&m_queueTimer, &QTimer::timeout, this, &ChargingFlow::mockAdvanceQueue);
    connect(&m_scanTimer, &QTimer::timeout, this, &ChargingFlow::mockScanTimeout);
    connect(&m_scanTickTimer, &QTimer::timeout, this, &ChargingFlow::mockScanTick);
    connect(&m_progressTimer, &QTimer::timeout, this, &ChargingFlow::mockProgressTick);
    connect(&m_occupyTimer, &QTimer::timeout, this, &ChargingFlow::mockOccupyTick);

    m_flow = {
        { QStringLiteral("credit_score"), 100 },
        { QStringLiteral("occupied"), false },   // 标记充电中（避免重复操作）
    };
}

void ChargingFlow::setPhase(const QString& p) {
    if (m_phase == p)
        return;
    m_phase = p;
    m_flow.insert(QStringLiteral("phase"), p);
    emit stateChanged();
}

void ChargingFlow::setFlow(const QString& key, const QVariant& v) {
    m_flow.insert(key, v);
    emit stateChanged();
}

// —— 由 QML ReservePage 调用：发起预约/充电 ——
void ChargingFlow::startCharge(int stationId, const QString& reserveType,
                               const QString& expectTime, int vehicleId,
                               int targetSoc, const QString& speed) {
    m_queueTimer.stop();
    m_scanTimer.stop();
    m_progressTimer.stop();
    m_occupyTimer.stop();

    // 级联拦截（先于预约请求）：有未完成订单（待结算/已预约）时不能再次预约。
    // 与服务端 joinQueue/createOrder 的 2001 规则一致；这里在用户端提前提示，避免扫码后才报错。
    if (m_user) {
        const QVariantList all = m_user->orders();
        for (const QVariant& v : all) {
            const QString st = v.toMap().value(QStringLiteral("status")).toString();
            if (st == QStringLiteral("pending_settle")
                || st == QStringLiteral("reserved")) {
                emit abnormal(QStringLiteral("您有未付款账单"),
                              QStringLiteral("请先完成结算后再预约"));
                return;
            }
        }
    }

    const QVariantMap st = m_explore ? m_explore->stationById(stationId) : QVariantMap();
    QVariantMap vehicle;
    if (m_user) {
        for (const QVariant& v : m_user->vehicles()) {
            const QVariantMap vm = v.toMap();
            if (vm.value(QStringLiteral("id")).toInt() == vehicleId) {
                vehicle = vm;
                break;
            }
        }
    }

    QString now = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    m_queueOrdinal++;
    m_orderSeq++;

    m_flow = {
        { QStringLiteral("phase"), QString() },
        { QStringLiteral("station_id"), stationId },
        { QStringLiteral("station_name"), st.value(QStringLiteral("name")) },
        { QStringLiteral("station_address"), st.value(QStringLiteral("address")) },
        { QStringLiteral("station_address"), st.value(QStringLiteral("address")) },
        { QStringLiteral("parking_fee"), st.value(QStringLiteral("parking_fee")) },
        { QStringLiteral("service_fee"), st.value(QStringLiteral("service_fee")) },
        { QStringLiteral("reserve_type"), reserveType },
        { QStringLiteral("expect_time"), expectTime },
        { QStringLiteral("reserved_time"), now },
        { QStringLiteral("vehicle_id"), vehicleId },
        { QStringLiteral("vehicle_name"),
          vehicle.value(QStringLiteral("name"), QStringLiteral("默认车辆")) },
        { QStringLiteral("battery_kwh"),
          vehicle.value(QStringLiteral("battery_kwh"), 60) },
        { QStringLiteral("target_soc"), qBound(30, targetSoc, 100) },
        { QStringLiteral("start_soc"), kStartSocDefault },
        { QStringLiteral("end_soc"), kStartSocDefault },
        { QStringLiteral("speed"), speed },
        { QStringLiteral("order_id"), m_orderSeq },
        { QStringLiteral("credit_score"), 100 },
        { QStringLiteral("occupied"), false },
        { QStringLiteral("penalty_fee"), 0 },
        { QStringLiteral("occupy_min"), 0 },
        { QStringLiteral("occupy_fee"), 0 },
    };

    // 单价依赖上面刚写入的 station_id/service_fee，须在整表赋值后再算
    m_flow.insert(QStringLiteral("unit_price"), currentUnitPrice());

    // 级联鉴权：未完成订单 / 冻结 由服务端校验（示例阶段在此模拟拦截）
    // 接线：在线 → reservation.join（服务端排队/匹配）；离线 → 本地 mock
    // 先把阶段重置为 idle：在线 join 响应是异步的（handleJoinMatched/Queued 只认 idle/queued），
    // 若上一次流程停在 cancelled/done，不重置会导致预约结果被静默丢弃。
    setPhase(QStringLiteral("idle"));
    if (m_backendOnline && m_sendBackend) {
        m_sendBackend(QStringLiteral("reservation.join"), S({{"station_id", stationId}}));
    } else {
        mockReserve(speed);
    }
}

// —— mock 服务端：reservation.join ——
void ChargingFlow::mockReserve(const QString& speed) {
    if (mockTryMatch(speed)) {
        // 有空位 → 直接匹配：scan_pending（扫码页会拿到目标桩码）
        return;
    }
    // 无空位 → 进入排队（队列安排由服务端 mock 推进）
    m_flow.insert(QStringLiteral("queue_no"), m_queueOrdinal);
    m_flow.insert(QStringLiteral("estimate_wait_min"), m_queueOrdinal * 6);
    setPhase(QStringLiteral("queued"));
    m_queueTimer.start(kQueueAdvanceSec * 1000);
}

// —— mock 服务端：队列轮候推进（此刻轮到本用户，模拟空出空闲桩）——
void ChargingFlow::mockAdvanceQueue() {
    m_queueTimer.stop();
    if (m_phase != QStringLiteral("queued"))
        return;
    const int stationId = m_flow.value(QStringLiteral("station_id")).toInt();
    QVariantList chargers = m_explore ? m_explore->chargersForStation(stationId) : QVariantList();
    QVariantMap chosen;
    for (const QVariant& v : chargers) {
        const QVariantMap c = v.toMap();
        if (c.value(QStringLiteral("status")).toString() == QStringLiteral("idle")) {
            chosen = c;
            break;
        }
    }
    // 演示兜底：若判定瞬间无空闲，强制取第一根（真实场景由服务端按顺序安排）
    if (chosen.isEmpty() && chargers.length() > 0)
        chosen = chargers.first().toMap();
    if (chosen.isEmpty())
        return;

    mockAssignCharger(chosen);
    emit reservationReady(QStringLiteral("您预约的桩已就绪（%1 号），请尽快前往扫码启动")
                              .arg(m_flow.value(QStringLiteral("charger_code")).toString()));
    emit abnormal(QStringLiteral("轮到您了"),
                  QStringLiteral("已为您匹配 ") + m_flow.value(QStringLiteral("charger_code")).toString());
}

// —— mock 服务端：预约/排队的空闲桩匹配 ——
bool ChargingFlow::mockTryMatch(const QString& speed) {
    const int stationId = m_flow.value(QStringLiteral("station_id")).toInt();
    const QVariantList chargers = m_explore ? m_explore->chargersForStation(stationId) : QVariantList();

    int fallbackId = -1;
    for (const QVariant& v : chargers) {
        const QVariantMap c = v.toMap();
        if (c.value(QStringLiteral("status")).toString() != QStringLiteral("idle"))
            continue;
        if (c.value(QStringLiteral("type")).toString() == speed) {
            mockAssignCharger(c);
            return true;
        }
        if (fallbackId < 0)
            fallbackId = c.value(QStringLiteral("id")).toInt(); // 记录同速度无关的空闲桩下限
    }
    // 无同速度空闲桩：退回任意空闲桩
    for (const QVariant& v : chargers) {
        const QVariantMap c = v.toMap();
        if (c.value(QStringLiteral("status")).toString() == QStringLiteral("idle")) {
            mockAssignCharger(c);
            return true;
        }
    }
    return false;
}

// —— mock 服务端：选定空闲桩，进入 scan_pending ——
void ChargingFlow::mockAssignCharger(const QVariantMap& c) {
    QString now = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    QString deadline = QDateTime::currentDateTime()
                            .addSecs(kScanWindowSec)
                            .toString(QStringLiteral("HH:mm"));

    m_flow.insert(QStringLiteral("charger_id"), c.value(QStringLiteral("id")));
    m_flow.insert(QStringLiteral("charger_code"), c.value(QStringLiteral("code")));
    m_flow.insert(QStringLiteral("charger_type"), c.value(QStringLiteral("type")));
    m_flow.insert(QStringLiteral("charger_power"), c.value(QStringLiteral("power"), 60));
    m_flow.insert(QStringLiteral("scan_deadline"), deadline);
    m_flow.insert(QStringLiteral("reserved_time"), now);

    setPhase(QStringLiteral("scan_pending"));
    mockStartScanDeadline();
}

// —— mock 服务端：扫码截止计时 ——
void ChargingFlow::mockStartScanDeadline() {
    m_flow.insert(QStringLiteral("scan_remaining_sec"), kScanWindowSec);
    m_scanTimer.start(kScanWindowSec * 1000);
    m_scanTickTimer.start(1000);
}

void ChargingFlow::mockScanTick() {
    const int left = m_flow.value(QStringLiteral("scan_remaining_sec")).toInt() - 1;
    if (left <= 0)
        return; // 走到 0 由 mockScanTimeout 统一收尾
    m_flow.insert(QStringLiteral("scan_remaining_sec"), left);
    emit stateChanged();
}

void ChargingFlow::mockScanTimeout() {
    mockApplyPenalty(kNoShowPenalty, kNoShowCreditLoss, QStringLiteral("no_show"));
    setPhase(QStringLiteral("cancelled"));
    emit abnormal(QStringLiteral("预约已超时未扫码"),
                  QStringLiteral("桩已释放，产生违约金 ¥5、信用分 -5"));
}

// —— 取消预约/排队：reservation.cancel / order.cancel ——
void ChargingFlow::cancel() {
    m_queueTimer.stop();
    m_scanTimer.stop();
    m_scanTickTimer.stop();
    QString reason;
    if (m_phase == QStringLiteral("queued")) {
        reason = QStringLiteral("已退出排队");
    } else if (m_phase == QStringLiteral("scan_pending")) {
        const int remaining = m_flow.value(QStringLiteral("scan_remaining_sec")).toInt();
        m_flow.insert(QStringLiteral("cancel_reason"), QStringLiteral("user_cancel"));
        // 主动取消：距扫码截止尚有较多时间免费；临近截止再取消需付违约金 + 扣信用（服务端无对应计费，保留本地演示）
        if (remaining <= kCancelFreeSec) {
            mockApplyPenalty(kCancelLatePenalty, kCancelLateCreditLoss,
                             QStringLiteral("user_cancel_late"));
            reason = QStringLiteral("临近扫码截止取消，产生违约金 ¥%1、信用分 -%2")
                         .arg(kCancelLatePenalty, 0, 'f', 2)
                         .arg(kCancelLateCreditLoss);
        } else {
            reason = QStringLiteral("已取消预约，桩已释放（免费）");
        }
    }
    // 接线：在线时同步通知服务端释放预约/取消订单（本地立即退出，响应不回写 UI）
    if (m_backendOnline && m_sendBackend) {
        if ((m_phase == QStringLiteral("queued") || m_phase == QStringLiteral("scan_pending"))
            && m_reservationId > 0) {
            m_sendBackend(QStringLiteral("reservation.cancel"),
                          S({{"reservation_id", m_reservationId}}));
        } else if ((m_phase == QStringLiteral("charging") || m_phase == QStringLiteral("settle"))
                   && m_orderId > 0) {
            m_sendBackend(QStringLiteral("order.cancel"), S({{"order_id", m_orderId}}));
        }
    }
    setPhase(QStringLiteral("cancelled"));
    emit abnormal(QStringLiteral("已取消预约"), reason);
}

// —— 扫码确认：校验一致性后 order.create → order.start（在线） / 本地直接充电（离线）——
bool ChargingFlow::confirmScan(int stationId, const QString& code) {
    if (m_phase != QStringLiteral("scan_pending"))
        return false;
    if (m_flow.value(QStringLiteral("station_id")).toInt() != stationId) {
        emit abnormal(QStringLiteral("电站不匹配"), QStringLiteral("请前往预约电站的分配桩处扫码"));
        return false;
    }
    if (m_flow.value(QStringLiteral("charger_code")).toString() != code) {
        emit abnormal(QStringLiteral("桩码不匹配"),
                      QStringLiteral("请扫描分配桩 ") + m_flow.value(QStringLiteral("charger_code")).toString());
        return false;
    }
    if (m_backendOnline && m_sendBackend) {
        // order.create → 拿到 order_id 后再 order.start（handleOrderCreated 里续发）
        m_sendBackend(QStringLiteral("order.create"), S({
            {"station_id", m_flow.value(QStringLiteral("station_id"))},
            {"charger_id", m_flow.value(QStringLiteral("charger_id"))},
        }));
        return true;
    }
    mockStartCharging();
    return true;
}

// —— mock 服务端：order.start ——
void ChargingFlow::mockStartCharging() {
    m_scanTimer.stop();
    m_scanTickTimer.stop();
    m_flow.insert(QStringLiteral("soc"), kStartSocDefault);
    m_flow.insert(QStringLiteral("power_kw"), 0.0);
    m_flow.insert(QStringLiteral("energy_kwh"), 0.0);
    m_flow.insert(QStringLiteral("duration_min"), 0.0);
    m_flow.insert(QStringLiteral("cost"), 0.0);
    m_flow.insert(QStringLiteral("occupied"), true);
    setPhase(QStringLiteral("charging"));
    m_progressTimer.start(400);
}

void ChargingFlow::finishCharging() {
    if (m_backendOnline && m_sendBackend && m_orderId > 0) {
        // order.finish：优先把客户端当前 SOC 带给服务端；服务端仍会用仿真值/时长兜底。
        m_sendBackend(QStringLiteral("order.finish"), S({
            {"order_id", m_orderId},
            {"end_soc", m_flow.value(QStringLiteral("soc"), -1.0)},
        }));
        return;
    }
    mockFinishCharging(true);
}

void ChargingFlow::mockFinishCharging(bool userEnded) {
    if (m_phase != QStringLiteral("charging"))
        return;
    m_progressTimer.stop();

    double target = m_flow.value(QStringLiteral("target_soc")).toDouble();
    if (!userEnded)
        m_flow.insert(QStringLiteral("soc"), target);

    double start = m_flow.value(QStringLiteral("start_soc")).toDouble();
    m_flow.insert(QStringLiteral("end_soc"), m_flow.value(QStringLiteral("soc")));

    double energy = (m_flow.value(QStringLiteral("soc")).toDouble() - start) / 100.0
                    * m_flow.value(QStringLiteral("battery_kwh"), 60).toDouble();
    m_flow.insert(QStringLiteral("energy_kwh"), energy);
    m_flow.insert(QStringLiteral("amount"), energy * m_flow.value(QStringLiteral("unit_price")).toDouble());

    setPhase(QStringLiteral("settle"));
    mockStartOccupy();
}

// —— mock 服务端：充电进度推进（push.order_progress / charging_measure）——
void ChargingFlow::mockProgressTick() {
    double power = m_flow.value(QStringLiteral("charger_power"), 60).toDouble();
    double soc = m_flow.value(QStringLiteral("soc")).toDouble();
    // 两段式曲线：0–80% 满功率，80% 后功率下降
    if (soc >= 80.0)
        power *= 0.45;

    double battery = m_flow.value(QStringLiteral("battery_kwh"), 60).toDouble();
    double dkWh = power * 0.4 / 3600.0;
    double target = m_flow.value(QStringLiteral("target_soc")).toDouble();

    double nextSoc = soc + dkWh / battery * 100.0;
    nextSoc = qMin(nextSoc, 100.0);

    double durMin = m_flow.value(QStringLiteral("duration_min")).toDouble() + 0.4 / 60.0;
    m_flow.insert(QStringLiteral("duration_min"), durMin);
    m_flow.insert(QStringLiteral("power_kw"), power);
    m_flow.insert(QStringLiteral("soc"), nextSoc);
    m_flow.insert(QStringLiteral("energy_kwh"),
                  (nextSoc - 20.0) / 100.0 * battery);
    m_flow.insert(QStringLiteral("cost"),
                  m_flow.value(QStringLiteral("energy_kwh")).toDouble()
                  * m_flow.value(QStringLiteral("unit_price")).toDouble());

    if (nextSoc >= target) {
        mockFinishCharging(false);
        emit abnormal(QStringLiteral("充电完成"),
                      QStringLiteral("车辆已达到目标电量，请及时挪车，避免占位费"));
        return;
    }
    emit stateChanged();
}

// —— mock 服务端：充满未结算 → 占位计时 ——
void ChargingFlow::mockStartOccupy() {
    m_flow.insert(QStringLiteral("occupy_min"), 0);
    m_flow.insert(QStringLiteral("occupy_fee"), 0.0);
    m_occupyTimer.start(1000);
}

void ChargingFlow::mockOccupyTick() {
    int occSec = m_flow.value(QStringLiteral("occupy_min")).toInt() + 1;
    m_flow.insert(QStringLiteral("occupy_min"), occSec);
    double parkingFee = m_flow.value(QStringLiteral("parking_fee"), 0.0).toDouble();
    // 宽限期内不计费，超期按停车费（元/小时）按时长小时数累计
    double fee = 0.0;
    if (occSec > kOccupyGraceSec)
        fee = parkingFee * (occSec - kOccupyGraceSec) / 3600.0;
    m_flow.insert(QStringLiteral("occupy_fee"), fee);
    if (parkingFee > 0 && occSec == kOccupyGraceSec + 1)
        emit abnormal(QStringLiteral("结束长时间占用"),
                      QStringLiteral("已产生占位费，请尽快挪车"));
    emit stateChanged();
}

// —— 结算：order.settle（在线） / 本地 mock（离线）——
void ChargingFlow::settle(int couponId) {
    m_occupyTimer.stop();
    if (m_backendOnline && m_sendBackend && m_orderId > 0) {
        // 服务端扣款并回写 balance/points；券/积分抵扣由服务端口径为准（本页选中券仅本地展示）
        m_sendBackend(QStringLiteral("order.settle"), S({{"order_id", m_orderId}}));
        return;
    }
    double amount = m_flow.value(QStringLiteral("amount")).toDouble();
    double discount = 0.0;
    QString couponLabel;
    if (couponId > 0 && m_user) {
        for (const QVariant& v : m_user->coupons()) {
            const QVariantMap c = v.toMap();
            if (c.value(QStringLiteral("id")).toInt() == couponId) {
                discount = c.value(QStringLiteral("discount_amount"), 0.0).toDouble();
                couponLabel = c.value(QStringLiteral("title")).toString();
                break;
            }
        }
    }
    double occupyFee = m_flow.value(QStringLiteral("occupy_fee")).toDouble();
    double pay = qMax(0.0, amount - discount);
    double total = pay + occupyFee;   // 占位费随结算一起扣

    m_flow.insert(QStringLiteral("discount_amount"), discount);
    m_flow.insert(QStringLiteral("coupon_label"), couponLabel);
    m_flow.insert(QStringLiteral("pay_amount"), pay);
    m_flow.insert(QStringLiteral("occupy_fee"), occupyFee);
    m_flow.insert(QStringLiteral("points_earned"), static_cast<int>(pay)); // 基础 1 积分/元
    m_flow.insert(QStringLiteral("settle_time"),
                  QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));

    // 扣余额（未来由服务端扣账，这里走 UserData.mock）
    if (m_user) {
        double bal = m_user->profile().value(QStringLiteral("balance"), 0.0).toDouble();
        if (bal < total) {
            setPhase(QStringLiteral("settle"));
            emit abnormal(QStringLiteral("余额不足"),
                          QStringLiteral("还差 ¥") + QString::number(total - bal, 'f', 2)
                          + QStringLiteral("，请先充值后结算"));
            return;
        }
        m_user->recharge(-total);
        m_flow.insert(QStringLiteral("balance_after"), bal - total);
    }
    // 离线结算：把本单回写为已完成订单（在线由 order.settle_resp → ingestOrder 处理，这里同样走 ingestOrder
    // 触发 ordersChanged，让订单页「待支付」账单及时消失）
    if (m_user) {
        QVariantMap o;
        o.insert(QStringLiteral("id"), m_orderId > 0 ? m_orderId
                                                     : m_flow.value(QStringLiteral("order_id")));
        o.insert(QStringLiteral("status"), QStringLiteral("completed"));
        o.insert(QStringLiteral("station_id"), m_flow.value(QStringLiteral("station_id")));
        o.insert(QStringLiteral("station_name"), m_flow.value(QStringLiteral("station_name")));
        o.insert(QStringLiteral("station_address"), m_flow.value(QStringLiteral("station_address")));
        o.insert(QStringLiteral("charger_code"), m_flow.value(QStringLiteral("charger_code")));
        o.insert(QStringLiteral("charger_type"), m_flow.value(QStringLiteral("charger_type")));
        o.insert(QStringLiteral("start_soc"), m_flow.value(QStringLiteral("start_soc")));
        o.insert(QStringLiteral("end_soc"), m_flow.value(QStringLiteral("end_soc")));
        o.insert(QStringLiteral("target_soc"), m_flow.value(QStringLiteral("target_soc")));
        o.insert(QStringLiteral("energy_kwh"), m_flow.value(QStringLiteral("energy_kwh")));
        o.insert(QStringLiteral("duration_min"), m_flow.value(QStringLiteral("duration_min")));
        o.insert(QStringLiteral("amount"), m_flow.value(QStringLiteral("amount")));
        o.insert(QStringLiteral("discount_amount"), m_flow.value(QStringLiteral("discount_amount")));
        o.insert(QStringLiteral("pay_amount"), m_flow.value(QStringLiteral("pay_amount")));
        o.insert(QStringLiteral("points_earned"), m_flow.value(QStringLiteral("points_earned")));
        o.insert(QStringLiteral("start_time"), m_flow.value(QStringLiteral("reserved_time")));
        o.insert(QStringLiteral("settle_time"), m_flow.value(QStringLiteral("settle_time")));
        m_user->ingestOrder(o);
    }
    setPhase(QStringLiteral("done"));
    emit stateChanged();
}

// —— mock 服务端：违约金 + 信用分 ——
void ChargingFlow::mockApplyPenalty(qreal penalty, int creditLoss, const QString& reason) {
    m_flow.insert(QStringLiteral("penalty_fee"), penalty);
    m_flow.insert(QStringLiteral("penalty_reason"), reason);
    m_flow.insert(QStringLiteral("credit_score"),
                  m_flow.value(QStringLiteral("credit_score"), 100).toInt() - creditLoss);
    if (m_user && penalty > 0)
        m_user->recharge(-penalty);
}

// —— 充电中途拔枪：转结算 ——
void ChargingFlow::simulatePeel() {
    if (m_phase != QStringLiteral("charging"))
        return;
    if (m_backendOnline && m_sendBackend && m_orderId > 0) {
        m_sendBackend(QStringLiteral("order.finish"), S({{"order_id", m_orderId}}));
        emit abnormal(QStringLiteral("充电中断"),
                      QStringLiteral("检测到拔枪，按当前已充量转入结算"));
        return;
    }
    m_progressTimer.stop();
    mockFinishCharging(true);
    emit abnormal(QStringLiteral("充电中断"),
                  QStringLiteral("检测到拔枪，按当前已充量转入结算"));
}

void ChargingFlow::simulateScanTimeout() {
    m_scanTimer.stop();
    mockScanTimeout();
}

void ChargingFlow::reset() {
    m_queueTimer.stop();
    m_scanTimer.stop();
    m_scanTickTimer.stop();
    m_progressTimer.stop();
    m_occupyTimer.stop();
    m_flow.insert(QStringLiteral("occupied"), false);
    setPhase(QStringLiteral("idle"));
}

// 当前进行中行程（供首页「进行中」栏合并展示）。
// 覆盖 all 活跃态：queued(排队中) / scan_pending(已预约待扫码) / charging(充电中) / settle(待结算)。
// 让用户从首页任一返回入口都能回到对应页；充电/结算分支沿用原有展示键。
// 键对齐 UserData 的 charging_order 展示字段（station_name/charger_code/type/soc/unit_price 等）。
QVariantMap ChargingFlow::currentOrder() const {
    if (m_phase != QStringLiteral("queued")
        && m_phase != QStringLiteral("scan_pending")
        && m_phase != QStringLiteral("charging")
        && m_phase != QStringLiteral("settle"))
        return {};

    QVariantMap o;
    o.insert(QStringLiteral("live"), true);                                            // 标识：来自实时流程
    o.insert(QStringLiteral("id"), -m_flow.value(QStringLiteral("order_id")).toInt()); // 负号避开种子订单 id
    o.insert(QStringLiteral("station_id"), m_flow.value(QStringLiteral("station_id")));
    o.insert(QStringLiteral("station_name"), m_flow.value(QStringLiteral("station_name")));
    o.insert(QStringLiteral("station_address"), m_flow.value(QStringLiteral("station_address")));
    o.insert(QStringLiteral("vehicle_name"), m_flow.value(QStringLiteral("vehicle_name")));
    o.insert(QStringLiteral("unit_price"), m_flow.value(QStringLiteral("unit_price")));
    o.insert(QStringLiteral("reserve_type"), m_flow.value(QStringLiteral("reserve_type")));
    o.insert(QStringLiteral("start_time"), m_flow.value(QStringLiteral("reserved_time")));

    if (m_phase == QStringLiteral("queued")) {
        o.insert(QStringLiteral("status"), QStringLiteral("queued"));
        o.insert(QStringLiteral("queue_no"), m_flow.value(QStringLiteral("queue_no")));
        o.insert(QStringLiteral("estimate_wait_min"), m_flow.value(QStringLiteral("estimate_wait_min")));
    } else if (m_phase == QStringLiteral("scan_pending")) {
        o.insert(QStringLiteral("status"), QStringLiteral("reserved"));
        o.insert(QStringLiteral("charger_code"), m_flow.value(QStringLiteral("charger_code")));
        o.insert(QStringLiteral("charger_type"), m_flow.value(QStringLiteral("charger_type")));
        o.insert(QStringLiteral("scan_deadline"), m_flow.value(QStringLiteral("scan_deadline")));
        o.insert(QStringLiteral("scan_remaining_sec"), m_flow.value(QStringLiteral("scan_remaining_sec")));
        o.insert(QStringLiteral("start_soc"), m_flow.value(QStringLiteral("start_soc")));
        o.insert(QStringLiteral("target_soc"), m_flow.value(QStringLiteral("target_soc")));
        return o;
    } else {
        const bool charging = (m_phase == QStringLiteral("charging"));
        o.insert(QStringLiteral("status"), charging ? QStringLiteral("charging")
                                                    : QStringLiteral("pending_settle"));
        o.insert(QStringLiteral("charger_code"), m_flow.value(QStringLiteral("charger_code")));
        o.insert(QStringLiteral("charger_type"), m_flow.value(QStringLiteral("charger_type")));
        o.insert(QStringLiteral("start_soc"), m_flow.value(QStringLiteral("start_soc")));
        o.insert(QStringLiteral("target_soc"), m_flow.value(QStringLiteral("target_soc")));
        o.insert(QStringLiteral("end_soc"), m_flow.value(QStringLiteral("end_soc")));
        o.insert(QStringLiteral("soc"), charging ? m_flow.value(QStringLiteral("soc"))
                                                 : m_flow.value(QStringLiteral("end_soc")));
        o.insert(QStringLiteral("energy_kwh"), m_flow.value(QStringLiteral("energy_kwh")));
        o.insert(QStringLiteral("cost"), m_flow.value(QStringLiteral("cost")));
        o.insert(QStringLiteral("amount"), m_flow.value(QStringLiteral("amount"), m_flow.value(QStringLiteral("cost"))));
        o.insert(QStringLiteral("pay_amount"), m_flow.value(QStringLiteral("pay_amount"),
                                                            m_flow.value(QStringLiteral("amount"),
                                                                         m_flow.value(QStringLiteral("cost")))));
        o.insert(QStringLiteral("duration_min"), m_flow.value(QStringLiteral("duration_min")));
    }
    return o;
}

double ChargingFlow::currentUnitPrice() const {
    const int stationId = m_flow.value(QStringLiteral("station_id")).toInt();
    double service = m_flow.value(QStringLiteral("service_fee"), 0.0).toDouble();
    QVariantList rules = m_explore ? m_explore->priceRulesForStation(stationId) : QVariantList();

    QTime now = QTime::currentTime();
    double fallback = 0.8;
    for (const QVariant& v : rules) {
        const QVariantMap r = v.toMap();
        const QString range = r.value(QStringLiteral("time_range")).toString(); // "HH:MM–HH:MM"
        QStringList parts = range.split(QStringLiteral("–"));
        if (parts.size() != 2)
            continue;
        QTime from = QTime::fromString(parts[0].trimmed(), QStringLiteral("HH:mm"));
        QTime to = QTime::fromString(parts[1].trimmed(), QStringLiteral("HH:mm"));
        if (!from.isValid() || !to.isValid())
            continue;
        bool inRange = (from <= to) ? (now >= from && now <= to)
                                    : (now >= from || now <= to); // 跨天时段
        if (inRange) {
            fallback = r.value(QStringLiteral("price"), fallback).toDouble();
            break;
        }
    }
    return fallback + service;
}

// ==================== 接线：服务端响应 / 推送路由 ====================

void ChargingFlow::onBackendMessage(const QString& type, int code, const QString& message,
                                    const QVariantMap& payload) {
    // 服务端业务错误：统一走 abnormal 提示（如 2002 余额不足 / 3002 桩不可用）
    if (code != 0) {
        if (type == QStringLiteral("order.create_resp")
            || type == QStringLiteral("order.start_resp")
            || type == QStringLiteral("order.finish_resp")
            || type == QStringLiteral("order.settle_resp")
            || type == QStringLiteral("reservation.join_resp")
            || type == QStringLiteral("reservation.cancel_resp")) {
            backToMockFallback(message);
        }
        return;
    }

    if (type == QStringLiteral("reservation.join_resp")) {
        const bool matched = payload.value(QStringLiteral("matched")).toBool();
        const QVariantMap res = payload.value(QStringLiteral("reservation")).toMap();
        if (matched)
            handleJoinMatched(res);
        else
            handleJoinQueued(res, payload.value(QStringLiteral("queue")).toMap());
    } else if (type == QStringLiteral("push.reservation_notify")) {
        handleReservationNotify(payload);
    } else if (type == QStringLiteral("order.create_resp")) {
        const QVariantMap o = payload.value(QStringLiteral("order")).toMap();
        handleOrderCreated(o.value(QStringLiteral("id")).toInt());
    } else if (type == QStringLiteral("order.start_resp")) {
        handleOrderStarted(payload.value(QStringLiteral("order")).toMap());
    } else if (type == QStringLiteral("order.finish_resp")) {
        handleOrderFinished(payload.value(QStringLiteral("order")).toMap());
    } else if (type == QStringLiteral("order.settle_resp")) {
        QVariantMap extra;
        extra.insert(QStringLiteral("points"), payload.value(QStringLiteral("points")));
        extra.insert(QStringLiteral("balance"), payload.value(QStringLiteral("balance")));
        extra.insert(QStringLiteral("total_points"), payload.value(QStringLiteral("total_points")));
        handleOrderSettled(payload.value(QStringLiteral("order")).toMap(), extra);
    } else if (type == QStringLiteral("push.order_progress")) {
        handleOrderProgress(payload);
    }
    // order.cancel_resp / reservation.cancel_resp：本地已置 cancelled，无需回写
}

// 服务端返回错误时的统一出口：提示后保持原阶段（不吞掉错误）
void ChargingFlow::backToMockFallback(const QString& msg) {
    emit abnormal(QStringLiteral("服务端返回错误"), msg);
}

// —— reservation.join_resp：有空桩直接匹配 → 扫码待启动 ——
void ChargingFlow::handleJoinMatched(const QVariantMap& res) {
    if (m_phase != QStringLiteral("idle") && m_phase != QStringLiteral("queued"))
        return;
    m_reservationId = res.value(QStringLiteral("id")).toInt();
    const int stationId = m_flow.value(QStringLiteral("station_id")).toInt();
    QVariantMap chosen;
    if (m_explore) {
        const QVariantList chargers = m_explore->chargersForStation(stationId);
        const int cid = res.value(QStringLiteral("charger_id")).toInt();
        for (const QVariant& v : chargers) {
            const QVariantMap c = v.toMap();
            if (c.value(QStringLiteral("id")).toInt() == cid) {
                chosen = c;
                break;
            }
        }
    }
    m_flow.insert(QStringLiteral("charger_id"), res.value(QStringLiteral("charger_id")));
    m_flow.insert(QStringLiteral("charger_code"), res.value(QStringLiteral("charger_code")));
    m_flow.insert(QStringLiteral("charger_type"),
                  chosen.value(QStringLiteral("type"), m_flow.value(QStringLiteral("speed"))));
    m_flow.insert(QStringLiteral("charger_power"),
                  chosen.value(QStringLiteral("power"), 60));
    m_flow.insert(QStringLiteral("reserved_time"),
                  QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    setPhase(QStringLiteral("scan_pending"));
    mockStartScanDeadline();   // 服务端无超时推送，扫码窗口保留本地倒计时
}

// —— reservation.join_resp：无空桩 → 排队 ——
void ChargingFlow::handleJoinQueued(const QVariantMap& res, const QVariantMap& queue) {
    m_reservationId = res.value(QStringLiteral("id")).toInt();
    m_flow.insert(QStringLiteral("queue_no"),
                  queue.value(QStringLiteral("queue_no"), res.value(QStringLiteral("queue_no"))));
    int waitMin = queue.value(QStringLiteral("ahead_count"), 0).toInt() * 6;
    const QDateTime eta = QDateTime::fromString(
        queue.value(QStringLiteral("estimate_start_time")).toString(),
        QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (eta.isValid())
        waitMin = qMax(1, static_cast<int>(QDateTime::currentDateTime().secsTo(eta) / 60.0 + 0.999));
    m_flow.insert(QStringLiteral("estimate_wait_min"), waitMin);
    setPhase(QStringLiteral("queued"));
    // 服务端负责叫号（push.reservation_notify），本地不再自行推进队列
}

// —— push.reservation_notify：轮到你了 → 分配桩号 → 扫码待启动 ——
void ChargingFlow::handleReservationNotify(const QVariantMap& payload) {
    const QVariantMap res = payload.value(QStringLiteral("reservation")).toMap();
    if (m_phase != QStringLiteral("queued") && m_phase != QStringLiteral("idle"))
        return;
    m_reservationId = res.value(QStringLiteral("id"), m_reservationId).toInt();
    QVariantMap chosen;
    if (m_explore) {
        const int cid = payload.value(QStringLiteral("charger_id"), res.value(QStringLiteral("charger_id"))).toInt();
        const QVariantList chargers = m_explore->chargersForStation(
            payload.value(QStringLiteral("station_id"), m_flow.value(QStringLiteral("station_id"))).toInt());
        for (const QVariant& v : chargers) {
            const QVariantMap c = v.toMap();
            if (c.value(QStringLiteral("id")).toInt() == cid) {
                chosen = c;
                break;
            }
        }
    }
    m_flow.insert(QStringLiteral("charger_id"),
                  payload.value(QStringLiteral("charger_id"), res.value(QStringLiteral("charger_id"))));
    m_flow.insert(QStringLiteral("charger_code"), res.value(QStringLiteral("charger_code")));
    m_flow.insert(QStringLiteral("charger_type"),
                  chosen.value(QStringLiteral("type"), m_flow.value(QStringLiteral("speed"))));
    m_flow.insert(QStringLiteral("charger_power"), chosen.value(QStringLiteral("power"), 60));
    m_flow.insert(QStringLiteral("reserved_time"),
                  QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    setPhase(QStringLiteral("scan_pending"));
    mockStartScanDeadline();
    emit reservationReady(payload.value(QStringLiteral("message")).toString());
    emit abnormal(QStringLiteral("轮到您了"),
                  QStringLiteral("已为您匹配 ") + m_flow.value(QStringLiteral("charger_code")).toString());
}

// —— order.create_resp：拿 order_id → 续发 order.start ——
void ChargingFlow::handleOrderCreated(int orderId) {
    if (orderId <= 0)
        return;
    m_orderId = orderId;
    m_flow.insert(QStringLiteral("order_id"), orderId);
    if (m_sendBackend)
        m_sendBackend(QStringLiteral("order.start"), S({
            {"order_id", orderId},
            {"start_soc", kStartSocDefault},
            {"target_soc", m_flow.value(QStringLiteral("target_soc"), 100)},
        }));
}

// —— order.start_resp：开始充电（进度由 push.order_progress 驱动，不启动本地进度定时器）——
void ChargingFlow::handleOrderStarted(const QVariantMap& order) {
    m_scanTimer.stop();
    m_scanTickTimer.stop();
    m_orderId = order.value(QStringLiteral("id"), m_orderId).toInt();
    m_chargedAt = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    applyOrderToFlow(order);
    m_flow.insert(QStringLiteral("soc"), order.value(QStringLiteral("start_soc"), kStartSocDefault));
    m_flow.insert(QStringLiteral("power_kw"), 0.0);
    m_flow.insert(QStringLiteral("energy_kwh"), 0.0);
    m_flow.insert(QStringLiteral("duration_min"), 0.0);
    m_flow.insert(QStringLiteral("cost"), 0.0);
    m_flow.insert(QStringLiteral("occupied"), true);
    setPhase(QStringLiteral("charging"));
}

// —— push.order_progress：充电进度（每秒） / 充满自动结束 ——
void ChargingFlow::handleOrderProgress(const QVariantMap& p) {
    if (m_orderId > 0 && p.value(QStringLiteral("order_id")).toInt() != m_orderId)
        return;   // 只认当前订单
    if (m_phase != QStringLiteral("charging"))
        return;
    const double soc = p.value(QStringLiteral("soc")).toDouble();
    m_flow.insert(QStringLiteral("soc"), soc);
    m_flow.insert(QStringLiteral("power_kw"), p.value(QStringLiteral("power_kw")));
    m_flow.insert(QStringLiteral("energy_kwh"), p.value(QStringLiteral("energy")));
    m_flow.insert(QStringLiteral("cost"), p.value(QStringLiteral("cost")));
    m_flow.insert(QStringLiteral("amount"), p.value(QStringLiteral("cost")));
    m_flow.insert(QStringLiteral("pay_amount"), p.value(QStringLiteral("cost")));
    // 服务端 push 无时长字段，本地按开始充电时间折算
    const QDateTime start = QDateTime::fromString(m_chargedAt, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (start.isValid())
        m_flow.insert(QStringLiteral("duration_min"),
                      start.msecsTo(QDateTime::currentDateTime()) / 60000.0);
    if (p.value(QStringLiteral("finished")).toBool()) {
        m_flow.insert(QStringLiteral("end_soc"), soc);
        setPhase(QStringLiteral("settle"));
        mockStartOccupy();
        emit abnormal(QStringLiteral("充电完成"),
                      QStringLiteral("车辆已达到目标电量，请及时挪车，避免占位费"));
        return;
    }
    emit stateChanged();
}

// —— order.finish_resp：结束充电 → 待结算 ——
void ChargingFlow::handleOrderFinished(const QVariantMap& order) {
    m_orderId = order.value(QStringLiteral("id"), m_orderId).toInt();
    applyOrderToFlow(order);
    // 结束电量以服务端结算值为准
    if (order.contains(QStringLiteral("end_soc")))
        m_flow.insert(QStringLiteral("soc"), order.value(QStringLiteral("end_soc")));
    if (!m_flow.contains(QStringLiteral("pay_amount")))
        m_flow.insert(QStringLiteral("pay_amount"), m_flow.value(QStringLiteral("amount"), 0.0));
    setPhase(QStringLiteral("settle"));
    mockStartOccupy();
}

// —— order.settle_resp：结算成功 ——
void ChargingFlow::handleOrderSettled(const QVariantMap& order, const QVariantMap& extra) {
    // 订单页直接结算历史账单（非当前流程订单）时，不回写流程状态；
    // 当前流程结算（ChargingFlow.settle → order.settle）的订单 id 一定等于 m_orderId
    if (m_orderId <= 0 || order.value(QStringLiteral("id")).toInt() != m_orderId)
        return;
    m_occupyTimer.stop();
    applyOrderToFlow(order);
    if (extra.contains(QStringLiteral("points")))
        m_flow.insert(QStringLiteral("points_earned"), extra.value(QStringLiteral("points")));
    if (extra.contains(QStringLiteral("balance")))
        m_flow.insert(QStringLiteral("balance_after"), extra.value(QStringLiteral("balance")));
    m_flow.insert(QStringLiteral("settle_time"),
                  order.value(QStringLiteral("settle_time"),
                              QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
    setPhase(QStringLiteral("done"));
}

// 服务端 order 字段回填 m_flow：只补空，不覆盖本地 mock 展示值（站名/桩码等）
void ChargingFlow::applyOrderToFlow(const QVariantMap& order) {
    if (order.isEmpty())
        return;
    m_orderId = order.value(QStringLiteral("id"), m_orderId).toInt();
    m_flow.insert(QStringLiteral("order_id"), m_orderId);
    const QStringList keys = {
        QStringLiteral("start_soc"), QStringLiteral("end_soc"), QStringLiteral("target_soc"),
        QStringLiteral("energy_kwh"), QStringLiteral("amount"), QStringLiteral("discount_amount"),
        QStringLiteral("pay_amount"), QStringLiteral("duration_min"), QStringLiteral("points_earned"),
        QStringLiteral("start_time"), QStringLiteral("end_time"), QStringLiteral("create_time"),
        QStringLiteral("settle_time"), QStringLiteral("price_level"),
    };
    const QStringList settlementKeys = {
        QStringLiteral("end_soc"), QStringLiteral("energy_kwh"), QStringLiteral("amount"),
        QStringLiteral("discount_amount"), QStringLiteral("pay_amount"),
        QStringLiteral("duration_min"), QStringLiteral("points_earned"),
        QStringLiteral("end_time"), QStringLiteral("settle_time"), QStringLiteral("price_level"),
    };
    for (const QString& k : keys) {
        if (order.contains(k) && (settlementKeys.contains(k) || !m_flow.contains(k)))
            m_flow.insert(k, order.value(k));
    }
    // 开始时间：充电/结算展示统一用服务端 start_time
    if (order.contains(QStringLiteral("start_time")))
        m_flow.insert(QStringLiteral("reserved_time"), order.value(QStringLiteral("start_time")));
}
