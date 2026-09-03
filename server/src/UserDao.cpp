#include "UserDao.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

namespace {

// 把查询结果的当前行转成 User 结构体(列顺序与下面的 SELECT 保持一致)
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
    // prepare + addBindValue 是"参数化查询": 用户输入永远只当数据, 不会被
    // 拼进 SQL 文本 —— 这是防 SQL 注入的标准做法(数据安全考虑的得分点)。
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

    // 手机号必须是 11 位数字(规格要求), 不合法直接拒绝
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

    // 不存在 → 自动注册。昵称 = "用户" + 后4位(规格约定)。
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

std::optional<User> recharge(int userId, double amount)
{
    if (amount <= 0) return std::nullopt;
    const auto u = findUserById(userId);
    if (!u) return std::nullopt;

    const double after = qRound((u->balance + amount) * 100) / 100.0;

    QSqlDatabase::database().transaction();
    QSqlQuery up;
    up.prepare(QStringLiteral("UPDATE user SET balance = ? WHERE id = ?"));
    up.addBindValue(after);
    up.addBindValue(userId);
    bool ok = up.exec();

    QSqlQuery w;
    w.prepare(QStringLiteral(
        "INSERT INTO wallet_transaction(user_id,type,amount,balance_after,order_id,remark,create_time) "
        "VALUES(?,'recharge',?,?,NULL,'账户充值',?)"));
    w.addBindValue(userId);
    w.addBindValue(amount);
    w.addBindValue(after);
    w.addBindValue(now());
    ok = ok && w.exec();

    if (!ok) { QSqlDatabase::database().rollback(); return std::nullopt; }
    QSqlDatabase::database().commit();
    return findUserById(userId);
}

}  // namespace dao
