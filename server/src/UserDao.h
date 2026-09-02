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

namespace dao {

// 按手机号查用户。查到返回 User, 查不到返回 std::nullopt。
std::optional<User> findUserByPhone(const QString &phone);

// 免密登录: 手机号存在 → 更新 last_login_time 并返回;
//           不存在   → 自动注册(昵称 = "用户" + 手机号后4位)再返回。
// created 不为空时回写"本次是否新注册"。
// 对应协议消息 user.login (见 docs/content/spec-协议.md)。
std::optional<User> loginOrRegister(const QString &phone, bool *created = nullptr);

}  // namespace dao
