#include "UserDao.h"

#include <QDateTime>
#include <QStringList>
#include <QVariantList>
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

// 缺失的 JSON key 会变成 null QString, 绑进 NOT NULL 列会整条插入失败
QString nz(const QString &s)
{
    return s.isNull() ? QString::fromLatin1("") : s;
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

// ==================== 修改资料 ====================
std::optional<User> updateProfile(int userId, const QString &nickname, const QString &avatarPath)
{
    if (nickname.trimmed().isEmpty() && avatarPath.isEmpty())
        return findUserById(userId);          // 什么都没传, 当作空操作

    QStringList sets;
    QVariantList binds;
    if (!nickname.trimmed().isEmpty()) {
        sets << QStringLiteral("nickname = ?");
        binds << nickname.trimmed();
    }
    if (!avatarPath.isEmpty()) {
        sets << QStringLiteral("avatar_path = ?");
        binds << avatarPath;
    }
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE user SET %1 WHERE id = ?").arg(sets.join(QStringLiteral(", "))));
    for (const auto &b : binds) q.addBindValue(b);
    q.addBindValue(userId);
    if (!q.exec() || q.numRowsAffected() == 0)
        return std::nullopt;
    return findUserById(userId);
}

// ==================== 我的车辆 ====================
namespace {

Vehicle rowToVehicle(const QSqlQuery &q)
{
    Vehicle v;
    v.id            = q.value(0).toInt();
    v.userId        = q.value(1).toInt();
    v.name          = q.value(2).toString();
    v.type          = q.value(3).toString();
    v.batteryKwh    = q.value(4).toDouble();
    v.connectorType = q.value(5).toString();
    v.maxPowerKw    = q.value(6).toDouble();
    v.isDefault     = q.value(7).toInt();
    v.createdTime   = q.value(8).toString();
    return v;
}

const char *kSelectVehicle =
    "SELECT id, user_id, name, type, battery_kwh, connector_type, max_power_kw, "
    "       is_default, created_time FROM vehicle ";

// 保证每个用户至多一辆默认车(schema 注释里写明"业务层保证")
void keepSingleDefault(int userId, int keepId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE vehicle SET is_default=0 WHERE user_id=? AND id<>?"));
    q.addBindValue(userId);
    q.addBindValue(keepId);
    q.exec();
}

}  // namespace

QList<Vehicle> listVehicles(int userId)
{
    QList<Vehicle> out;
    QSqlQuery q;
    q.prepare(QString::fromLatin1(kSelectVehicle)
              + QStringLiteral("WHERE user_id=? ORDER BY is_default DESC, id"));
    q.addBindValue(userId);
    if (!q.exec()) return out;
    while (q.next()) out.append(rowToVehicle(q));
    return out;
}

std::optional<Vehicle> findVehicle(int userId, int vehicleId)
{
    QSqlQuery q;
    q.prepare(QString::fromLatin1(kSelectVehicle) + QStringLiteral("WHERE id=? AND user_id=?"));
    q.addBindValue(vehicleId);
    q.addBindValue(userId);
    if (!q.exec() || !q.next()) return std::nullopt;
    return rowToVehicle(q);
}

std::optional<Vehicle> addVehicle(const Vehicle &v)
{
    // 第一辆车自动设为默认
    const bool first = listVehicles(v.userId).isEmpty();
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO vehicle(user_id,name,type,battery_kwh,connector_type,max_power_kw,"
        "  is_default,created_time) VALUES(?,?,?,?,?,?,?,?)"));
    q.addBindValue(v.userId);
    q.addBindValue(v.name.trimmed().isEmpty() ? QStringLiteral("我的车") : v.name.trimmed());
    q.addBindValue(v.type.isEmpty() ? QStringLiteral("car") : v.type);
    q.addBindValue(v.batteryKwh > 0 ? v.batteryKwh : 60.0);
    q.addBindValue(v.connectorType.isEmpty() ? QStringLiteral("dc_gb") : v.connectorType);
    q.addBindValue(v.maxPowerKw > 0 ? v.maxPowerKw : 120.0);
    q.addBindValue((first || v.isDefault) ? 1 : 0);
    q.addBindValue(now());
    if (!q.exec()) return std::nullopt;
    const int id = q.lastInsertId().toInt();
    if (first || v.isDefault) keepSingleDefault(v.userId, id);
    return findVehicle(v.userId, id);
}

std::optional<Vehicle> updateVehicle(int userId, int vehicleId, const Vehicle &v)
{
    if (!findVehicle(userId, vehicleId)) return std::nullopt;   // 不存在或不是你的
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE vehicle SET name=?, type=?, battery_kwh=?, connector_type=?, max_power_kw=?, "
        "  is_default=? WHERE id=? AND user_id=?"));
    q.addBindValue(nz(v.name));
    q.addBindValue(nz(v.type).isEmpty() ? QStringLiteral("car") : nz(v.type));
    q.addBindValue(v.batteryKwh > 0 ? v.batteryKwh : 60.0);
    q.addBindValue(nz(v.connectorType).isEmpty() ? QStringLiteral("dc_gb") : nz(v.connectorType));
    q.addBindValue(v.maxPowerKw);
    q.addBindValue(v.isDefault ? 1 : 0);
    q.addBindValue(vehicleId);
    q.addBindValue(userId);
    if (!q.exec()) return std::nullopt;
    if (v.isDefault) keepSingleDefault(userId, vehicleId);
    return findVehicle(userId, vehicleId);
}

bool deleteVehicle(int userId, int vehicleId, bool *busyOut)
{
    if (busyOut) *busyOut = false;
    const auto v = findVehicle(userId, vehicleId);
    if (!v) return false;

    // 有进行中订单绑着这辆车就不能删 —— 否则订单会指向一辆不存在的车,
    // 外键还会直接拦下删除, 报出难懂的错。
    QSqlQuery busy;
    busy.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM charging_order WHERE vehicle_id=? "
        "AND status IN ('reserved','charging','pending_settle')"));
    busy.addBindValue(vehicleId);
    if (busy.exec() && busy.next() && busy.value(0).toInt() > 0) {
        if (busyOut) *busyOut = true;
        return false;
    }
    // 历史订单也引用了这辆车(外键), 所以先把它们的 vehicle_id 置空再删
    QSqlDatabase::database().transaction();
    QSqlQuery detach;
    detach.prepare(QStringLiteral("UPDATE charging_order SET vehicle_id=NULL WHERE vehicle_id=?"));
    detach.addBindValue(vehicleId);
    bool ok = detach.exec();

    QSqlQuery del;
    del.prepare(QStringLiteral("DELETE FROM vehicle WHERE id=? AND user_id=?"));
    del.addBindValue(vehicleId);
    del.addBindValue(userId);
    ok = ok && del.exec() && del.numRowsAffected() > 0;
    if (!ok) { QSqlDatabase::database().rollback(); return false; }
    QSqlDatabase::database().commit();

    // 删掉的是默认车 → 把剩下的第一辆设为默认
    if (v->isDefault) {
        const auto rest = listVehicles(userId);
        if (!rest.isEmpty()) {
            QSqlQuery s;
            s.prepare(QStringLiteral("UPDATE vehicle SET is_default=1 WHERE id=?"));
            s.addBindValue(rest.first().id);
            s.exec();
        }
    }
    return true;
}

}  // namespace dao
