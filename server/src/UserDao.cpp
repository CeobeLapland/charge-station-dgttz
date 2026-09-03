#include "UserDao.h"

#include <QDateTime>
#include <QSqlQuery>
#include <QVariant>

namespace {

// 把查询结果的当前行转成User结构体
User rowToUser(const QSqlQuery &q)
{
    User u;
    u.id         = q.value(0).toInt();
    u.phone      = q.value(1).toString();
    u.nickname   = q.value(2).toString();
    u.avatarPath = q.value(3).toString();
    u.balance    = q.value(4).toDouble();
    u.points     = q.value(5).toInt();
    u.level      = q.value(6).toString();
    u.status     = q.value(7).toString();
    return u;
}

const char *kSelectUserByPhone =
    "SELECT id, phone, nickname, avatar_path, balance, points, level, status "
    "FROM user WHERE phone = ?";

const char *kSelectUserById =
    "SELECT id, phone, nickname, avatar_path, balance, points, level, status "
    "FROM user WHERE id = ?";

QString now()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

}  // namespace

namespace dao {

std::optional<User> findUserByPhone(const QString &phone)
{
    QSqlQuery q;
    q.prepare(kSelectUserByPhone);
    q.addBindValue(phone);
    if (!q.exec() || !q.next())
        return std::nullopt;
    return rowToUser(q);
}

std::optional<User> findUserById(int id)
{
    QSqlQuery q;
    q.prepare(kSelectUserById);
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return std::nullopt;
    return rowToUser(q);
}

std::optional<User> loginOrRegister(const QString &phone, bool *created)
{
    if (created) *created = false;

    // 手机号必须是 11 位数字
    if (phone.size() != 11 || phone.toLongLong() == 0)
        return std::nullopt;

    if (auto existing = findUserByPhone(phone)) {
        QSqlQuery touch;
        touch.prepare(QStringLiteral("UPDATE user SET last_login_time = ? WHERE id = ?"));
        touch.addBindValue(now());
        touch.addBindValue(existing->id);
        touch.exec();
        return existing;
    }

    // 不存在 → 自动注册 昵称 = "用户" + 后4位
    QSqlQuery ins;
    ins.prepare(QStringLiteral(
        "INSERT INTO user(phone, nickname, register_time, last_login_time) "
        "VALUES(?, ?, ?, ?)"));
    ins.addBindValue(phone);
    ins.addBindValue(QStringLiteral("用户") + phone.right(4));
    ins.addBindValue(now());
    ins.addBindValue(now());
    if (!ins.exec())
        return std::nullopt;

    if (created) *created = true;
    return findUserByPhone(phone);
}

}  // namespace dao
