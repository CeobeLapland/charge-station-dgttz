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
        }),
        S({
            {"id",             103},
            {"name",           QStringLiteral("家用特斯拉 Model Y")},
            {"type",           QStringLiteral("car")},
            {"battery_kwh",    78.4},
            {"connector_type", QStringLiteral("dc_gb")},
            {"max_power_kw",   250.0},
            {"is_default",     0},
            {"created_time",   QStringLiteral("2024-06-18 10:20:00")}
        }),
        S({
            {"id",             104},
            {"name",           QStringLiteral("老爸的比亚迪汉")},
            {"type",           QStringLiteral("car")},
            {"battery_kwh",    85.4},
            {"connector_type", QStringLiteral("dc_gb")},
            {"max_power_kw",   230.0},
            {"is_default",     0},
            {"created_time",   QStringLiteral("2023-09-25 16:40:00")}
        }),
        S({
            {"id",             105},
            {"name",           QStringLiteral("五菱宏光 MINI")},
            {"type",           QStringLiteral("car")},
            {"battery_kwh",    17.3},
            {"connector_type", QStringLiteral("ac_gb")},
            {"max_power_kw",   6.6},
            {"is_default",     0},
            {"created_time",   QStringLiteral("2024-02-14 09:00:00")}
        }),
        S({
            {"id",             106},
            {"name",           QStringLiteral("接娃电动三轮车")},
            {"type",           QStringLiteral("three_wheeler")},
            {"battery_kwh",    5.6},
            {"connector_type", QStringLiteral("ac_gb")},
            {"max_power_kw",   2.2},
            {"is_default",     0},
            {"created_time",   QStringLiteral("2025-01-10 11:30:00")}
        }),
        S({
            {"id",             107},
            {"name",           QStringLiteral("小米 SU7 租赁车")},
            {"type",           QStringLiteral("car")},
            {"battery_kwh",    101.0},
            {"connector_type", QStringLiteral("dc_gb")},
            {"max_power_kw",   400.0},
            {"is_default",     0},
            {"created_time",   QStringLiteral("2025-05-08 08:55:00")}
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
        // —— 已预约 #1（即将开始·北京）——
        { 9101, 5, "自营·三里屯太古里站", "北京·朝阳", "T-03", "fast", "reserved",
          0.0, 85.0, 0.0, "2025-08-21 09:30:00", "", 0, 0.0, "flat", 0.0, 0.0, 0, "", 0,
          "2025-08-20 19:10:00", "" },
        // —— 已预约 #2（未来·上海）——
        { 9102, 7, "特来电·上海陆家嘴滨江站", "上海·浦东", "L-05", "slow", "reserved",
          0.0, 100.0, 0.0, "2025-08-22 22:00:00", "", 0, 0.0, "valley", 0.0, 0.0, 0, "", 0,
          "2025-08-20 18:45:00", "" },
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
          "2025-07-28 15:40:00", "" },
        // —— 新增：进行中（慢充）——
        { 9006, 7, "特来电·上海陆家嘴滨江站", "上海·浦东", "L-08", "slow", "charging",
          30.0, 100.0, 42.0, "2025-08-20 10:15:00", "", 0, 0.0, "flat", 0.0, 0.0, 0, "", 0,
          "2025-08-20 10:10:00", "" },
        // —— 新增：待结算（谷时段）——
        { 9007, 8, "自营·广州天河体育西站", "广州·天河", "B3", "fast", "pending_settle",
          8.0, 95.0, 95.0, "2025-08-18 23:40:00", "2025-08-19 00:35:00", 55, 56.3, "valley",
          30.4, 3.0, 0, "夜间充电券 ¥3", 61, "2025-08-18 23:35:00", "" },
        // —— 新增：已完成（尖峰）——
        { 9008, 3, "星星充·杭州西湖银泰站", "杭州·西湖", "C-05", "fast", "completed",
          22.0, 85.0, 85.0, "2025-08-17 19:05:00", "2025-08-17 19:47:00", 42, 33.1, "peak",
          36.4, 5.0, 200, "新人立减 ¥5 + 200 积分", 44, "2025-08-17 19:00:00", "2025-08-17 19:47:20" },
        // —— 新增：已完成（成都）——
        { 9009, 9, "自营·成都天府三街站", "成都·高新", "D1", "fast", "completed",
          15.0, 90.0, 90.0, "2025-08-13 12:30:00", "2025-08-13 13:08:00", 38, 30.5, "flat",
          20.1, 0.0, 0, "", 40, "2025-08-13 12:25:00", "2025-08-13 13:08:10" },
        // —— 新增：已完成（武汉慢充）——
        { 9010, 10, "国网·武汉光谷广场站", "武汉·东湖高新", "S-02", "slow", "completed",
          40.0, 95.0, 95.0, "2025-08-10 22:10:00", "2025-08-11 02:40:00", 270, 28.5, "valley",
          8.8, 0.0, 0, "", 18, "2025-08-10 22:05:00", "2025-08-11 02:40:30" },
        // —— 新增：已取消（超时未到）——
        { 9011, 12, "星星充·南京新街口站", "南京·鼓楼", "E-10", "fast", "cancelled",
          0.0, 80.0, 0.0, "", "", 0, 0.0, "", 0.0, 0.0, 0, "", 0,
          "2025-08-09 14:20:00", "" },
        // —— 新增：已完成（西安）——
        { 9012, 13, "自营·西安高新万达广场站", "西安·高新", "F6", "fast", "completed",
          25.0, 90.0, 90.0, "2025-08-06 20:15:00", "2025-08-06 20:58:00", 43, 29.8, "flat",
          19.7, 10.0, 0, "满 100 减 10", 39, "2025-08-06 20:10:00", "2025-08-06 20:58:30" },
        // —— 新增：已完成（重庆谷时段）——
        { 9013, 14, "国网·重庆解放碑站", "重庆·渝中", "G-03", "fast", "completed",
          10.0, 90.0, 90.0, "2025-08-01 00:10:00", "2025-08-01 00:52:00", 42, 33.6, "valley",
          10.6, 0.0, 0, "", 33, "2025-07-31 23:55:00", "2025-08-01 00:52:10" },
        // —— 新增：待结算（慢充·苏州）——
        { 9014, 15, "特来电·苏州金鸡湖站", "苏州·工业园", "H-12", "slow", "pending_settle",
          30.0, 100.0, 100.0, "2025-07-29 19:20:00", "2025-07-29 23:50:00", 270, 14.2, "flat",
          9.2, 0.0, 0, "", 15, "2025-07-29 19:15:00", "" },
        // —— 新增：已取消（用户主动）——
        { 9015, 16, "自营·天津滨海机场站", "天津·滨海", "J-07", "fast", "cancelled",
          0.0, 85.0, 0.0, "", "", 0, 0.0, "", 0.0, 0.0, 0, "", 0,
          "2025-07-25 08:05:00", "" },
        // —— 新增：已完成（老订单·青岛）——
        { 9016, 17, "星星充·青岛五四广场站", "青岛·市南", "M-09", "fast", "completed",
          28.0, 95.0, 95.0, "2025-07-20 17:40:00", "2025-07-20 18:28:00", 48, 30.4, "peak",
          33.5, 0.0, 0, "", 41, "2025-07-20 17:35:00", "2025-07-20 18:28:20" }
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
    if (orderId == 9007) {
        return {
            node("reserved",        QStringLiteral("预约成功"),       "2025-08-18 23:35:00", {}),
            node("arrived",         QStringLiteral("到达充电站"),     "2025-08-18 23:40:00", {}),
            node("started",         QStringLiteral("开始充电"),       "2025-08-18 23:40:00", QStringLiteral("起始电量 8%")),
            node("soc_50",          QStringLiteral("电量达到 50%"),   "2025-08-19 00:02:00", {}),
            node("target_reached",  QStringLiteral("达到目标电量"),   "2025-08-19 00:35:00", QStringLiteral("目标电量 95%")),
            node("finished",        QStringLiteral("充电结束"),       "2025-08-19 00:35:00", QStringLiteral("待结算 · 已使用夜间充电券"))
        };
    }
    if (orderId == 9008) {
        return {
            node("reserved",        QStringLiteral("预约成功"),       "2025-08-17 19:00:00", {}),
            node("arrived",         QStringLiteral("到达充电站"),     "2025-08-17 19:05:00", {}),
            node("started",         QStringLiteral("开始充电"),       "2025-08-17 19:05:00", QStringLiteral("起始电量 22%")),
            node("soc_50",          QStringLiteral("电量达到 50%"),   "2025-08-17 19:22:00", {}),
            node("target_reached",  QStringLiteral("达到目标电量"),   "2025-08-17 19:47:00", QStringLiteral("目标电量 85%")),
            node("finished",        QStringLiteral("充电结束"),       "2025-08-17 19:47:00", {}),
            node("settled",         QStringLiteral("自动结算"),       "2025-08-17 19:47:20", QStringLiteral("实付 31.40 元（含优惠券 ¥5 + 200 积分）"))
        };
    }
    if (orderId == 9009) {
        return {
            node("reserved",        QStringLiteral("预约成功"),       "2025-08-13 12:25:00", {}),
            node("arrived",         QStringLiteral("到达充电站"),     "2025-08-13 12:30:00", {}),
            node("started",         QStringLiteral("开始充电"),       "2025-08-13 12:30:00", QStringLiteral("起始电量 15%")),
            node("soc_50",          QStringLiteral("电量达到 50%"),   "2025-08-13 12:47:00", {}),
            node("target_reached",  QStringLiteral("达到目标电量"),   "2025-08-13 13:08:00", QStringLiteral("目标电量 90%")),
            node("finished",        QStringLiteral("充电结束"),       "2025-08-13 13:08:00", {}),
            node("settled",         QStringLiteral("自动结算"),       "2025-08-13 13:08:10", QStringLiteral("实付 20.10 元"))
        };
    }
    if (orderId == 9010) {
        return {
            node("reserved",        QStringLiteral("预约成功"),       "2025-08-10 22:05:00", {}),
            node("arrived",         QStringLiteral("到达充电站"),     "2025-08-10 22:10:00", {}),
            node("started",         QStringLiteral("开始慢充"),       "2025-08-10 22:10:00", QStringLiteral("起始电量 40% · 慢充谷电时段")),
            node("soc_50",          QStringLiteral("电量达到 50%"),   "2025-08-10 23:25:00", {}),
            node("soc_80",          QStringLiteral("电量达到 80%"),   "2025-08-11 01:15:00", {}),
            node("target_reached",  QStringLiteral("达到目标电量"),   "2025-08-11 02:40:00", QStringLiteral("目标电量 95%")),
            node("finished",        QStringLiteral("充电结束"),       "2025-08-11 02:40:00", {}),
            node("settled",         QStringLiteral("自动结算"),       "2025-08-11 02:40:30", QStringLiteral("实付 8.80 元（谷时优惠）"))
        };
    }
    if (orderId == 9012) {
        return {
            node("reserved",        QStringLiteral("预约成功"),       "2025-08-06 20:10:00", {}),
            node("arrived",         QStringLiteral("到达充电站"),     "2025-08-06 20:15:00", {}),
            node("started",         QStringLiteral("开始充电"),       "2025-08-06 20:15:00", QStringLiteral("起始电量 25%")),
            node("soc_50",          QStringLiteral("电量达到 50%"),   "2025-08-06 20:34:00", {}),
            node("target_reached",  QStringLiteral("达到目标电量"),   "2025-08-06 20:58:00", QStringLiteral("目标电量 90%")),
            node("finished",        QStringLiteral("充电结束"),       "2025-08-06 20:58:00", {}),
            node("settled",         QStringLiteral("自动结算"),       "2025-08-06 20:58:30", QStringLiteral("实付 9.70 元（已使用满 100 减 10）"))
        };
    }
    if (orderId == 9013) {
        return {
            node("reserved",        QStringLiteral("预约成功"),       "2025-07-31 23:55:00", {}),
            node("arrived",         QStringLiteral("到达充电站"),     "2025-08-01 00:10:00", {}),
            node("started",         QStringLiteral("开始充电"),       "2025-08-01 00:10:00", QStringLiteral("起始电量 10%")),
            node("soc_50",          QStringLiteral("电量达到 50%"),   "2025-08-01 00:30:00", {}),
            node("target_reached",  QStringLiteral("达到目标电量"),   "2025-08-01 00:52:00", QStringLiteral("目标电量 90%")),
            node("finished",        QStringLiteral("充电结束"),       "2025-08-01 00:52:00", {}),
            node("settled",         QStringLiteral("自动结算"),       "2025-08-01 00:52:10", QStringLiteral("实付 10.60 元"))
        };
    }
    if (orderId == 9014) {
        return {
            node("reserved",        QStringLiteral("预约成功"),       "2025-07-29 19:15:00", {}),
            node("arrived",         QStringLiteral("到达充电站"),     "2025-07-29 19:20:00", {}),
            node("started",         QStringLiteral("开始慢充"),       "2025-07-29 19:20:00", QStringLiteral("起始电量 30% · 慢充")),
            node("soc_50",          QStringLiteral("电量达到 50%"),   "2025-07-29 21:30:00", {}),
            node("target_reached",  QStringLiteral("达到目标电量"),   "2025-07-29 23:50:00", QStringLiteral("目标电量 100%")),
            node("finished",        QStringLiteral("充电结束"),       "2025-07-29 23:50:00", QStringLiteral("待结算"))
        };
    }
    if (orderId == 9016) {
        return {
            node("reserved",        QStringLiteral("预约成功"),       "2025-07-20 17:35:00", {}),
            node("arrived",         QStringLiteral("到达充电站"),     "2025-07-20 17:40:00", {}),
            node("started",         QStringLiteral("开始充电"),       "2025-07-20 17:40:00", QStringLiteral("起始电量 28%")),
            node("soc_50",          QStringLiteral("电量达到 50%"),   "2025-07-20 18:02:00", {}),
            node("target_reached",  QStringLiteral("达到目标电量"),   "2025-07-20 18:28:00", QStringLiteral("目标电量 95%")),
            node("finished",        QStringLiteral("充电结束"),       "2025-07-20 18:28:00", {}),
            node("settled",         QStringLiteral("自动结算"),       "2025-07-20 18:28:20", QStringLiteral("实付 33.50 元"))
        };
    }
    // —— 已预约：三里屯太古里站 T-03 ——
    if (orderId == 9101) {
        return {
            node("reserved",        QStringLiteral("预约成功"),       "2025-08-20 19:10:00", QStringLiteral("快充桩 T-03 · 目标电量 85%")),
            node("waiting_arrive",  QStringLiteral("等待到达"),       "2025-08-20 19:10:01", QStringLiteral("预约开始时间 08-21 09:30"))
        };
    }
    // —— 已预约：上海陆家嘴滨江站 L-05（谷时段慢充）——
    if (orderId == 9102) {
        return {
            node("reserved",        QStringLiteral("预约成功"),       "2025-08-20 18:45:00", QStringLiteral("慢充桩 L-05 · 目标电量 100% · 谷时段")),
            node("waiting_arrive",  QStringLiteral("等待到达"),       "2025-08-20 18:45:01", QStringLiteral("预约开始时间 08-22 22:00"))
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
        }),
        S({
            {"id",             5005},
            {"title",          QStringLiteral("首充专享立减 ¥10")},
            {"type",           QStringLiteral("first_charge")},
            {"discount_amount", 10.0},
            {"min_amount",     0.0},
            {"scope",          QStringLiteral("仅限首单使用")},
            {"time_range",     QString()},
            {"valid_until",    QStringLiteral("2025-11-30")},
            {"status",         QStringLiteral("unused")},
            {"receive_time",   QStringLiteral("2025-07-01 08:00:00")}
        }),
        S({
            {"id",             5006},
            {"title",          QStringLiteral("小鹏品牌专属券 ¥8")},
            {"type",           QStringLiteral("brand")},
            {"discount_amount", 8.0},
            {"min_amount",     20.0},
            {"scope",          QStringLiteral("小鹏车型专享")},
            {"time_range",     QString()},
            {"valid_until",    QStringLiteral("2025-10-15")},
            {"status",         QStringLiteral("unused")},
            {"receive_time",   QStringLiteral("2025-07-20 10:00:00")}
        }),
        S({
            {"id",             5007},
            {"title",          QStringLiteral("周末充电特惠 ¥6")},
            {"type",           QStringLiteral("weekend")},
            {"discount_amount", 6.0},
            {"min_amount",     15.0},
            {"scope",          QStringLiteral("周末 00:00–24:00")},
            {"time_range",     QStringLiteral("周末全天")},
            {"valid_until",    QStringLiteral("2025-09-30")},
            {"status",         QStringLiteral("unused")},
            {"receive_time",   QStringLiteral("2025-08-01 09:00:00")}
        }),
        S({
            {"id",             5008},
            {"title",          QStringLiteral("中秋限定立减 ¥12")},
            {"type",           QStringLiteral("festival")},
            {"discount_amount", 12.0},
            {"min_amount",     50.0},
            {"scope",          QStringLiteral("中秋假期专享")},
            {"time_range",     QStringLiteral("2025-09-06 至 2025-09-08")},
            {"valid_until",    QStringLiteral("2025-09-08")},
            {"status",         QStringLiteral("unused")},
            {"receive_time",   QStringLiteral("2025-08-20 10:00:00")}
        }),
        S({
            {"id",             5009},
            {"title",          QStringLiteral("慢充专属 8 折券")},
            {"type",           QStringLiteral("slow_charge")},
            {"discount_amount", 6.0},
            {"min_amount",     0.0},
            {"scope",          QStringLiteral("慢充枪专享")},
            {"time_range",     QString()},
            {"valid_until",    QStringLiteral("2025-11-20")},
            {"status",         QStringLiteral("used")},
            {"receive_time",   QStringLiteral("2025-05-20 12:00:00")}
        }),
        S({
            {"id",             5010},
            {"title",          QStringLiteral("生日专属 ¥15")},
            {"type",           QStringLiteral("birthday")},
            {"discount_amount", 15.0},
            {"min_amount",     0.0},
            {"scope",          QStringLiteral("当月通用")},
            {"time_range",     QString()},
            {"valid_until",    QStringLiteral("2025-08-31")},
            {"status",         QStringLiteral("expired")},
            {"receive_time",   QStringLiteral("2025-08-01 00:00:00")}
        }),
        S({
            {"id",             5011},
            {"title",          QStringLiteral("积分兑换满 30 减 3")},
            {"type",           QStringLiteral("point_exchange")},
            {"discount_amount", 3.0},
            {"min_amount",     30.0},
            {"scope",          QStringLiteral("通用")},
            {"time_range",     QString()},
            {"valid_until",    QStringLiteral("2025-12-15")},
            {"status",         QStringLiteral("unused")},
            {"receive_time",   QStringLiteral("2025-07-30 12:00:00")}
        }),
        S({
            {"id",             5012},
            {"title",          QStringLiteral("818 活动满 80 减 18")},
            {"type",           QStringLiteral("event")},
            {"discount_amount", 18.0},
            {"min_amount",     80.0},
            {"scope",          QStringLiteral("818 品牌日")},
            {"time_range",     QStringLiteral("2025-08-18 当天")},
            {"valid_until",    QStringLiteral("2025-08-18")},
            {"status",         QStringLiteral("used")},
            {"receive_time",   QStringLiteral("2025-08-10 10:00:00")}
        }),
        S({
            {"id",             5013},
            {"title",          QStringLiteral("老用户回馈 ¥20")},
            {"type",           QStringLiteral("vip_gift")},
            {"discount_amount", 20.0},
            {"min_amount",     100.0},
            {"scope",          QStringLiteral("VIP 会员专享")},
            {"time_range",     QString()},
            {"valid_until",    QStringLiteral("2026-01-31")},
            {"status",         QStringLiteral("unused")},
            {"receive_time",   QStringLiteral("2025-08-15 09:00:00")}
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
        S({{"id", 7005}, {"change", 58},  {"reason", QStringLiteral("charge")}, {"create_time", QStringLiteral("2025-07-20 18:40:00")}}),
        // —— 新增积分记录 ——
        S({{"id", 7006}, {"change", 61},  {"reason", QStringLiteral("charge")},   {"create_time", QStringLiteral("2025-08-19 00:40:00")}}),
        S({{"id", 7007}, {"change", 44},  {"reason", QStringLiteral("charge")},   {"create_time", QStringLiteral("2025-08-17 19:48:00")}}),
        S({{"id", 7008}, {"change", 40},  {"reason", QStringLiteral("charge")},   {"create_time", QStringLiteral("2025-08-13 13:09:00")}}),
        S({{"id", 7009}, {"change", 18},  {"reason", QStringLiteral("charge")},   {"create_time", QStringLiteral("2025-08-11 02:41:00")}}),
        S({{"id", 7010}, {"change", -100},{"reason", QStringLiteral("redeem")},   {"create_time", QStringLiteral("2025-08-08 10:20:00")}}),
        S({{"id", 7011}, {"change", 39},  {"reason", QStringLiteral("charge")},   {"create_time", QStringLiteral("2025-08-06 20:59:00")}}),
        S({{"id", 7012}, {"change", 33},  {"reason", QStringLiteral("charge")},   {"create_time", QStringLiteral("2025-08-01 00:53:00")}}),
        S({{"id", 7013}, {"change", 50},  {"reason", QStringLiteral("sign_in")},  {"create_time", QStringLiteral("2025-07-28 08:00:00")}}),
        S({{"id", 7014}, {"change", 15},  {"reason", QStringLiteral("charge")},   {"create_time", QStringLiteral("2025-07-29 23:55:00")}}),
        S({{"id", 7015}, {"change", 41},  {"reason", QStringLiteral("charge")},   {"create_time", QStringLiteral("2025-07-20 18:29:00")}}),
        S({{"id", 7016}, {"change", -500},{"reason", QStringLiteral("redeem")},   {"create_time", QStringLiteral("2025-07-15 14:30:00")}})
    };
}

// —— 钱包流水（wallet_transaction）——
QVariantList buildWallet() {
    return {
        S({{"id", 8001}, {"type", QStringLiteral("recharge")}, {"amount", 100.0}, {"balance_after", 186.50}, {"remark", QStringLiteral("余额充值")}, {"create_time", QStringLiteral("2025-08-19 20:50:00")}}),
        S({{"id", 8002}, {"type", QStringLiteral("consume")},  {"amount", -14.50}, {"balance_after", 86.50},  {"remark", QStringLiteral("充电结算（订单 9002）")}, {"create_time", QStringLiteral("2025-08-19 21:52:00")}}),
        S({{"id", 8003}, {"type", QStringLiteral("recharge")}, {"amount", 50.0},  {"balance_after", 101.0},  {"remark", QStringLiteral("余额充值")}, {"create_time", QStringLiteral("2025-08-10 09:00:00")}}),
        S({{"id", 8004}, {"type", QStringLiteral("consume")},  {"amount", -16.20}, {"balance_after", 51.0},   {"remark", QStringLiteral("充电结算（订单 9003）")}, {"create_time", QStringLiteral("2025-08-15 09:58:00")}}),
        // —— 新增钱包流水 ——
        S({{"id", 8005}, {"type", QStringLiteral("consume")},  {"amount", -27.40}, {"balance_after", 86.50},  {"remark", QStringLiteral("充电结算（订单 9007）")},     {"create_time", QStringLiteral("2025-08-19 00:35:00")}}),
        S({{"id", 8006}, {"type", QStringLiteral("recharge")}, {"amount", 200.0}, {"balance_after", 310.70}, {"remark", QStringLiteral("支付宝充值 ¥200")},             {"create_time", QStringLiteral("2025-08-17 18:30:00")}}),
        S({{"id", 8007}, {"type", QStringLiteral("consume")},  {"amount", -31.40}, {"balance_after", 279.30}, {"remark", QStringLiteral("充电结算（订单 9008）")},     {"create_time", QStringLiteral("2025-08-17 19:47:00")}}),
        S({{"id", 8008}, {"type", QStringLiteral("consume")},  {"amount", -20.10}, {"balance_after", 259.20}, {"remark", QStringLiteral("充电结算（订单 9009）")},     {"create_time", QStringLiteral("2025-08-13 13:08:00")}}),
        S({{"id", 8009}, {"type", QStringLiteral("consume")},  {"amount", -8.80},  {"balance_after", 238.30}, {"remark", QStringLiteral("充电结算（订单 9010）")},     {"create_time", QStringLiteral("2025-08-11 02:40:00")}}),
        S({{"id", 8010}, {"type", QStringLiteral("refund")},   {"amount", 5.0},    {"balance_after", 247.10}, {"remark", QStringLiteral("预约取消退款（订单 9011）")}, {"create_time", QStringLiteral("2025-08-09 14:50:00")}}),
        S({{"id", 8011}, {"type", QStringLiteral("consume")},  {"amount", -9.70},  {"balance_after", 237.40}, {"remark", QStringLiteral("充电结算（订单 9012）")},     {"create_time", QStringLiteral("2025-08-06 20:58:00")}}),
        S({{"id", 8012}, {"type", QStringLiteral("consume")},  {"amount", -10.60}, {"balance_after", 226.80}, {"remark", QStringLiteral("充电结算（订单 9013）")},     {"create_time", QStringLiteral("2025-08-01 00:52:00")}}),
        S({{"id", 8013}, {"type", QStringLiteral("recharge")}, {"amount", 30.0},   {"balance_after", 83.90},  {"remark", QStringLiteral("微信充值 ¥30")},               {"create_time", QStringLiteral("2025-07-25 21:10:00")}}),
        S({{"id", 8014}, {"type", QStringLiteral("consume")},  {"amount", -33.50}, {"balance_after", 53.90},  {"remark", QStringLiteral("充电结算（订单 9016）")},     {"create_time", QStringLiteral("2025-07-20 18:28:00")}})
    };
}

// —— 站内消息（notification）——
QVariantList buildNotifications() {
    return {
        S({{"id", 6001}, {"type", QStringLiteral("order")},       {"title", QStringLiteral("充电完成")},          {"content", QStringLiteral("您在中关村软件园旗舰站的充电已完成，请及时结算。")}, {"is_read", 0}, {"create_time", QStringLiteral("2025-08-19 21:52:00")}}),
        S({{"id", 6002}, {"type", QStringLiteral("coupon")},      {"title", QStringLiteral("优惠券到账")},        {"content", QStringLiteral("您已获得「满 50 减 5」优惠券，有效期至 2025-10-31。")}, {"is_read", 0}, {"create_time", QStringLiteral("2025-06-01 10:00:00")}}),
        S({{"id", 6003}, {"type", QStringLiteral("point")},       {"title", QStringLiteral("积分到账")},          {"content", QStringLiteral("本单获得 32 积分，当前累计 1280 积分。")}, {"is_read", 1}, {"create_time", QStringLiteral("2025-08-15 09:58:00")}}),
        S({{"id", 6004}, {"type", QStringLiteral("reservation")}, {"title", QStringLiteral("排队提醒")},          {"content", QStringLiteral("您预约的充电桩即将轮到，请前往。")}, {"is_read", 1}, {"create_time", QStringLiteral("2025-07-28 15:42:00")}}),
        // —— 新增站内消息 ——
        S({{"id", 6005}, {"type", QStringLiteral("order")},       {"title", QStringLiteral("充电进行中")},        {"content", QStringLiteral("您在国贸中心旗舰站的 A-01 号桩已开始充电，预计 35 分钟后充满。")}, {"is_read", 0}, {"create_time", QStringLiteral("2025-08-20 18:05:00")}}),
        S({{"id", 6006}, {"type", QStringLiteral("system")},      {"title", QStringLiteral("系统升级通知")},      {"content", QStringLiteral("8 月 22 日 02:00–04:00 将进行系统升级，期间部分服务可能短暂中断。")}, {"is_read", 0}, {"create_time", QStringLiteral("2025-08-20 10:00:00")}}),
        S({{"id", 6007}, {"type", QStringLiteral("coupon")},      {"title", QStringLiteral("中秋活动已开启")},    {"content", QStringLiteral("中秋限定立减 ¥12 优惠券已发放至您的卡包，中秋假期可用。")}, {"is_read", 0}, {"create_time", QStringLiteral("2025-08-20 10:05:00")}}),
        S({{"id", 6008}, {"type", QStringLiteral("point")},       {"title", QStringLiteral("连续签到奖励")},      {"content", QStringLiteral("已连续签到 7 天，额外奖励 50 积分已入账。")}, {"is_read", 1}, {"create_time", QStringLiteral("2025-07-28 08:01:00")}}),
        S({{"id", 6009}, {"type", QStringLiteral("reservation")}, {"title", QStringLiteral("预约成功")},          {"content", QStringLiteral("已为您预约广州天河体育西站 B3 号快充桩，请于 23:40 前到达。")}, {"is_read", 1}, {"create_time", QStringLiteral("2025-08-18 23:35:00")}}),
        S({{"id", 6010}, {"type", QStringLiteral("promotion")},   {"title", QStringLiteral("818 品牌日来袭")},    {"content", QStringLiteral("8 月 18 日下单享满 80 减 18，全平台通用，数量有限先到先得。")}, {"is_read", 1}, {"create_time", QStringLiteral("2025-08-10 10:00:00")}}),
        S({{"id", 6011}, {"type", QStringLiteral("order")},       {"title", QStringLiteral("结算成功")},          {"content", QStringLiteral("订单 9008 已自动结算，实付 31.40 元，已使用优惠券与积分。")}, {"is_read", 1}, {"create_time", QStringLiteral("2025-08-17 19:47:30")}}),
        S({{"id", 6012}, {"type", QStringLiteral("wallet")},      {"title", QStringLiteral("充值成功")},          {"content", QStringLiteral("您已通过支付宝充值 ¥200，当前余额 310.70 元。")}, {"is_read", 1}, {"create_time", QStringLiteral("2025-08-17 18:30:30")}}),
        S({{"id", 6013}, {"type", QStringLiteral("security")},    {"title", QStringLiteral("异地登录提醒")},      {"content", QStringLiteral("检测到您的账号在上海浦东新设备登录，若非本人操作请及时修改密码。")}, {"is_read", 1}, {"create_time", QStringLiteral("2025-08-20 10:15:00")}}),
        S({{"id", 6014}, {"type", QStringLiteral("vip")},         {"title", QStringLiteral("VIP 会员续费提醒")},  {"content", QStringLiteral("您的 VIP 会员将于 2025-12-31 到期，续费可享专属优惠。")}, {"is_read", 0}, {"create_time", QStringLiteral("2025-08-18 09:00:00")}})
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

// —— 会员套餐模板（member_plan 表）——
QVariantList buildMemberPlans() {
    return {
        S({
            {"id",                  1},
            {"name",                QStringLiteral("周卡体验版")},
            {"price",               9.90},
            {"valid_days",          7},
            {"service_fee_discount", 0.95},
            {"night_discount",      0.90},
            {"points_multiplier",   1.2},
            {"status",              QStringLiteral("active")},
            {"description",         QStringLiteral("服务费 9.5 折 · 夜间 9 折 · 积分 1.2 倍")},
            {"perks",               QStringList{QStringLiteral("服务费 9.5 折"), QStringLiteral("夜间充电 9 折"), QStringLiteral("积分 1.2 倍")}}
        }),
        S({
            {"id",                  2},
            {"name",                QStringLiteral("月卡标准版")},
            {"price",               29.90},
            {"valid_days",          30},
            {"service_fee_discount", 0.85},
            {"night_discount",      0.80},
            {"points_multiplier",   1.5},
            {"status",              QStringLiteral("active")},
            {"description",         QStringLiteral("服务费 8.5 折 · 夜间 8 折 · 积分 1.5 倍 · 专属客服")},
            {"perks",               QStringList{QStringLiteral("服务费 8.5 折"), QStringLiteral("夜间充电 8 折"), QStringLiteral("积分 1.5 倍"), QStringLiteral("专属客服通道"), QStringLiteral("每月 2 张免费停车券")}}
        }),
        S({
            {"id",                  3},
            {"name",                QStringLiteral("季卡尊享版")},
            {"price",               79.00},
            {"valid_days",          90},
            {"service_fee_discount", 0.75},
            {"night_discount",      0.70},
            {"points_multiplier",   1.8},
            {"status",              QStringLiteral("active")},
            {"description",         QStringLiteral("服务费 7.5 折 · 夜间 7 折 · 积分 1.8 倍 · 生日礼包")},
            {"perks",               QStringList{QStringLiteral("服务费 7.5 折"), QStringLiteral("夜间充电 7 折"), QStringLiteral("积分 1.8 倍"), QStringLiteral("专属客服"), QStringLiteral("生日礼包 ¥30"), QStringLiteral("每月 5 张停车券"), QStringLiteral("免预约排队 2 次/月")}}
        }),
        S({
            {"id",                  4},
            {"name",                QStringLiteral("年卡特惠版")},
            {"price",               299.00},
            {"valid_days",          365},
            {"service_fee_discount", 0.65},
            {"night_discount",      0.60},
            {"points_multiplier",   2.0},
            {"status",              QStringLiteral("active")},
            {"description",         QStringLiteral("服务费 6.5 折 · 夜间 6 折 · 积分 2 倍 · 免预约特权")},
            {"perks",               QStringList{QStringLiteral("服务费 6.5 折"), QStringLiteral("夜间充电 6 折"), QStringLiteral("积分 2 倍"), QStringLiteral("专属VIP客服"), QStringLiteral("季度大礼包"), QStringLiteral("免预约不限次"), QStringLiteral("全年停车券免费")}}
        })
    };
}

}  // namespace

UserData::UserData(QObject* parent)
    : QObject(parent), m_nickname(QStringLiteral("用户0001")), m_avatarPath(QString()), m_balance(86.50) {}

QVariantMap UserData::profile() const {
    return S({
        {"id",              3},
        {"phone",           kPhone},
        {"nickname",        m_nickname},
        {"avatar_path",     m_avatarPath},
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

bool UserData::updateAvatar(const QString& avatarPath) {
    m_avatarPath = avatarPath;
    emit profileChanged();
    return true;
}

double UserData::totalEnergyKwh() const {
    double total = 0.0;
    for (const auto& v : orders()) {
        auto m = v.toMap();
        // 已完成 + 待结算 的订单计入累计用电量
        QString status = m.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("completed") || status == QStringLiteral("pending_settle"))
            total += m.value(QStringLiteral("energy_kwh")).toDouble();
    }
    return total;
}

QVariantList UserData::memberPlans() const {
    static const QVariantList s = buildMemberPlans();
    return s;
}

QVariantMap UserData::currentPlan() const {
    for (const auto& v : memberPlans()) {
        auto m = v.toMap();
        if (m.value(QStringLiteral("id")).toInt() == m_currentPlanId) {
            return S({
                {"plan_id",           m_currentPlanId},
                {"name",              m.value(QStringLiteral("name"))},
                {"price",             m.value(QStringLiteral("price"))},
                {"valid_days",        m.value(QStringLiteral("valid_days"))},
                {"service_fee_discount", m.value(QStringLiteral("service_fee_discount"))},
                {"night_discount",    m.value(QStringLiteral("night_discount"))},
                {"points_multiplier", m.value(QStringLiteral("points_multiplier"))},
                {"description",       m.value(QStringLiteral("description"))},
                {"start_time",        QStringLiteral("2025-08-01 00:00:00")},
                {"end_time",          QStringLiteral("2025-08-31 23:59:59")},
                {"days_left",         11},
                {"status",            QStringLiteral("active")}
            });
        }
    }
    return {};
}

bool UserData::subscribePlan(int planId) {
    // 简单模拟：只要在套餐列表里就订阅成功
    for (const auto& v : memberPlans()) {
        auto m = v.toMap();
        if (m.value(QStringLiteral("id")).toInt() == planId
            && m.value(QStringLiteral("status")).toString() == QStringLiteral("active")) {
            m_currentPlanId = planId;
            emit profileChanged();
            return true;
        }
    }
    return false;
}