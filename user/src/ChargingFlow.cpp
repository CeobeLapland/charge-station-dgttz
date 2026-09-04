#include "ChargingFlow.h"

#include <QDateTime>
#include <QTime>

#include "ExploreData.h"
#include "UserData.h"

// —— 违约 / 流程阈值（spec-充电全流程 第4节建议初值，示例阶段可加速演示）——
namespace {
constexpr int    kScanWindowSec = 60;        // 扫码保留窗口（示例加快，真实建议 10 分钟）
constexpr int    kQueueAdvanceSec = 4;       // 排队轮到推进间隔（演示用，真实由服务端安排）
constexpr int    kOccupyGraceSec = 10;       // 占位宽限（示例加速，真实建议 10 分钟）
constexpr qreal  kNoShowPenalty = 5.0;       // no_show 违约金（元）
constexpr int    kNoShowCreditLoss = 5;      // no_show 信用分扣减
constexpr double kStartSocDefault = 20.0;    // 起始电量（示例无车辆实时电量，用默认值）
} // namespace

ChargingFlow::ChargingFlow(QObject* parent)
    : QObject(parent) {
    connect(&m_queueTimer, &QTimer::timeout, this, &ChargingFlow::mockAdvanceQueue);
    connect(&m_scanTimer, &QTimer::timeout, this, &ChargingFlow::mockScanTimeout);
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
    mockReserve(speed);
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
    m_scanTimer.start(kScanWindowSec * 1000);
}

void ChargingFlow::mockScanTimeout() {
    mockApplyPenalty(kNoShowPenalty, kNoShowCreditLoss, QStringLiteral("no_show"));
    setPhase(QStringLiteral("cancelled"));
    emit abnormal(QStringLiteral("预约已超时未扫码"),
                  QStringLiteral("桩已释放，产生违约金 ¥5、信用分 -5"));
}

// —— 取消预约/排队：reservation.cancel ——
void ChargingFlow::cancel() {
    m_queueTimer.stop();
    m_scanTimer.stop();
    QString reason;
    if (m_phase == QStringLiteral("queued")) {
        reason = QStringLiteral("已退出排队");
    } else if (m_phase == QStringLiteral("scan_pending")) {
        reason = QStringLiteral("已取消预约，桩已释放");
        m_flow.insert(QStringLiteral("cancel_reason"), QStringLiteral("user_cancel"));
    }
    setPhase(QStringLiteral("cancelled"));
    emit abnormal(QStringLiteral("预约已取消"), reason);
}

// —— 扫码确认：校验一致性后 order.start ——
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
    mockStartCharging();
    return true;
}

// —— mock 服务端：order.start ——
void ChargingFlow::mockStartCharging() {
    m_scanTimer.stop();
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

// —— 结算：order.settle ——
void ChargingFlow::settle(int couponId) {
    m_occupyTimer.stop();
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
    m_progressTimer.stop();
    m_occupyTimer.stop();
    m_flow.insert(QStringLiteral("occupied"), false);
    setPhase(QStringLiteral("idle"));
}

// 当前进行中订单（供首页「正在进行」栏合并展示）。
// 生命周期：开始充电 → charging；达目标/提前结束 → settle（待结算）→ 结算成功（done）后消失。
// 键对齐 UserData 的 charging_order 展示字段（station_name/charger_code/type/soc/unit_price 等）。
QVariantMap ChargingFlow::currentOrder() const {
    if (m_phase != QStringLiteral("charging") && m_phase != QStringLiteral("settle"))
        return {};
    const bool charging = (m_phase == QStringLiteral("charging"));
    QVariantMap o;
    o.insert(QStringLiteral("live"), true);                                            // 标识：来自实时流程
    o.insert(QStringLiteral("id"), -m_flow.value(QStringLiteral("order_id")).toInt()); // 负号避开种子订单 id
    o.insert(QStringLiteral("status"), charging ? QStringLiteral("charging")
                                               : QStringLiteral("pending_settle"));
    o.insert(QStringLiteral("station_id"), m_flow.value(QStringLiteral("station_id")));
    o.insert(QStringLiteral("station_name"), m_flow.value(QStringLiteral("station_name")));
    o.insert(QStringLiteral("station_address"), m_flow.value(QStringLiteral("station_address")));
    o.insert(QStringLiteral("charger_code"), m_flow.value(QStringLiteral("charger_code")));
    o.insert(QStringLiteral("charger_type"), m_flow.value(QStringLiteral("charger_type")));
    o.insert(QStringLiteral("start_soc"), m_flow.value(QStringLiteral("start_soc")));
    o.insert(QStringLiteral("target_soc"), m_flow.value(QStringLiteral("target_soc")));
    o.insert(QStringLiteral("end_soc"), m_flow.value(QStringLiteral("end_soc")));
    o.insert(QStringLiteral("soc"), charging ? m_flow.value(QStringLiteral("soc"))
                                             : m_flow.value(QStringLiteral("end_soc")));
    o.insert(QStringLiteral("vehicle_name"), m_flow.value(QStringLiteral("vehicle_name")));
    o.insert(QStringLiteral("unit_price"), m_flow.value(QStringLiteral("unit_price")));
    o.insert(QStringLiteral("reserve_type"), m_flow.value(QStringLiteral("reserve_type")));
    o.insert(QStringLiteral("start_time"), m_flow.value(QStringLiteral("reserved_time")));
    o.insert(QStringLiteral("energy_kwh"), m_flow.value(QStringLiteral("energy_kwh")));
    o.insert(QStringLiteral("cost"), m_flow.value(QStringLiteral("cost")));
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