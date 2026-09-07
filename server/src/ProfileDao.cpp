#include "ProfileDao.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <cmath>

namespace {

QString nowStr()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

// 缺失的 JSON key 取出来是 null QString, 绑进 NOT NULL 列会整条插入失败
QString nz(const QString &s)
{
    return s.isNull() ? QString::fromLatin1("") : s;
}

double scalarOf(const QString &sql, const QVariantList &binds = {})
{
    QSqlQuery q;
    q.prepare(sql);
    for (const auto &b : binds) q.addBindValue(b);
    if (!q.exec() || !q.next()) return 0.0;
    return q.value(0).toDouble();
}

}  // namespace

namespace dao {

// ==================== 收藏 ====================
QList<FavoriteRow> listFavorites(int userId)
{
    QList<FavoriteRow> out;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT f.id, s.id, s.name, s.address, s.area, f.create_time, "
        "       s.longitude, s.latitude, s.service_fee, "
        "       (SELECT COUNT(*) FROM charger c WHERE c.station_id=s.id), "
        "       (SELECT COUNT(*) FROM charger c WHERE c.station_id=s.id AND c.status='idle') "
        "FROM favorite f JOIN station s ON s.id=f.station_id "
        "WHERE f.user_id=? ORDER BY f.id DESC"));
    q.addBindValue(userId);
    if (!q.exec()) return out;
    while (q.next()) {
        FavoriteRow f;
        f.id            = q.value(0).toInt();
        f.stationId     = q.value(1).toInt();
        f.stationName   = q.value(2).toString();
        f.address       = q.value(3).toString();
        f.area          = q.value(4).toString();
        f.createTime    = q.value(5).toString();
        f.longitude     = q.value(6).toDouble();
        f.latitude      = q.value(7).toDouble();
        f.serviceFee    = q.value(8).toDouble();
        f.totalChargers = q.value(9).toInt();
        f.freeChargers  = q.value(10).toInt();
        out.append(f);
    }
    return out;
}

bool isFavorite(int userId, int stationId)
{
    return scalarOf(QStringLiteral(
        "SELECT COUNT(*) FROM favorite WHERE user_id=? AND station_id=?"),
        {userId, stationId}) > 0;
}

bool addFavorite(int userId, int stationId)
{
    if (scalarOf(QStringLiteral("SELECT COUNT(*) FROM station WHERE id=?"), {stationId}) < 1)
        return false;
    if (isFavorite(userId, stationId)) return true;      // 重复收藏是幂等的
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO favorite(user_id, station_id, create_time) VALUES(?,?,?)"));
    q.addBindValue(userId);
    q.addBindValue(stationId);
    q.addBindValue(nowStr());
    return q.exec();
}

bool removeFavorite(int userId, int stationId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM favorite WHERE user_id=? AND station_id=?"));
    q.addBindValue(userId);
    q.addBindValue(stationId);
    return q.exec();
}

// ==================== 通知 ====================
QList<NotificationRow> listNotifications(int userId, int limit)
{
    if (limit <= 0) limit = 50;
    QList<NotificationRow> out;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT id, type, title, content, IFNULL(related_id,0), is_read, create_time "
        "FROM notification WHERE user_id=? ORDER BY id DESC LIMIT ?"));
    q.addBindValue(userId);
    q.addBindValue(limit);
    if (!q.exec()) return out;
    while (q.next()) {
        NotificationRow n;
        n.id         = q.value(0).toInt();
        n.type       = q.value(1).toString();
        n.title      = q.value(2).toString();
        n.content    = q.value(3).toString();
        n.relatedId  = q.value(4).toInt();
        n.isRead     = q.value(5).toInt();
        n.createTime = q.value(6).toString();
        out.append(n);
    }
    return out;
}

int unreadCount(int userId)
{
    return static_cast<int>(scalarOf(QStringLiteral(
        "SELECT COUNT(*) FROM notification WHERE user_id=? AND is_read=0"), {userId}));
}

bool markNotificationRead(int userId, int notificationId)
{
    QSqlQuery q;
    if (notificationId > 0) {
        q.prepare(QStringLiteral("UPDATE notification SET is_read=1 WHERE id=? AND user_id=?"));
        q.addBindValue(notificationId);
        q.addBindValue(userId);
    } else {
        q.prepare(QStringLiteral("UPDATE notification SET is_read=1 WHERE user_id=?"));
        q.addBindValue(userId);
    }
    return q.exec();
}

int clearNotifications(int userId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM notification WHERE user_id=?"));
    q.addBindValue(userId);
    if (!q.exec()) return 0;
    return q.numRowsAffected();
}

int pushNotification(int userId, const QString &type, const QString &title,
                     const QString &content, int relatedId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO notification(user_id,type,title,content,related_id,is_read,create_time) "
        "VALUES(?,?,?,?,?,0,?)"));
    q.addBindValue(userId);
    q.addBindValue(nz(type).isEmpty() ? QStringLiteral("system") : type);
    q.addBindValue(nz(title));
    q.addBindValue(nz(content));
    q.addBindValue(relatedId > 0 ? QVariant(relatedId) : QVariant());
    q.addBindValue(nowStr());
    if (!q.exec()) return 0;
    return q.lastInsertId().toInt();
}

// ==================== 积分明细 ====================
QList<PointRecordRow> listPointRecords(int userId, int limit)
{
    if (limit <= 0) limit = 50;
    QList<PointRecordRow> out;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT id, change, reason, create_time FROM point_record "
        "WHERE user_id=? ORDER BY id DESC LIMIT ?"));
    q.addBindValue(userId);
    q.addBindValue(limit);
    if (!q.exec()) return out;
    while (q.next()) {
        PointRecordRow p;
        p.id         = q.value(0).toInt();
        p.change     = q.value(1).toInt();
        p.reason     = q.value(2).toString();
        p.createTime = q.value(3).toString();
        out.append(p);
    }
    return out;
}

// ==================== 会员套餐 ====================
QList<MemberPlanRow> listPlans()
{
    QList<MemberPlanRow> out;
    QSqlQuery q;
    if (!q.exec(QStringLiteral(
            "SELECT id, name, price, valid_days, service_fee_discount, night_discount, "
            "       points_multiplier, status, description FROM member_plan "
            "WHERE status='active' ORDER BY price")))
        return out;
    while (q.next()) {
        MemberPlanRow p;
        p.id                 = q.value(0).toInt();
        p.name               = q.value(1).toString();
        p.price              = q.value(2).toDouble();
        p.validDays          = q.value(3).toInt();
        p.serviceFeeDiscount = q.value(4).toDouble();
        p.nightDiscount      = q.value(5).toDouble();
        p.pointsMultiplier   = q.value(6).toDouble();
        p.status             = q.value(7).toString();
        p.description        = q.value(8).toString();
        out.append(p);
    }
    return out;
}

std::optional<UserPlanRow> currentPlan(int userId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT up.id, up.plan_id, mp.name, up.start_time, up.end_time, up.status "
        "FROM user_plan up JOIN member_plan mp ON mp.id=up.plan_id "
        "WHERE up.user_id=? AND up.status='active' AND up.end_time >= ? "
        "ORDER BY up.id DESC LIMIT 1"));
    q.addBindValue(userId);
    q.addBindValue(nowStr());
    if (!q.exec() || !q.next()) return std::nullopt;
    UserPlanRow u;
    u.id        = q.value(0).toInt();
    u.planId    = q.value(1).toInt();
    u.planName  = q.value(2).toString();
    u.startTime = q.value(3).toString();
    u.endTime   = q.value(4).toString();
    u.status    = q.value(5).toString();
    return u;
}

std::optional<UserPlanRow> subscribePlan(int userId, int planId, double *needMoney)
{
    QSqlQuery p;
    p.prepare(QStringLiteral("SELECT price, valid_days FROM member_plan WHERE id=? AND status='active'"));
    p.addBindValue(planId);
    if (!p.exec() || !p.next()) return std::nullopt;
    const double price = p.value(0).toDouble();
    const int    days  = p.value(1).toInt();

    const double balance = scalarOf(QStringLiteral("SELECT balance FROM user WHERE id=?"), {userId});
    if (balance < price) {
        if (needMoney) *needMoney = price;
        return std::nullopt;
    }
    const double after = std::round((balance - price) * 100) / 100.0;
    const QString start = nowStr();
    const QString end   = QDateTime::currentDateTime().addDays(days)
                              .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    QSqlDatabase::database().transaction();
    bool ok = true;

    QSqlQuery u;
    u.prepare(QStringLiteral("UPDATE user SET balance=?, level='vip' WHERE id=?"));
    u.addBindValue(after);
    u.addBindValue(userId);
    ok = ok && u.exec();

    // 旧套餐置为过期, 同一时间只保留一个 active
    QSqlQuery old;
    old.prepare(QStringLiteral("UPDATE user_plan SET status='expired' WHERE user_id=? AND status='active'"));
    old.addBindValue(userId);
    ok = ok && old.exec();

    QSqlQuery ins;
    ins.prepare(QStringLiteral(
        "INSERT INTO user_plan(user_id,plan_id,start_time,end_time,status,create_time) "
        "VALUES(?,?,?,?,'active',?)"));
    ins.addBindValue(userId);
    ins.addBindValue(planId);
    ins.addBindValue(start);
    ins.addBindValue(end);
    ins.addBindValue(nowStr());
    ok = ok && ins.exec();

    QSqlQuery w;
    w.prepare(QStringLiteral(
        "INSERT INTO wallet_transaction(user_id,type,amount,balance_after,order_id,remark,create_time) "
        "VALUES(?,'consume',?,?,NULL,'订阅会员套餐',?)"));
    w.addBindValue(userId);
    w.addBindValue(-price);
    w.addBindValue(after);
    w.addBindValue(nowStr());
    ok = ok && w.exec();

    if (!ok) { QSqlDatabase::database().rollback(); return std::nullopt; }
    QSqlDatabase::database().commit();
    return currentPlan(userId);
}

// ==================== 优惠券 ====================
namespace {

const char *kSelectMyCoupon =
    "SELECT uc.id, c.id, IFNULL(c.station_id,0), c.valid_days, c.title, c.type, "
    "       c.time_range, uc.status, uc.receive_time, c.discount_amount, c.min_amount "
    "FROM user_coupon uc JOIN coupon c ON c.id=uc.coupon_id ";

CouponRow rowToCoupon(const QSqlQuery &q)
{
    CouponRow c;
    c.id             = q.value(0).toInt();
    c.couponId       = q.value(1).toInt();
    c.stationId      = q.value(2).toInt();
    c.validDays      = q.value(3).toInt();
    c.title          = q.value(4).toString();
    c.type           = q.value(5).toString();
    c.timeRange      = q.value(6).toString();
    c.status         = q.value(7).toString();
    c.receiveTime    = q.value(8).toString();
    c.discountAmount = q.value(9).toDouble();
    c.minAmount      = q.value(10).toDouble();
    return c;
}

}  // namespace

QList<CouponRow> listMyCoupons(int userId, const QString &status)
{
    QList<CouponRow> out;
    QSqlQuery q;
    if (status.trimmed().isEmpty()) {
        q.prepare(QString::fromLatin1(kSelectMyCoupon)
                  + QStringLiteral("WHERE uc.user_id=? ORDER BY uc.id DESC"));
        q.addBindValue(userId);
    } else {
        q.prepare(QString::fromLatin1(kSelectMyCoupon)
                  + QStringLiteral("WHERE uc.user_id=? AND uc.status=? ORDER BY uc.id DESC"));
        q.addBindValue(userId);
        q.addBindValue(status.trimmed());
    }
    if (!q.exec()) return out;
    while (q.next()) out.append(rowToCoupon(q));
    return out;
}

QList<CouponRow> listClaimableCoupons(int userId)
{
    QList<CouponRow> out;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT 0, c.id, IFNULL(c.station_id,0), c.valid_days, c.title, c.type, "
        "       c.time_range, 'claimable', '', c.discount_amount, c.min_amount "
        "FROM coupon c WHERE c.status='active' "
        "  AND (c.total < 0 OR c.total > (SELECT COUNT(*) FROM user_coupon u WHERE u.coupon_id=c.id)) "
        "  AND c.id NOT IN (SELECT coupon_id FROM user_coupon WHERE user_id=?) "
        "ORDER BY c.discount_amount DESC"));
    q.addBindValue(userId);
    if (!q.exec()) return out;
    while (q.next()) out.append(rowToCoupon(q));
    return out;
}

std::optional<CouponRow> claimCoupon(int userId, int couponId)
{
    // 同一张券模板只能领一次
    if (scalarOf(QStringLiteral(
            "SELECT COUNT(*) FROM user_coupon WHERE user_id=? AND coupon_id=?"),
            {userId, couponId}) > 0)
        return std::nullopt;
    if (scalarOf(QStringLiteral(
            "SELECT COUNT(*) FROM coupon WHERE id=? AND status='active'"), {couponId}) < 1)
        return std::nullopt;

    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO user_coupon(user_id, coupon_id, status, receive_time) "
        "VALUES(?,?,'unused',?)"));
    q.addBindValue(userId);
    q.addBindValue(couponId);
    q.addBindValue(nowStr());
    if (!q.exec()) return std::nullopt;
    const int ucId = q.lastInsertId().toInt();

    QSqlQuery f;
    f.prepare(QString::fromLatin1(kSelectMyCoupon) + QStringLiteral("WHERE uc.id=?"));
    f.addBindValue(ucId);
    if (!f.exec() || !f.next()) return std::nullopt;
    return rowToCoupon(f);
}

// ==================== 评价 ====================
namespace {

const char *kSelectReview =
    "SELECT r.id, r.user_id, r.station_id, IFNULL(r.order_id,0), r.useful_count, "
    "       u.nickname, s.name, r.tags, r.content, r.create_time, "
    "       r.overall_score, r.speed_score, r.device_score, r.parking_score, "
    "       r.hygiene_score, r.service_score "
    "FROM review r JOIN user u ON u.id=r.user_id JOIN station s ON s.id=r.station_id ";

ReviewRow rowToReview(const QSqlQuery &q)
{
    ReviewRow r;
    r.id          = q.value(0).toInt();
    r.userId      = q.value(1).toInt();
    r.stationId   = q.value(2).toInt();
    r.orderId     = q.value(3).toInt();
    r.usefulCount = q.value(4).toInt();
    r.nickname    = q.value(5).toString();
    r.stationName = q.value(6).toString();
    r.tags        = q.value(7).toString();
    r.content     = q.value(8).toString();
    r.createTime  = q.value(9).toString();
    r.overall     = q.value(10).toDouble();
    r.speed       = q.value(11).toDouble();
    r.device      = q.value(12).toDouble();
    r.parking     = q.value(13).toDouble();
    r.hygiene     = q.value(14).toDouble();
    r.service     = q.value(15).toDouble();
    return r;
}

}  // namespace

QList<ReviewRow> listReviews(int stationId, int userId, int limit)
{
    if (limit <= 0) limit = 30;
    QList<ReviewRow> out;
    QSqlQuery q;
    if (stationId > 0) {
        q.prepare(QString::fromLatin1(kSelectReview)
                  + QStringLiteral("WHERE r.station_id=? AND r.status='normal' "
                                   "ORDER BY r.id DESC LIMIT ?"));
        q.addBindValue(stationId);
    } else {
        q.prepare(QString::fromLatin1(kSelectReview)
                  + QStringLiteral("WHERE r.user_id=? ORDER BY r.id DESC LIMIT ?"));
        q.addBindValue(userId);
    }
    q.addBindValue(limit);
    if (!q.exec()) return out;
    while (q.next()) out.append(rowToReview(q));
    return out;
}

std::optional<ReviewRow> createReview(int userId, int orderId, double overall,
                                      double speed, double device, double parking,
                                      double hygiene, double service,
                                      const QString &tags, const QString &content,
                                      int *errCode, QString *errMsg)
{
    auto fail = [&](int c, const QString &m) {
        if (errCode) *errCode = c;
        if (errMsg)  *errMsg  = m;
    };
    // 只能评价自己已完成的订单
    QSqlQuery o;
    o.prepare(QStringLiteral(
        "SELECT station_id, status FROM charging_order WHERE id=? AND user_id=?"));
    o.addBindValue(orderId);
    o.addBindValue(userId);
    if (!o.exec() || !o.next()) {
        fail(4001, QStringLiteral("订单不存在: id=%1").arg(orderId));
        return std::nullopt;
    }
    const int stationId = o.value(0).toInt();
    if (o.value(1).toString() != QStringLiteral("completed")) {
        fail(2003, QStringLiteral("订单尚未完成, 不能评价"));
        return std::nullopt;
    }
    if (scalarOf(QStringLiteral("SELECT COUNT(*) FROM review WHERE order_id=?"), {orderId}) > 0) {
        fail(4002, QStringLiteral("该订单已经评价过了"));
        return std::nullopt;
    }
    const double ov = (overall > 0) ? qBound(1.0, overall, 5.0) : 5.0;
    auto sub = [&](double v) { return v > 0 ? qBound(1.0, v, 5.0) : ov; };

    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO review(user_id,station_id,order_id,overall_score,speed_score,device_score,"
        "  parking_score,hygiene_score,service_score,tags,content,useful_count,status,create_time) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,0,'normal',?)"));
    q.addBindValue(userId);
    q.addBindValue(stationId);
    q.addBindValue(orderId);
    q.addBindValue(ov);
    q.addBindValue(sub(speed));
    q.addBindValue(sub(device));
    q.addBindValue(sub(parking));
    q.addBindValue(sub(hygiene));
    q.addBindValue(sub(service));
    q.addBindValue(nz(tags));
    q.addBindValue(nz(content));
    q.addBindValue(nowStr());
    if (!q.exec()) {
        fail(4002, QStringLiteral("提交评价失败"));
        return std::nullopt;
    }
    QSqlQuery f;
    f.prepare(QString::fromLatin1(kSelectReview) + QStringLiteral("WHERE r.id=?"));
    f.addBindValue(q.lastInsertId().toInt());
    if (!f.exec() || !f.next()) return std::nullopt;
    return rowToReview(f);
}

bool markReviewUseful(int reviewId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE review SET useful_count=useful_count+1 WHERE id=?"));
    q.addBindValue(reviewId);
    return q.exec() && q.numRowsAffected() > 0;
}

// ==================== 天气 / 常见问题 ====================
std::optional<WeatherRow> weatherOf(const QString &area)
{
    QSqlQuery q;
    if (area.trimmed().isEmpty()) {
        // 不指定区域时取"全市"这条
        q.prepare(QStringLiteral(
            "SELECT area, condition, temperature, forecast, update_time FROM weather "
            "WHERE area='全市' ORDER BY update_time DESC LIMIT 1"));
    } else {
        q.prepare(QStringLiteral(
            "SELECT area, condition, temperature, forecast, update_time FROM weather "
            "WHERE area=? ORDER BY update_time DESC LIMIT 1"));
        q.addBindValue(area.trimmed());
    }
    if (!q.exec() || !q.next()) return std::nullopt;
    WeatherRow w;
    w.area        = q.value(0).toString();
    w.condition   = q.value(1).toString();
    w.temperature = q.value(2).toDouble();
    w.forecast    = q.value(3).toString();
    w.updateTime  = q.value(4).toString();
    return w;
}

QList<FaqRow> listFaq(const QString &category)
{
    QList<FaqRow> out;
    QSqlQuery q;
    if (category.trimmed().isEmpty()) {
        q.prepare(QStringLiteral(
            "SELECT id, category, question, answer, sort FROM faq WHERE enabled=1 "
            "ORDER BY sort, id"));
    } else {
        q.prepare(QStringLiteral(
            "SELECT id, category, question, answer, sort FROM faq "
            "WHERE enabled=1 AND category=? ORDER BY sort, id"));
        q.addBindValue(category.trimmed());
    }
    if (!q.exec()) return out;
    while (q.next()) {
        FaqRow f;
        f.id       = q.value(0).toInt();
        f.category = q.value(1).toString();
        f.question = q.value(2).toString();
        f.answer   = q.value(3).toString();
        f.sort     = q.value(4).toInt();
        out.append(f);
    }
    return out;
}

}  // namespace dao
