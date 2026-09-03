#pragma once
#include <QList>
#include <QString>
#include <optional>

// ============================================================
// AdminDao — 管理端相关查询/操作
// 对应协议消息 admin.* 的数据来源。
// 字段命名与 admin/src/network/MockDataProvider.cpp 一比一对齐,
// 管理端从 mock 切到真实服务端时页面代码不需要任何改动。
// ============================================================

struct AdminAccount {
    int     id = 0;
    QString account;
};

struct RevenuePoint {
    QString date;        // yyyy-MM-dd
    double  amount = 0;  // 当日实收(元)
};

struct RevenueSummary {
    QList<RevenuePoint> trend;   // 按日期升序, 无订单的日子补 0
    double today = 0;            // 今日实收
    double month = 0;            // 本月实收
    double total = 0;            // 历史累计实收
};

// 电桩状态分布(管理端首页环形图)
struct StatusDistribution {
    int charging = 0, idle = 0, fault = 0, offline = 0, reserved = 0, rebooting = 0;
    int total = 0;
};

// 电站全字段(管理端表格用, 比 StationView 多几列)
struct StationFull {
    int     id = 0;
    QString name, address, area;
    double  longitude = 0, latitude = 0;
    int     totalChargers = 0;
    double  onlineRate = 100;      // 在线率%: 由 charger 实时统计(非 idle 之外的在线比例)
    double  serviceFee = 0, parkingFee = 0;
    QString businessHours;
    QString facilitiesJson;        // 库里存的是 JSON 文本, 由 WsServer 解析成数组
    QString ownerType;
    int     hasSwap = 0;
    int     freeChargers = 0;      // 实时统计
};

// 电桩全字段
struct ChargerFull {
    int     id = 0;
    QString code;
    int     stationId = 0;
    QString stationName;           // JOIN station 得到, 管理端表格直接显示
    QString type;
    double  power = 0;
    QString status;
    double  voltage = 0, current = 0, temperature = 0;
    QString faultCode, commStatus;
    int     healthScore = 100, totalChargeCount = 0, totalChargeDuration = 0;
    QString createdTime;
};

// 用户全字段(管理端用户管理页)
struct UserRow {
    int     id = 0;
    QString phone, nickname, avatarPath;
    double  balance = 0;
    int     points = 0;
    QString level, status, registerTime, lastLoginTime;
};

struct DeviceLogRow {
    int     id = 0, chargerId = 0;
    QString action, op, opTime, result;   // op = operator(管理员账号); operator 是 C++ 关键字
};

struct RiskRow {
    int     chargerId = 0, healthScore = 0;
    QString riskLevel, suggestion;
};

namespace dao {

// ---- 认证 ----
// 账号密码校验。成功返回 admin 信息, 失败(账号不存在或密码错)返回 nullopt。
std::optional<AdminAccount> adminLogin(const QString &account, const QString &password);

// ---- 统计 ----
// 近 days 天营收趋势 + 今日/本月/累计。无订单的日子补 0, 保证前端折线不断。
RevenueSummary revenue(int days);

// 全部电桩的状态分布。
StatusDistribution chargerStatusDistribution();

// 健康分最低的前 limit 台电桩(故障风险排序)。
QList<RiskRow> faultRisks(int limit);

// ---- 列表 ----
QList<StationFull> listStationsFull();
std::optional<StationFull> findStationFullById(int stationId);

// stationId <= 0 表示全部电桩。
QList<ChargerFull> listChargers(int stationId);

// keyword 为空表示全部; 否则按手机号/昵称模糊匹配。
QList<UserRow>     listUsers(const QString &keyword);
std::optional<UserRow> findUserRowById(int userId);

// 某台电桩的运维日志(按时间倒序)。
QList<DeviceLogRow> listDeviceLogs(int chargerId);

// ---- 写操作 ----
// 对电桩执行运维动作并记一条 device_log。
//   action = "restart" → status 置 rebooting
//   action = "pause"   → status 置 offline
// 电桩不存在返回 nullopt。成功返回新写入的日志行。
std::optional<DeviceLogRow> chargerAction(int chargerId, const QString &action,
                                          const QString &opAccount);

// 冻结/解冻用户。status 只能是 normal / frozen。
std::optional<UserRow> setUserStatus(int userId, const QString &status);

// 新增电站。返回新建的电站(含自增 id)。
std::optional<StationFull> addStation(const StationFull &s);

}  // namespace dao
