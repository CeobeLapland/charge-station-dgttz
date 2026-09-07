#pragma once
#include <QString>
#include <optional>

// ============================================================
// UserDao — 用户表的数据访问层 (Data Access Object)
// 约定: 只有 DAO 层允许写 SQL, 服务端业务逻辑一律调这里的函数,
//       这样字段名/SQL 只维护一份, 也方便统一加错误处理。
// 字段与 docs/content/spec-数据库.md 的 user 表一一对应。
// ============================================================

struct User {
    int     id = 0;
    QString phone;
    QString nickname;
    QString avatarPath;
    double  balance = 0.0;
    int     points  = 0;
    QString level;    // normal / vip / enterprise
    QString status;   // normal / frozen
};

// 我的车辆(vehicle 表)
struct Vehicle {
    int     id = 0, userId = 0;
    QString name, type, connectorType;
    double  batteryKwh = 60, maxPowerKw = 120;
    int     isDefault = 0;
    QString createdTime;
};

namespace dao {

// 按手机号查用户。查到返回 User, 查不到返回 std::nullopt。
std::optional<User> findUserByPhone(const QString &phone);

// 按主键 id 查用户。会话绑定后, 服务端一律用 id 取当前用户(见 WsServer::userIdOf)。
std::optional<User> findUserById(int id);

// 免密登录: 手机号存在 → 更新 last_login_time 并返回;
//           不存在   → 自动注册(昵称 = "用户" + 手机号后4位)再返回。
// created 不为空时回写"本次是否新注册"。
// 对应协议消息 user.login (见 docs/content/spec-协议.md)。
std::optional<User> loginOrRegister(const QString &phone, bool *created = nullptr);

// 余额充值(模拟支付)。amount 必须 > 0。
// 事务内同时: 加余额 + 记一条 wallet_transaction 流水 —— 余额是"现在多少钱",
// 流水是"怎么变成这样的", 两者必须一起写。
// 成功返回充值后的用户; 金额非法或用户不存在返回 nullopt。
std::optional<User> recharge(int userId, double amount);

// 修改昵称/头像(对应 user.update_profile)。两个参数都为空则不做任何修改。
std::optional<User> updateProfile(int userId, const QString &nickname, const QString &avatarPath);

// ---- 我的车辆(对应 vehicle.add / list / update / delete) ----
QList<Vehicle>          listVehicles(int userId);
std::optional<Vehicle>  findVehicle(int userId, int vehicleId);
std::optional<Vehicle>  addVehicle(const Vehicle &v);
std::optional<Vehicle>  updateVehicle(int userId, int vehicleId, const Vehicle &v);
// 删除车辆。有进行中订单占用时返回 false(busyOut 回填 true)。
bool                    deleteVehicle(int userId, int vehicleId, bool *busyOut = nullptr);

}  // namespace dao
