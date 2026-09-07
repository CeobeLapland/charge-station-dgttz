#include "OrderDao.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTime>
#include <QVariant>
#include <cmath>

#include "StationDao.h"

namespace {

QString nowStr()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

void fail(OpError *err, int code, const QString &msg)
{
    if (err) { err->code = code; err->message = msg; }
}

// 分时电价档位(与 price_rule.time_range / make_seed.py 一致)
QString priceLevelOf(int hour)
{
    if (hour < 8)              return QStringLiteral("valley");
    if (hour >= 17 && hour < 21) return QStringLiteral("peak");
    return QStringLiteral("flat");
}

const char *kSelectOrder =
    "SELECT o.id, o.user_id, o.station_id, o.charger_id, IFNULL(o.vehicle_id,0), "
    "       s.name, c.code, o.status, "
    "       IFNULL(o.start_soc,0), IFNULL(o.target_soc,0), IFNULL(o.end_soc,0), "
    "       IFNULL(o.start_time,''), IFNULL(o.end_time,''), o.create_time, IFNULL(o.settle_time,''), "
    "       IFNULL(o.price_level,'flat'), o.duration_min, o.energy_kwh, "
    "       o.amount, o.discount_amount, o.pay_amount, o.points_used, o.points_earned "
    "FROM charging_order o JOIN station s ON s.id=o.station_id "
    "JOIN charger c ON c.id=o.charger_id ";

OrderView rowToOrder(const QSqlQuery &q)
{
    OrderView o;
    o.id             = q.value(0).toInt();
    o.userId         = q.value(1).toInt();
    o.stationId      = q.value(2).toInt();
    o.chargerId      = q.value(3).toInt();
    o.vehicleId      = q.value(4).toInt();
    o.stationName    = q.value(5).toString();
    o.chargerCode    = q.value(6).toString();
    o.status         = q.value(7).toString();
    o.startSoc       = q.value(8).toDouble();
    o.targetSoc      = q.value(9).toDouble();
    o.endSoc         = q.value(10).toDouble();
    o.startTime      = q.value(11).toString();
    o.endTime        = q.value(12).toString();
    o.createTime     = q.value(13).toString();
    o.settleTime     = q.value(14).toString();
    o.priceLevel     = q.value(15).toString();
    o.durationMin    = q.value(16).toInt();
    o.energyKwh      = q.value(17).toDouble();
    o.amount         = q.value(18).toDouble();
    o.discountAmount = q.value(19).toDouble();
    o.payAmount      = q.value(20).toDouble();
    o.pointsUsed     = q.value(21).toInt();
    o.pointsEarned   = q.value(22).toInt();
    return o;
}

// 按 id 取订单(不校验归属), 内部用
std::optional<OrderView> fetchOrder(int orderId)
{
    QSqlQuery q;
    q.prepare(QString::fromLatin1(kSelectOrder) + QStringLiteral("WHERE o.id = ?"));
    q.addBindValue(orderId);
    if (!q.exec() || !q.next()) return std::nullopt;
    return rowToOrder(q);
}

// 取订单并校验归属; 不存在或不是本人的一律当"不存在"
std::optional<OrderView> fetchOwnOrder(int userId, int orderId, OpError *err)
{
    const auto o = fetchOrder(orderId);
    if (!o || o->userId != userId) {
        fail(err, 4001, QStringLiteral("订单不存在: id=%1").arg(orderId));
        return std::nullopt;
    }
    return o;
}

void addTimeline(int orderId, const QString &node, const QString &label,
                 const QString &detail = QString())
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO order_timeline(order_id, node, label, event_time, detail) VALUES(?,?,?,?,?)"));
    q.addBindValue(orderId);
    q.addBindValue(node);
    q.addBindValue(label);
    q.addBindValue(nowStr());
    q.addBindValue(detail.isEmpty() ? QVariant() : QVariant(detail));
    q.exec();
}

void setChargerStatus(int chargerId, const QString &status)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE charger SET status = ? WHERE id = ?"));
    q.addBindValue(status);
    q.addBindValue(chargerId);
    q.exec();
}

double scalarOf(const QString &sql, const QVariantList &binds)
{
    QSqlQuery q;
    q.prepare(sql);
    for (const auto &b : binds) q.addBindValue(b);
    if (!q.exec() || !q.next()) return 0.0;
    return q.value(0).toDouble();
}

}  // namespace

namespace dao {

void updateChargerElectrics(int chargerId, double voltage, double current, double temperature)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE charger SET voltage=?, current=?, temperature=? WHERE id=?"));
    q.addBindValue(voltage);
    q.addBindValue(current);
    q.addBindValue(temperature);
    q.addBindValue(chargerId);
    q.exec();
}

void insertMeasure(int chargerId, int stationId, const QString &time,
                   double powerKw, double soc, double energyDelta, double temperature)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO charging_measure(charger_id, station_id, measure_time, power_kw, soc, "
        "  energy_delta_kwh, temperature) VALUES(?,?,?,?,?,?,?)"));
    q.addBindValue(chargerId);
    q.addBindValue(stationId);
    q.addBindValue(time);
    q.addBindValue(powerKw);
    q.addBindValue(soc);
    q.addBindValue(energyDelta);
    q.addBindValue(temperature);
    q.exec();
}

// ---- 服务端重启后, 把仍在 charging 的订单捞出来交还给仿真器 ----
// 不做这件事的话: 仿真状态只在内存里, 重启后没人再推进这些订单,
// 电桩永远停在 charging, 那根桩从此不能被任何人使用。
QList<ResumeOrder> ordersToResume()
{
    QList<ResumeOrder> out;
    QSqlQuery q;
    if (!q.exec(QStringLiteral(
            "SELECT id, user_id, charger_id, station_id, IFNULL(start_soc,20) "
            "FROM charging_order WHERE status='charging'")))
        return out;
    while (q.next()) {
        ResumeOrder r;
        r.orderId   = q.value(0).toInt();
        r.userId    = q.value(1).toInt();
        r.chargerId = q.value(2).toInt();
        r.stationId = q.value(3).toInt();
        r.startSoc  = q.value(4).toDouble();
        out.append(r);
    }
    return out;
}

// 恢复时把 start_time 重置为现在: 服务端重启 = 这次充电会话重新计时,
// 否则时长会把停机的那段时间也算进去。
void resetStartTime(int orderId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE charging_order SET start_time=? WHERE id=?"));
    q.addBindValue(nowStr());
    q.addBindValue(orderId);
    q.exec();
}

// ================== 预约 / 智能排队 ==================
namespace {

ReservationView rowToReservation(const QSqlQuery &q)
{
    ReservationView r;
    r.id                = q.value(0).toInt();
    r.userId            = q.value(1).toInt();
    r.stationId         = q.value(2).toInt();
    r.chargerId         = q.value(3).toInt();
    r.queueNo           = q.value(4).toInt();
    r.reserveTime       = q.value(5).toString();
    r.estimateStartTime = q.value(6).toString();
    r.status            = q.value(7).toString();
    r.stationName       = q.value(8).toString();
    r.chargerCode       = q.value(9).toString();
    return r;
}

const char *kSelectReservation =
    "SELECT r.id, r.user_id, r.station_id, IFNULL(r.charger_id,0), r.queue_no, "
    "       r.reserve_time, IFNULL(r.estimate_start_time,''), r.status, "
    "       s.name, IFNULL((SELECT code FROM charger c WHERE c.id=r.charger_id),'') "
    "FROM reservation r JOIN station s ON s.id=r.station_id ";

// 该站当前排在此人前面、还在等待的人数
int aheadOf(int stationId, int queueNo)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM reservation WHERE station_id=? AND status='waiting' AND queue_no<?"));
    q.addBindValue(stationId);
    q.addBindValue(queueNo);
    if (!q.exec() || !q.next()) return 0;
    return q.value(0).toInt();
}

std::optional<ReservationView> fetchReservation(int id)
{
    QSqlQuery q;
    q.prepare(QString::fromLatin1(kSelectReservation) + QStringLiteral("WHERE r.id=?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next()) return std::nullopt;
    ReservationView r = rowToReservation(q);
    r.aheadCount = (r.status == QStringLiteral("waiting")) ? aheadOf(r.stationId, r.queueNo) : 0;
    return r;
}

// 找该站一台空闲桩, 没有返回 0
int findIdleCharger(int stationId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT id FROM charger WHERE station_id=? AND status='idle' ORDER BY id LIMIT 1"));
    q.addBindValue(stationId);
    if (!q.exec() || !q.next()) return 0;
    return q.value(0).toInt();
}

}  // namespace

std::optional<ReservationView> joinQueue(int userId, int stationId, OpError *err)
{
    // 电站必须存在
    if (scalarOf(QStringLiteral("SELECT COUNT(*) FROM station WHERE id=?"), {stationId}) < 1) {
        fail(err, 4001, QStringLiteral("电站不存在: id=%1").arg(stationId));
        return std::nullopt;
    }
    // 幂等: 同一个人在同一个站重复加入, 直接返回已有的那条, 不重复排号
    QSqlQuery ex;
    ex.prepare(QStringLiteral(
        "SELECT id FROM reservation WHERE user_id=? AND station_id=? "
        "AND status IN ('waiting','matched') ORDER BY id DESC LIMIT 1"));
    ex.addBindValue(userId);
    ex.addBindValue(stationId);
    if (ex.exec() && ex.next())
        return fetchReservation(ex.value(0).toInt());

    const int idle = findIdleCharger(stationId);

    QSqlDatabase::database().transaction();
    QSqlQuery ins;
    if (idle > 0) {
        // 有空桩 → 直接匹配, 不用排队
        ins.prepare(QStringLiteral(
            "INSERT INTO reservation(user_id,station_id,charger_id,queue_no,reserve_time,"
            "  estimate_start_time,notified,status) VALUES(?,?,?,0,?,?,1,'matched')"));
        ins.addBindValue(userId);
        ins.addBindValue(stationId);
        ins.addBindValue(idle);
        ins.addBindValue(nowStr());
        ins.addBindValue(nowStr());
    } else {
        // 没空桩 → 入队。排号 = 当前等待人数 + 1, 预计每人 20 分钟
        const int waiting = static_cast<int>(scalarOf(
            QStringLiteral("SELECT COUNT(*) FROM reservation WHERE station_id=? AND status='waiting'"),
            {stationId}));
        const int queueNo = waiting + 1;
        const QString eta = QDateTime::currentDateTime().addSecs(queueNo * 20 * 60)
                                .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        ins.prepare(QStringLiteral(
            "INSERT INTO reservation(user_id,station_id,charger_id,queue_no,reserve_time,"
            "  estimate_start_time,notified,status) VALUES(?,?,NULL,?,?,?,0,'waiting')"));
        ins.addBindValue(userId);
        ins.addBindValue(stationId);
        ins.addBindValue(queueNo);
        ins.addBindValue(nowStr());
        ins.addBindValue(eta);
    }
    if (!ins.exec()) {
        QSqlDatabase::database().rollback();
        fail(err, 4002, QStringLiteral("加入排队失败"));
        return std::nullopt;
    }
    const int rid = ins.lastInsertId().toInt();
    QSqlDatabase::database().commit();
    return fetchReservation(rid);
}

std::optional<ReservationView> cancelReservation(int userId, int reservationId, OpError *err)
{
    const auto r = fetchReservation(reservationId);
    if (!r || r->userId != userId) {
        fail(err, 4001, QStringLiteral("预约不存在: id=%1").arg(reservationId));
        return std::nullopt;
    }
    if (r->status == QStringLiteral("cancelled")) {
        fail(err, 2003, QStringLiteral("该预约已取消"));
        return std::nullopt;
    }
    QSqlQuery up;
    up.prepare(QStringLiteral("UPDATE reservation SET status='cancelled' WHERE id=?"));
    up.addBindValue(reservationId);
    if (!up.exec()) {
        fail(err, 4002, QStringLiteral("取消失败"));
        return std::nullopt;
    }
    return fetchReservation(reservationId);
}

QList<ReservationView> listReservations(int userId)
{
    QList<ReservationView> out;
    QSqlQuery q;
    q.prepare(QString::fromLatin1(kSelectReservation)
              + QStringLiteral("WHERE r.user_id=? ORDER BY r.id DESC"));
    q.addBindValue(userId);
    if (!q.exec()) return out;
    while (q.next()) {
        ReservationView r = rowToReservation(q);
        if (r.status == QStringLiteral("waiting")) r.aheadCount = aheadOf(r.stationId, r.queueNo);
        out.append(r);
    }
    return out;
}

std::optional<ReservationView> matchNextInQueue(int stationId)
{
    const int idle = findIdleCharger(stationId);
    if (idle <= 0) return std::nullopt;             // 没有空桩, 不用叫号

    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT id FROM reservation WHERE station_id=? AND status='waiting' AND notified=0 "
        "ORDER BY queue_no, id LIMIT 1"));
    q.addBindValue(stationId);
    if (!q.exec() || !q.next()) return std::nullopt;  // 没人在等
    const int rid = q.value(0).toInt();

    QSqlQuery up;
    up.prepare(QStringLiteral(
        "UPDATE reservation SET status='matched', charger_id=?, notified=1, estimate_start_time=? "
        "WHERE id=?"));
    up.addBindValue(idle);
    up.addBindValue(nowStr());
    up.addBindValue(rid);
    if (!up.exec()) return std::nullopt;
    return fetchReservation(rid);
}

SimParams simParamsOf(int orderId)
{
    SimParams p;
    const auto o = fetchOrder(orderId);
    if (!o) return p;
    p.powerKw    = scalarOf(QStringLiteral("SELECT power FROM charger WHERE id=?"), {o->chargerId});
    p.batteryKwh = (o->vehicleId > 0)
        ? scalarOf(QStringLiteral("SELECT battery_kwh FROM vehicle WHERE id=?"), {o->vehicleId})
        : 60.0;
    if (p.batteryKwh <= 0) p.batteryKwh = 60.0;
    const double svcFee = scalarOf(QStringLiteral("SELECT service_fee FROM station WHERE id=?"),
                                   {o->stationId});
    p.unitPrice = dao::currentPrice(o->stationId) + svcFee;
    p.ok = true;
    return p;
}

double userBalance(int userId)
{
    return scalarOf(QStringLiteral("SELECT balance FROM user WHERE id = ?"), {userId});
}

int userPoints(int userId)
{
    return static_cast<int>(scalarOf(QStringLiteral("SELECT points FROM user WHERE id = ?"), {userId}));
}

// ==================== 创建订单 ====================
std::optional<OrderView> createOrder(int userId, int stationId, int chargerId, OpError *err)
{
    // 规则1: 一个用户同时只能有一个进行中订单
    const double active = scalarOf(
        QStringLiteral("SELECT COUNT(*) FROM charging_order WHERE user_id = ? "
                       "AND status IN ('reserved','charging','pending_settle')"),
        {userId});
    if (active > 0) {
        fail(err, 2001, QStringLiteral("您有未完成的充电订单, 请先完成或取消"));
        return std::nullopt;
    }

    // 规则2: 电桩必须存在, 且当前空闲
    QSqlQuery c;
    c.prepare(QStringLiteral("SELECT station_id, status FROM charger WHERE id = ?"));
    c.addBindValue(chargerId);
    if (!c.exec() || !c.next()) {
        fail(err, 4001, QStringLiteral("电桩不存在: id=%1").arg(chargerId));
        return std::nullopt;
    }
    const int     realStation = c.value(0).toInt();
    const QString chargerStat = c.value(1).toString();
    if (chargerStat != QStringLiteral("idle")) {
        fail(err, 3002, QStringLiteral("该电桩当前不可用(状态: %1)").arg(chargerStat));
        return std::nullopt;
    }
    // 客户端传的 station_id 只做校验, 真正以电桩所属电站为准
    if (stationId > 0 && stationId != realStation) {
        fail(err, 9001, QStringLiteral("station_id 与该电桩所属电站不一致"));
        return std::nullopt;
    }

    // 默认车辆(用于电池容量), 没有车也能下单
    const int vehicleId = static_cast<int>(scalarOf(
        QStringLiteral("SELECT IFNULL((SELECT id FROM vehicle WHERE user_id=? "
                       "ORDER BY is_default DESC, id LIMIT 1),0)"),
        {userId}));

    QSqlDatabase::database().transaction();
    QSqlQuery ins;
    ins.prepare(QStringLiteral(
        "INSERT INTO charging_order(user_id, station_id, charger_id, vehicle_id, status, create_time) "
        "VALUES(?,?,?,?,'reserved',?)"));
    ins.addBindValue(userId);
    ins.addBindValue(realStation);
    ins.addBindValue(chargerId);
    ins.addBindValue(vehicleId > 0 ? QVariant(vehicleId) : QVariant());
    ins.addBindValue(nowStr());
    if (!ins.exec()) {
        QSqlDatabase::database().rollback();
        fail(err, 4002, QStringLiteral("创建订单失败"));
        return std::nullopt;
    }
    const int orderId = ins.lastInsertId().toInt();
    setChargerStatus(chargerId, QStringLiteral("reserved"));
    addTimeline(orderId, QStringLiteral("reserved"), QStringLiteral("预约成功"));
    QSqlDatabase::database().commit();

    return fetchOrder(orderId);
}

// ==================== 开始充电 ====================
std::optional<OrderView> startOrder(int userId, int orderId, double startSoc, OpError *err)
{
    const auto o = fetchOwnOrder(userId, orderId, err);
    if (!o) return std::nullopt;
    if (o->status != QStringLiteral("reserved")) {
        fail(err, 2003, QStringLiteral("订单当前状态(%1)不允许开始充电").arg(o->status));
        return std::nullopt;
    }
    // 规则3: 起充余额门槛
    if (userBalance(userId) < kMinStartBalance) {
        fail(err, 2002, QStringLiteral("余额不足, 起充需至少 %1 元").arg(kMinStartBalance));
        return std::nullopt;
    }
    if (startSoc < 0 || startSoc > 100) startSoc = 20.0;   // 默认起始电量

    QSqlDatabase::database().transaction();
    QSqlQuery up;
    up.prepare(QStringLiteral(
        "UPDATE charging_order SET status='charging', start_time=?, start_soc=?, target_soc=100 "
        "WHERE id=?"));
    up.addBindValue(nowStr());
    up.addBindValue(startSoc);
    up.addBindValue(orderId);
    if (!up.exec()) {
        QSqlDatabase::database().rollback();
        fail(err, 4002, QStringLiteral("开始充电失败"));
        return std::nullopt;
    }
    setChargerStatus(o->chargerId, QStringLiteral("charging"));
    addTimeline(orderId, QStringLiteral("started"), QStringLiteral("开始充电"));
    QSqlDatabase::database().commit();

    return fetchOrder(orderId);
}

// ==================== 结束充电 ====================
std::optional<OrderView> finishOrder(int userId, int orderId, double endSoc, OpError *err)
{
    const auto o = fetchOwnOrder(userId, orderId, err);
    if (!o) return std::nullopt;
    if (o->status != QStringLiteral("charging")) {
        fail(err, 2003, QStringLiteral("订单当前状态(%1)不允许结束充电").arg(o->status));
        return std::nullopt;
    }

    // --- 时长 ---
    const QDateTime st = QDateTime::fromString(o->startTime, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QDateTime en = QDateTime::currentDateTime();
    int durationMin = st.isValid() ? static_cast<int>(st.secsTo(en) / 60) : 0;
    if (durationMin < 1) durationMin = 1;          // 演示时可能只有几秒, 至少按 1 分钟计

    // --- 电量 ---
    const double batteryKwh = (o->vehicleId > 0)
        ? scalarOf(QStringLiteral("SELECT battery_kwh FROM vehicle WHERE id=?"), {o->vehicleId})
        : 60.0;
    const double powerKw = scalarOf(QStringLiteral("SELECT power FROM charger WHERE id=?"),
                                    {o->chargerId});
    double energy = 0, finalSoc = 0;
    if (endSoc >= 0 && endSoc > o->startSoc) {
        // 仿真/客户端给了结束电量: 按 SOC 差算电量(更贴近真实演示)
        finalSoc = qMin(100.0, endSoc);
        energy   = (finalSoc - o->startSoc) / 100.0 * (batteryKwh > 0 ? batteryKwh : 60.0);
    } else {
        // 没给: 按 实际时长 × 功率 × 0.92(充电效率) 估算。
        // ★ 必须按电池剩余容量封顶 —— 否则服务端重启后结束一笔旧订单时,
        //   duration 会是几千分钟, 算出"给 60kWh 的车充了 7000 度电"的天价账单。
        const double cap = batteryKwh > 0 ? batteryKwh : 60.0;
        const double roomKwh = qMax(0.0, (100.0 - o->startSoc) / 100.0 * cap);
        energy   = qMin(roomKwh, powerKw * (durationMin / 60.0) * 0.92);
        finalSoc = qMin(100.0, o->startSoc + energy / cap * 100.0);
    }
    // 时长同样封顶: 最多按"充满所需时间"计, 避免订单详情里出现几千分钟
    if (powerKw > 0) {
        const int maxMin = static_cast<int>(std::ceil(energy / (powerKw * 0.92) * 60.0));
        if (maxMin > 0 && durationMin > maxMin) durationMin = maxMin;
    }
    energy = std::round(energy * 100) / 100.0;

    // --- 金额 = 电量 × (分时电价 + 服务费) ---
    const int     hour   = st.isValid() ? st.time().hour() : QTime::currentTime().hour();
    const QString level  = priceLevelOf(hour);
    const double  price  = dao::currentPrice(o->stationId);
    const double  svcFee = scalarOf(QStringLiteral("SELECT service_fee FROM station WHERE id=?"),
                                    {o->stationId});
    const double  amount = std::round(energy * (price + svcFee) * 100) / 100.0;

    QSqlDatabase::database().transaction();
    QSqlQuery up;
    up.prepare(QStringLiteral(
        "UPDATE charging_order SET status='pending_settle', end_time=?, end_soc=?, "
        "  duration_min=?, energy_kwh=?, price_level=?, amount=?, pay_amount=? WHERE id=?"));
    up.addBindValue(nowStr());
    up.addBindValue(std::round(finalSoc * 10) / 10.0);
    up.addBindValue(durationMin);
    up.addBindValue(energy);
    up.addBindValue(level);
    up.addBindValue(amount);
    up.addBindValue(amount);          // 未抵扣前实付 = 应收
    up.addBindValue(orderId);
    if (!up.exec()) {
        QSqlDatabase::database().rollback();
        fail(err, 4002, QStringLiteral("结束充电失败"));
        return std::nullopt;
    }
    // 电桩回到空闲, 并累加统计
    QSqlQuery cs;
    cs.prepare(QStringLiteral(
        "UPDATE charger SET status='idle', voltage=0, current=0, "
        "  total_charge_count=total_charge_count+1, total_charge_duration=total_charge_duration+? "
        "WHERE id=?"));
    cs.addBindValue(durationMin);
    cs.addBindValue(o->chargerId);
    cs.exec();
    addTimeline(orderId, QStringLiteral("finished"), QStringLiteral("充电结束"),
                QStringLiteral("充入 %1 kWh").arg(energy));
    QSqlDatabase::database().commit();

    return fetchOrder(orderId);
}

// ==================== 结算(五张表, 一个事务) ====================
std::optional<OrderView> settleOrder(int userId, int orderId, int pointsUsed, OpError *err)
{
    const auto o = fetchOwnOrder(userId, orderId, err);
    if (!o) return std::nullopt;
    if (o->status != QStringLiteral("pending_settle")) {
        fail(err, 2003, QStringLiteral("订单当前状态(%1)不允许结算").arg(o->status));
        return std::nullopt;
    }

    // --- 积分抵扣: 不能超过自己的积分, 也不能抵扣超过订单金额 ---
    const int ownPoints = userPoints(userId);
    if (pointsUsed < 0) pointsUsed = 0;
    pointsUsed = qMin(pointsUsed, ownPoints);
    pointsUsed = qMin(pointsUsed, static_cast<int>(o->amount * kPointsPerYuan));
    const double discount = std::round(pointsUsed * 100.0 / kPointsPerYuan) / 100.0;
    const double payAmount = std::round(qMax(0.0, o->amount - discount) * 100) / 100.0;

    // --- 余额检查 ---
    const double balance = userBalance(userId);
    if (balance < payAmount) {
        fail(err, 2002, QStringLiteral("余额不足: 需 %1 元, 当前 %2 元").arg(payAmount).arg(balance));
        return std::nullopt;
    }
    const int pointsEarned = static_cast<int>(std::floor(payAmount));   // 1 元 = 1 积分
    const double balanceAfter = std::round((balance - payAmount) * 100) / 100.0;

    // ★ 五张表必须一起成功: 扣钱了却没改订单状态 = 用户白花钱
    QSqlDatabase::database().transaction();
    bool okAll = true;

    // 1) user: 扣余额 + 积分净变化
    QSqlQuery u;
    u.prepare(QStringLiteral("UPDATE user SET balance=?, points=points-?+? WHERE id=?"));
    u.addBindValue(balanceAfter);
    u.addBindValue(pointsUsed);
    u.addBindValue(pointsEarned);
    u.addBindValue(userId);
    okAll = okAll && u.exec();

    // 2) charging_order: 转 completed
    QSqlQuery od;
    od.prepare(QStringLiteral(
        "UPDATE charging_order SET status='completed', settle_time=?, discount_amount=?, "
        "  pay_amount=?, points_used=?, points_earned=? WHERE id=?"));
    od.addBindValue(nowStr());
    od.addBindValue(discount);
    od.addBindValue(payAmount);
    od.addBindValue(pointsUsed);
    od.addBindValue(pointsEarned);
    od.addBindValue(orderId);
    okAll = okAll && od.exec();

    // 3) wallet_transaction: 钱包流水(余额是"现在多少钱", 流水是"怎么变成这样的")
    QSqlQuery w;
    w.prepare(QStringLiteral(
        "INSERT INTO wallet_transaction(user_id,type,amount,balance_after,order_id,remark,create_time) "
        "VALUES(?,'consume',?,?,?,?,?)"));
    w.addBindValue(userId);
    w.addBindValue(-payAmount);
    w.addBindValue(balanceAfter);
    w.addBindValue(orderId);
    w.addBindValue(QStringLiteral("充电消费 #%1").arg(orderId));
    w.addBindValue(nowStr());
    okAll = okAll && w.exec();

    // 4) point_record: 积分流水(获得 / 抵扣各记一条)
    if (pointsEarned > 0) {
        QSqlQuery p;
        p.prepare(QStringLiteral(
            "INSERT INTO point_record(user_id,change,reason,create_time) VALUES(?,?,'charge',?)"));
        p.addBindValue(userId);
        p.addBindValue(pointsEarned);
        p.addBindValue(nowStr());
        okAll = okAll && p.exec();
    }
    if (pointsUsed > 0) {
        QSqlQuery p;
        p.prepare(QStringLiteral(
            "INSERT INTO point_record(user_id,change,reason,create_time) VALUES(?,?,'redeem',?)"));
        p.addBindValue(userId);
        p.addBindValue(-pointsUsed);
        p.addBindValue(nowStr());
        okAll = okAll && p.exec();
    }

    // 5) order_timeline
    addTimeline(orderId, QStringLiteral("settled"), QStringLiteral("结算完成"),
                QStringLiteral("实付 %1 元, 获得 %2 积分").arg(payAmount).arg(pointsEarned));

    if (!okAll) {
        QSqlDatabase::database().rollback();      // 任何一步失败 → 当作什么都没发生
        fail(err, 4002, QStringLiteral("结算失败, 已回滚"));
        return std::nullopt;
    }
    QSqlDatabase::database().commit();

    return fetchOrder(orderId);
}

// ==================== 取消订单 ====================
std::optional<OrderView> cancelOrder(int userId, int orderId, OpError *err)
{
    const auto o = fetchOwnOrder(userId, orderId, err);
    if (!o) return std::nullopt;
    if (o->status != QStringLiteral("reserved")) {
        fail(err, 2003, QStringLiteral("订单当前状态(%1)不允许取消").arg(o->status));
        return std::nullopt;
    }
    QSqlDatabase::database().transaction();
    QSqlQuery up;
    up.prepare(QStringLiteral("UPDATE charging_order SET status='cancelled' WHERE id=?"));
    up.addBindValue(orderId);
    if (!up.exec()) {
        QSqlDatabase::database().rollback();
        fail(err, 4002, QStringLiteral("取消失败"));
        return std::nullopt;
    }
    setChargerStatus(o->chargerId, QStringLiteral("idle"));   // 把桩还回去
    QSqlDatabase::database().commit();
    // 注意: order_timeline.node 的 CHECK 里没有 cancelled, 取消不写时间轴节点
    return fetchOrder(orderId);
}

// ==================== 查询 ====================
QList<OrderView> listOrders(int userId, const QString &status)
{
    QList<OrderView> out;
    QSqlQuery q;
    if (status.trimmed().isEmpty()) {
        q.prepare(QString::fromLatin1(kSelectOrder)
                  + QStringLiteral("WHERE o.user_id = ? ORDER BY o.id DESC"));
        q.addBindValue(userId);
    } else {
        q.prepare(QString::fromLatin1(kSelectOrder)
                  + QStringLiteral("WHERE o.user_id = ? AND o.status = ? ORDER BY o.id DESC"));
        q.addBindValue(userId);
        q.addBindValue(status.trimmed());
    }
    if (!q.exec()) return out;
    while (q.next()) out.append(rowToOrder(q));
    return out;
}

std::optional<OrderView> findOrder(int userId, int orderId)
{
    const auto o = fetchOrder(orderId);
    if (!o || o->userId != userId) return std::nullopt;
    return o;
}

QList<TimelineRow> timelineOf(int orderId)
{
    QList<TimelineRow> out;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT id, node, label, event_time, IFNULL(detail,'') FROM order_timeline "
        "WHERE order_id = ? ORDER BY event_time, id"));
    q.addBindValue(orderId);
    if (!q.exec()) return out;
    while (q.next()) {
        TimelineRow t;
        t.id        = q.value(0).toInt();
        t.node      = q.value(1).toString();
        t.label     = q.value(2).toString();
        t.eventTime = q.value(3).toString();
        t.detail    = q.value(4).toString();
        out.append(t);
    }
    return out;
}

}  // namespace dao
