#pragma once
#include <QList>
#include <QString>
#include <optional>

// ProfileDao — 用户个人域数据: 收藏 / 通知 / 积分明细 / 会员套餐 / 优惠券 / 评价
// 对应协议消息 favorite.* / notification.* / point.* / plan.* / coupon.* / review.*

struct FavoriteRow {
    int     id = 0, stationId = 0;
    QString stationName, address, area, createTime;
    double  longitude = 0, latitude = 0, serviceFee = 0;
    int     totalChargers = 0, freeChargers = 0;
};

struct NotificationRow {
    int     id = 0, relatedId = 0, isRead = 0;
    QString type, title, content, createTime;
};

struct PointRecordRow {
    int     id = 0, change = 0;
    QString reason, createTime;
};

struct MemberPlanRow {
    int     id = 0, validDays = 0;
    QString name, description, status;
    double  price = 0, serviceFeeDiscount = 1, nightDiscount = 1, pointsMultiplier = 1;
};

struct UserPlanRow {
    int     id = 0, planId = 0;
    QString planName, startTime, endTime, status;
};

struct CouponRow {
    int     id = 0;              // user_coupon.id, 核销用
    int     couponId = 0, stationId = 0, validDays = 0;
    QString title, type, timeRange, status, receiveTime;
    double  discountAmount = 0, minAmount = 0;
};

struct ReviewRow {
    int     id = 0, userId = 0, stationId = 0, orderId = 0, usefulCount = 0;
    QString nickname, stationName, tags, content, createTime;
    double  overall = 0, speed = 0, device = 0, parking = 0, hygiene = 0, service = 0;
};

struct WeatherRow {
    QString area, condition, forecast, updateTime;
    double  temperature = 0;
};

struct FaqRow {
    int     id = 0, sort = 0;
    QString category, question, answer;
};

namespace dao {

// 收藏
QList<FavoriteRow> listFavorites(int userId);
bool               addFavorite(int userId, int stationId);      // 已收藏则原样返回 true
bool               removeFavorite(int userId, int stationId);
bool               isFavorite(int userId, int stationId);

// 通知
QList<NotificationRow> listNotifications(int userId, int limit);
int                    unreadCount(int userId);
bool                   markNotificationRead(int userId, int notificationId);  // id<=0 表示全部已读
int                    clearNotifications(int userId);                        // 返回删除条数
// 服务端主动写一条通知
int                    pushNotification(int userId, const QString &type, const QString &title,
                                        const QString &content, int relatedId);

// 积分明细
QList<PointRecordRow> listPointRecords(int userId, int limit);

// 会员套餐
QList<MemberPlanRow>   listPlans();
std::optional<UserPlanRow> currentPlan(int userId);
// 订阅套餐: 扣余额 + 写 user_plan + 记钱包流水, 一个事务。
// 余额不足返回 nullopt 并把 needMoney 置为所需金额。
std::optional<UserPlanRow> subscribePlan(int userId, int planId, double *needMoney);

// 优惠券
QList<CouponRow> listMyCoupons(int userId, const QString &status);  // status 为空=全部
QList<CouponRow> listClaimableCoupons(int userId);                  // 还没领的模板
std::optional<CouponRow> claimCoupon(int userId, int couponId);     // 领券

// 评价
QList<ReviewRow> listReviews(int stationId, int userId, int limit);  // 二选一: 按站或按人
std::optional<ReviewRow> createReview(int userId, int orderId, double overall,
                                      double speed, double device, double parking,
                                      double hygiene, double service,
                                      const QString &tags, const QString &content,
                                      int *errCode, QString *errMsg);
bool markReviewUseful(int reviewId);

// 天气 / 常见问题
std::optional<WeatherRow> weatherOf(const QString &area);
QList<FaqRow>             listFaq(const QString &category);

}  // namespace dao
