#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// 用户个人域示例数据（未接服务端期间，供「我的 / 订单 / 消息 / 结算」等页面展示）。
// 字段 1:1 对齐 DATA_STRUCTURE.md 的 user / vehicle / charging_order / order_timeline /
// coupon / user_coupon / point_record / wallet_transaction / notification，键统一蛇形。
// 与 ExploreData 同寿命：注册为 UserClient 模块的 QML singleton。
class UserData : public QObject {
    Q_OBJECT
public:
    explicit UserData(QObject* parent = nullptr);

    // —— 当前用户（user 表）——
    Q_INVOKABLE QVariantMap profile() const;

    // —— 我的车辆（vehicle 表）——
    Q_INVOKABLE QVariantList vehicles() const;

    // —— 订单（charging_order，含 station_name/charger_code 等聚合展示字段）——
    Q_INVOKABLE QVariantList orders() const;
    Q_INVOKABLE QVariantMap orderById(int orderId) const;

    // —— 订单时间轴（order_timeline）——
    Q_INVOKABLE QVariantList orderTimeline(int orderId) const;

    // —— 优惠券（user_coupon 合并 coupon 模板）——
    Q_INVOKABLE QVariantList coupons() const;

    // —— 积分记录（point_record）——
    Q_INVOKABLE QVariantList pointRecords() const;

    // —— 钱包流水（wallet_transaction）——
    Q_INVOKABLE QVariantList walletTransactions() const;

    // —— 站内消息（notification）——
    Q_INVOKABLE QVariantList notifications() const;

    // —— 充电画像（由 orders 聚合，不入库）——
    Q_INVOKABLE QVariantMap portrait() const;

    // —— 变更（示例阶段内存态，触发 profileChanged 刷新 UI）——
    Q_INVOKABLE bool recharge(double amount);
    Q_INVOKABLE bool updateNickname(const QString& nickname);
    Q_INVOKABLE bool updateAvatar(const QString& avatarPath);

    // —— 聚合：累计总用电量（kWh，来自已完成订单 energy_kwh 之和）——
    Q_INVOKABLE double totalEnergyKwh() const;

    // —— 会员套餐（member_plan 表）——
    Q_INVOKABLE QVariantList memberPlans() const;

    // —— 用户当前订阅（user_plan）——
    Q_INVOKABLE QVariantMap currentPlan() const;

    // —— 订阅会员套餐（示例：成功返回 true）——
    Q_INVOKABLE bool subscribePlan(int planId);

    // —— 我的收藏（favorite：user 收藏 station）——
    Q_INVOKABLE QVariantList favorites() const;          // [{station_id, create_time}, ...]
    Q_INVOKABLE bool isFavorite(int stationId) const;
    Q_INVOKABLE bool toggleFavorite(int stationId);      // 收藏/取消收藏，返回切换后是否收藏

    // —— 我的评论（review：用户发过的评价，社区/评价功能占位）——
    Q_INVOKABLE QVariantList myReviews() const;

    // —— 我的车辆变更（vehicle，示例内存态）——
    Q_INVOKABLE int addVehicle(const QVariantMap& v);     // 新增，成功返回新 id，失败 -1
    Q_INVOKABLE bool updateVehicle(int vehicleId, const QVariantMap& v);
    Q_INVOKABLE bool removeVehicle(int vehicleId);

signals:
    void profileChanged();
    void favoritesChanged();
    void vehiclesChanged();

private:
    QString m_nickname;
    QString m_avatarPath;
    double m_balance = 0.0;
    int m_currentPlanId = 2;   // 当前订阅套餐 id（2=月卡）
    // 内部可变态（示例阶段内存）：车辆列表与收藏列表
    QList<QVariantMap> m_vehicles;
    QList<QVariantMap> m_favorites;   // {station_id, create_time}
};