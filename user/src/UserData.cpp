#include "UserData.h"

#include <QDateTime>
#include <QHash>
#include <QVariant>

#include <algorithm>

namespace {

// 便捷构造 map
QVariantMap S(const std::initializer_list<std::pair<QString, QVariant>>& list) {
    QVariantMap m;
    for (const auto& p : list)
        m.insert(p.first, p.second);
    return m;
}

// 首个（默认）用户手机号，作为 profile 的固定 phone；昵称默认「用户+后四位」
const QString kPhone = QStringLiteral("13800000001");

// —— 我的车辆（vehicle）——
QVariantList buildVehicles() {
    return {
        S({
            {"id",             101},
            {"name",           QStringLiteral("我的小鹏 G6")},
            {"type",           QStringLiteral("car")},
            {"battery_kwh",    66.0},
            {"connector_type", QStringLiteral("dc_gb")},
            {"max_power_kw",   250.0},
            {"is_default",     1},
            {"created_time",   QStringLiteral("2024-12-10 14:00:00")}
        }),
        S({
            {"id",             102},
            {"name",           QStringLiteral("通勤电摩")},
            {"type",           QStringLiteral("two_wheeler")},
            {"battery_kwh",    4.0},
            {"connector_type", QStringLiteral("ac_gb")},
            {"max_power_kw",   3.3},
            {"is_default",     0},
            {"created_time",   QStringLiteral("2025-03-22 09:10:00")}
        })
    };
}

// 订单公共辅助：把 charging_order 字段 + 展示聚合字段一起产出
struct OrderSeed {
    int id;
    int stationId;
    const char* stationName;
    const char* stationArea;
    const char* chargerCode;
    const char* chargerType;
    const char* status;
    double startSoc;
    double targetSoc;
    double endSoc;
    const char* startTime;
    const char* endTime;
    int durationMin;
    double energyKwh;
    const char* priceLevel;
    double amount;
    double discountAmount;
    int pointsUsed;
    const char* couponTitle;
    int pointsEarned;
    const char* createTime;
    const char* settleTime;
};

QVariantMap buildOrder(const OrderSeed& o) {
    return S({
        {"id",               o.id},
        {"station_id",       o.stationId},
        {"station_name",     QString::fromUtf8(o.stationName)},
        {"station_area",     QString::fromUtf8(o.stationArea)},
        {"charger_code",     QString::fromUtf8(o.chargerCode)},
        {"charger_type",     QString::fromUtf8(o.chargerType)},
        {"status",           QString::fromUtf8(o.status)},
        {"start_soc",        o.startSoc},
        {"target_soc",       o.targetSoc},
        {"end_soc",          o.endSoc},
        {"start_time",       QString::fromUtf8(o.startTime)},
        {"end_time",         QString::fromUtf8(o.endTime)},
        {"duration_min",     o.durationMin},
        {"energy_kwh",       o.energyKwh},
        {"price_level",      QString::fromUtf8(o.priceLevel)},
        {"amount",           o.amount},
        {"discount_amount",  o.discountAmount},
        {"points_used",      o.pointsUsed},
        {"pay_amount",       o.amount - o.discountAmount},
        {"coupon_title",     QString::fromUtf8(o.couponTitle)},
        {"points_earned",    o.pointsEarned},
        {"create_time",      QString::fromUtf8(o.createTime)},
        {"settle_time",      QString::fromUtf8(o.settleTime)}
    });
}

QList<OrderSeed> orderSeeds() {
    return {
        // 进行中（充电中）
        { 9001, 1, "星星充·国贸中心旗舰站", "北京·朝阳", "A-01", "fast", "charging",
          18.0, 90.0, 18.0, "2025-08-20 18:02:00", "", 0, 0.0, "peak", 0.0, 0.0, 0, "", 0,
          "2025-08-20 17:58:00", "" },
        // 待结算
        { 9002, 4, "自营·中关村软件园旗舰站", "北京·海淀", "A2", "fast", "pending_settle",
          12.0, 90.0, 90.0, "2025-08-19 21:10:00", "2025-08-19 21:52:00", 42, 28.7, "flat",
          19.5, 5.0, 0, "满 50 减 5", 39, "2025-08-19 21:05:00", "" },
        // 已完成
        { 9003, 11, "自营·深圳湾科技园站", "深圳·南山", "N1", "fast", "completed",
          20.0, 80.0, 80.0, "2025-08-15 09:20:00", "2025-08-15 09:58:00", 38, 24.6, "flat",
          16.2, 0.0, 0, "", 32, "2025-08-15 09:15:00", "2025-08-15 09:58:30" },
        // 已完成（旧）
        { 9004, 2, "国网·朝阳公园南门站", "北京·朝阳", "1号", "fast", "completed",
          35.0, 95.0, 95.0, "2025-08-02 19:30:00", "2025-08-02 20:20:00", 50, 31.2, "valley",
          9.9, 0.0, 0, "", 25, "2025-08-02 19:25:00", "2025-08-02 20:21:00" },
        // 已取消
        { 9005, 6, "自营·王府井百货站", "北京·东城", "K2", "fast", "cancelled",
          0.0, 90.0, 0.0, "", "", 0, 0.0, "", 0.0, 0.0, 0, "", 0,
          "2025-07-28 15:40:00", "" }
    };
}

// —— 订单时间轴（order_timeline），仅给有节点的订单 ——
QVariantList buildTimeline(int orderId) {
    // 通用：按订单状态给一组节点（示例数据，与服务端落库字段对齐）
    auto node = [](const char* n, const QString& label, const char* time, const QString& detail) {
        (void)n;
        return S({
            {"node",       QString::fromUtf8(n)},
            {"label",      label},
            {"event_time", QString::fromUtf8(time)},
            {"detail",     detail}
        });
    };
    if (orderId == 9003) {
        return {
            node("reserved",        QStringLiteral("预约成功"),       "2025-08-15 09:15:00", QStringLiteral("目标电站 深圳湾科技园站")),
            node("arrived",         QStringLiteral("到达充电站"),     "2025-08-15 09:20:00", {}),
            node("started",         QStringLiteral("开始充电"),       "2025-08-15 09:20:00", QStringLiteral("起始电量 20%")),
            node("soc_50",          QStringLiteral("电量达到 50%"),   "2025-08-15 09:41:00", {}),
            node("target_reached",  QStringLiteral("达到目标电量"),   "2025-08-15 09:58:00", QStringLiteral("目标电量 80%")),
            node("finished",        QStringLiteral("充电结束"),       "2025-08-15 09:58:00", {}),
            node("settled",         QStringLiteral("自动结算"),       "2025-08-15 09:58:30", QStringLiteral("实付 16.20 元"))
        };
    }
    if (orderId == 9004) {
        return {
            node("reserved",        QStringLiteral("预约成功"),       "2025-08-02 19:25:00", {}),
            node("arrived",         QStringLiteral("到达充电站"),     "2025-08-02 19:30:00", {}),
            node("started",         QStringLiteral("开始充电"),       "2025-08-02 19:30:00", QStringLiteral("起始电量 35%")),
            node("soc_50",          QStringLiteral("电量达到 50%"),   "2025-08-02 19:53:00", {}),
            node("target_reached",  QStringLiteral("达到目标电量"),   "2025-08-02 20:20:00", QStringLiteral("目标电量 95%")),
            node("finished",        QStringLiteral("充电结束"),       "2025-08-02 20:20:00", {}),
            node("settled",         QStringLiteral("自动结算"),       "2025-08-02 20:21:00", QStringLiteral("实付 9.90 元"))
        };
    }
    if (orderId == 9002) {
        return {
            node("reserved",        QStringLiteral("预约成功"),       "2025-08-19 21:05:00", {}),
            node("arrived",         QStringLiteral("到达充电站"),     "2025-08-19 21:10:00", {}),
            node("started",         QStringLiteral("开始充电"),       "2025-08-19 21:10:00", QStringLiteral("起始电量 12%")),
            node("target_reached",  QStringLiteral("达到目标电量"),   "2025-08-19 21:52:00", QStringLiteral("目标电量 90%")),
            node("finished",        QStringLiteral("充电结束"),       "2025-08-19 21:52:00", QStringLiteral("待结算"))
        };
    }
    return {};
}

// —— 优惠券（user_coupon + coupon 合并）——
QVariantList buildCoupons() {
    return {
        S({
            {"id",             5001},
            {"title",          QStringLiteral("新人立减 ¥5")},
            {"type",           QStringLiteral("new_user")},
            {"discount_amount", 5.0},
            {"min_amount",     0.0},
            {"scope",          QStringLiteral("通用")},
            {"time_range",     QString()},
            {"valid_until",    QStringLiteral("2025-12-31")},
            {"status",         QStringLiteral("unused")},
            {"receive_time",   QStringLiteral("2024-11-02 09:30:00")}
        }),
        S({
            {"id",             5002},
            {"title",          QStringLiteral("满 50 减 5")},
            {"type",           QStringLiteral("full_reduction")},
            {"discount_amount", 5.0},
            {"min_amount",     50.0},
            {"scope",          QStringLiteral("通用")},
            {"time_range",     QString()},
            {"valid_until",    QStringLiteral("2025-10-31")},
            {"status",         QStringLiteral("unused")},
            {"receive_time",   QStringLiteral("2025-06-01 10:00:00")}
        }),
        S({
            {"id",             5003},
            {"title",          QStringLiteral("夜间充电券 ¥3")},
            {"type",           QStringLiteral("night")},
            {"discount_amount", 3.0},
            {"min_amount",     0.0},
            {"scope",          QStringLiteral("夜间 22:00–06:00")},
            {"time_range",     QStringLiteral("22:00–06:00")},
            {"valid_until",    QStringLiteral("2025-09-30")},
            {"status",         QStringLiteral("used")},
            {"receive_time",   QStringLiteral("2025-05-15 20:00:00")}
        }),
        S({
            {"id",             5004},
            {"title",          QStringLiteral("满 100 减 10")},
            {"type",           QStringLiteral("full_reduction")},
            {"discount_amount", 10.0},
            {"min_amount",     100.0},
            {"scope",          QStringLiteral("通用")},
            {"time_range",     QString()},
            {"valid_until",    QStringLiteral("2025-07-31")},
            {"status",         QStringLiteral("expired")},
            {"receive_time",   QStringLiteral("2025-01-01 10:00:00")}
        })
    };
}

// —— 积分记录（point_record）——
QVariantList buildPointRecords() {
    return {
        S({{"id", 7001}, {"change", 39},  {"reason", QStringLiteral("charge")}, {"create_time", QStringLiteral("2025-08-19 21:58:00")}}),
        S({{"id", 7002}, {"change", 32},  {"reason", QStringLiteral("charge")}, {"create_time", QStringLiteral("2025-08-15 09:58:00")}}),
        S({{"id", 7003}, {"change", 25},  {"reason", QStringLiteral("charge")}, {"create_time", QStringLiteral("2025-08-02 20:21:00")}}),
        S({{"id", 7004}, {"change", -200},{"reason", QStringLiteral("redeem")}, {"create_time", QStringLiteral("2025-07-30 12:00:00")}}),
        S({{"id", 7005}, {"change", 58},  {"reason", QStringLiteral("charge")}, {"create_time", QStringLiteral("2025-07-20 18:40:00")}})
    };
}

// —— 钱包流水（wallet_transaction）——
QVariantList buildWallet() {
    return {
        S({{"id", 8001}, {"type", QStringLiteral("recharge")}, {"amount", 100.0}, {"balance_after", 186.50}, {"remark", QStringLiteral("余额充值")}, {"create_time", QStringLiteral("2025-08-19 20:50:00")}}),
        S({{"id", 8002}, {"type", QStringLiteral("consume")},  {"amount", -14.50}, {"balance_after", 86.50},  {"remark", QStringLiteral("充电结算（订单 9002）")}, {"create_time", QStringLiteral("2025-08-19 21:52:00")}}),
        S({{"id", 8003}, {"type", QStringLiteral("recharge")}, {"amount", 50.0},  {"balance_after", 101.0},  {"remark", QStringLiteral("余额充值")}, {"create_time", QStringLiteral("2025-08-10 09:00:00")}}),
        S({{"id", 8004}, {"type", QStringLiteral("consume")},  {"amount", -16.20}, {"balance_after", 51.0},   {"remark", QStringLiteral("充电结算（订单 9003）")}, {"create_time", QStringLiteral("2025-08-15 09:58:00")}})
    };
}

// —— 站内消息（notification）——
QVariantList buildNotifications() {
    return {
        S({{"id", 6001}, {"type", QStringLiteral("order")},       {"title", QStringLiteral("充电完成")},          {"content", QStringLiteral("您在中关村软件园旗舰站的充电已完成，请及时结算。")}, {"is_read", 0}, {"create_time", QStringLiteral("2025-08-19 21:52:00")}}),
        S({{"id", 6002}, {"type", QStringLiteral("coupon")},      {"title", QStringLiteral("优惠券到账")},        {"content", QStringLiteral("您已获得「满 50 减 5」优惠券，有效期至 2025-10-31。")}, {"is_read", 0}, {"create_time", QStringLiteral("2025-06-01 10:00:00")}}),
        S({{"id", 6003}, {"type", QStringLiteral("point")},       {"title", QStringLiteral("积分到账")},          {"content", QStringLiteral("本单获得 32 积分，当前累计 1280 积分。")}, {"is_read", 1}, {"create_time", QStringLiteral("2025-08-15 09:58:00")}}),
        S({{"id", 6004}, {"type", QStringLiteral("reservation")}, {"title", QStringLiteral("排队提醒")},          {"content", QStringLiteral("您预约的充电桩即将轮到，请前往。")}, {"is_read", 1}, {"create_time", QStringLiteral("2025-07-28 15:42:00")}})
    };
}

// —— 画像：由订单聚合 ——
QVariantMap buildPortrait(const QList<OrderSeed>& orders) {
    int completed = 0;
    double energySum = 0.0;
    QHash<int, int> stationCount;
    int fastCount = 0, slowCount = 0;
    for (const auto& o : orders) {
        if (QString::fromUtf8(o.status) == QStringLiteral("completed")) {
            completed++;
            energySum += o.energyKwh;
            stationCount[o.stationId]++;
        }
        if (QString::fromUtf8(o.chargerType) == QStringLiteral("fast")) fastCount++;
        else slowCount++;
    }
    int favStation = 0, favMax = 0;
    for (auto it = stationCount.begin(); it != stationCount.end(); ++it) {
        if (it.value() > favMax) { favMax = it.value(); favStation = it.key(); }
    }
    QString favName;
    for (const auto& o : orders)
        if (o.stationId == favStation) { favName = QString::fromUtf8(o.stationName); break; }

    const int months = 6;  // 示例口径：约半年
    double monthAvg = completed / static_cast<double>(months);
    double avgEnergy = completed > 0 ? energySum / completed : 0.0;
    return S({
        {"month_avg_count",   monthAvg},
        {"avg_energy_kwh",    avgEnergy},
        {"favorite_station",  favName},
        {"usual_hours",       QStringLiteral("18:00–21:00")},
        {"prefer_type",       fastCount >= slowCount ? QStringLiteral("快充") : QStringLiteral("慢充")}
    });
}

}  // namespace

UserData::UserData(QObject* parent)
    : QObject(parent), m_nickname(QStringLiteral("用户0001")), m_balance(86.50) {}

QVariantMap UserData::profile() const {
    return S({
        {"id",              3},
        {"phone",           kPhone},
        {"nickname",        m_nickname},
        {"avatar_path",     QString()},
        {"balance",         m_balance},
        {"points",          1280},
        {"level",           QStringLiteral("vip")},
        {"status",          QStringLiteral("normal")},
        {"register_time",   QStringLiteral("2024-11-02 09:30:00")},
        {"last_login_time", QStringLiteral("2025-08-20 08:15:00")}
    });
}

QVariantList UserData::vehicles() const {
    static const QVariantList s = buildVehicles();
    return s;
}

QVariantList UserData::orders() const {
    static const auto seeds = orderSeeds();
    QVariantList out;
    for (const auto& o : seeds)
        out.append(buildOrder(o));
    // 按 create_time 降序（新在前）
    std::sort(out.begin(), out.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap().value(QStringLiteral("create_time")).toString()
             > b.toMap().value(QStringLiteral("create_time")).toString();
    });
    return out;
}

QVariantMap UserData::orderById(int orderId) const {
    for (const auto& v : orders()) {
        auto m = v.toMap();
        if (m.value(QStringLiteral("id")).toInt() == orderId)
            return m;
    }
    return {};
}

QVariantList UserData::orderTimeline(int orderId) const {
    return buildTimeline(orderId);
}

QVariantList UserData::coupons() const {
    static const QVariantList s = buildCoupons();
    return s;
}

QVariantList UserData::pointRecords() const {
    static const QVariantList s = buildPointRecords();
    return s;
}

QVariantList UserData::walletTransactions() const {
    static const QVariantList s = buildWallet();
    return s;
}

QVariantList UserData::notifications() const {
    static const QVariantList s = buildNotifications();
    return s;
}

QVariantMap UserData::portrait() const {
    static const auto seeds = orderSeeds();
    static const QVariantMap s = buildPortrait(seeds);
    return s;
}

bool UserData::recharge(double amount) {
    if (amount <= 0.0)
        return false;
    m_balance += amount;
    emit profileChanged();
    return true;
}

bool UserData::updateNickname(const QString& nickname) {
    const QString trimmed = nickname.trimmed();
    if (trimmed.isEmpty() || trimmed.size() < 2 || trimmed.size() > 20)
        return false;
    m_nickname = trimmed;
    emit profileChanged();
    return true;
}