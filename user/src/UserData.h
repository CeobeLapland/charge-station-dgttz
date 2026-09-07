#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

// 用户个人域数据（未接服务端期间展示种子示例；接服务端后由 BackendBridge 把
// 服务端响应经 apply* 写入缓存并发出 changed 信号，QML 绑定不变）。
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
    //Q_INVOKABLE bool subscribePlan(int planId);

    // —— 我的收藏（favorite：user 收藏 station）——
    Q_INVOKABLE QVariantList favorites() const;          // [{station_id, create_time}, ...]
    Q_INVOKABLE bool isFavorite(int stationId) const;
    Q_INVOKABLE bool toggleFavorite(int stationId);      // 收藏/取消收藏，返回切换后是否收藏

    // —— 我的评论（review：用户发过的评价，社区/评价功能占位）——
    Q_INVOKABLE QVariantList myReviews() const;

    // —— 变更（示例阶段内存态；接服务端后同步发协议消息，成功由服务端响应回写）——
    //Q_INVOKABLE bool recharge(double amount);
    //Q_INVOKABLE bool updateNickname(const QString& nickname);
    //Q_INVOKABLE bool updateAvatar(const QString& avatarPath);

    // —— 我的车辆变更（vehicle，示例内存态）——
    Q_INVOKABLE int addVehicle(const QVariantMap& v);     // 新增，成功返回新 id，失败 -1
    Q_INVOKABLE bool updateVehicle(int vehicleId, const QVariantMap& v);
    Q_INVOKABLE bool removeVehicle(int vehicleId);

    // —— 聚合：累计总用电量（kWh，来自已完成订单 energy_kwh 之和）——
    //Q_INVOKABLE double totalEnergyKwh() const;

    // —— 订阅会员套餐（示例：成功返回 true）——
    Q_INVOKABLE bool subscribePlan(int planId);

    // ==================== 服务端数据写入（BackendBridge 调用）====================
    // 全部 normalize 成 QML 已绑定的 mock 形状后写缓存，并发出对应 changed 信号。
    void applyUser(const QVariantMap& user);                     // user.info / user.recharge / update_profile
    void applyPortrait(const QVariantMap& p);                    // 画像缓存（服务端暂返空，客户端用订单聚合）
    void applyOrders(const QVariantList& orders);                // order.list
    void applyOrderDetail(const QVariantMap& order, const QVariantList& timeline); // order.detail
    Q_INVOKABLE void ingestOrder(const QVariantMap& order);      // 充电流程单笔回写（order.create/start/finish/settle）
    void applyVehicles(const QVariantList& vehicles);            // vehicle.list
    void applyCoupons(const QVariantList& coupons);              // coupon.list（含 valid_days→valid_until 换算）
    void applyPointRecords(const QVariantList& records, double totalPoints); // point.list
    void applyNotifications(const QVariantList& notifications);  // notification.list
    void applyMemberPlans(const QVariantList& plans);            // plan.list
    void applyCurrentPlan(const QVariantMap& plan);              // plan.my / plan.subscribe（可空）
    void applyFavorites(const QVariantList& favorites);          // favorite.list
    void applyMyReviews(const QVariantList& reviews);            // review.list（不带 station_id）
    void applyWallet(const QVariantList& transactions);          // 钱包流水（服务端暂无接口，预留）
    void applyBalance(double balance);                           // 余额回写（recharge/settle）
    void applyPoints(double points);                             // 积分回写（point.list/settle）

    // 后端发送钩子：main.cpp 注入 client.sendMap，本地变更时同步发协议消息
    void setBackendSender(std::function<void(const QString&, const QVariantMap&)> sender) {
        m_sendBackend = std::move(sender);
    }

signals:
    void profileChanged();
    void favoritesChanged();
    void vehiclesChanged();
    // —— 接线新增：服务端数据到达后 QML 页面据此刷新 ——
    void ordersChanged();
    void couponsChanged();
    void pointsChanged();
    void notificationsChanged();
    void memberPlansChanged();
    void currentPlanChanged();
    void myReviewsChanged();

private:
    QVariantMap seedProfile() const;

    // 本地可变态（示例阶段内存）：车辆/收藏/昵称/余额
    QString m_nickname;
    QString m_avatarPath;
    double m_balance = 0.0;
    int m_points = 1280;
    QString m_level = QStringLiteral("vip");
    int m_currentPlanId = 2;   // 当前订阅套餐 id（2=月卡）

    QList<QVariantMap> m_vehicles;
    QList<QVariantMap> m_favorites;   // {station_id, create_time}

    // —— 服务端缓存（构造时初始化为种子，保证离线与在线展示一致）——
    QVariantMap m_profile;                       // user（服务端字段）
    QVariantMap m_portrait;                      // 画像（聚合缓存）
    QList<QVariantMap> m_orders;                 // charging_order（mock 形状）
    QHash<int, QVariantList> m_timelines;        // order_id → order_timeline
    QList<QVariantMap> m_coupons;                // mock 形状
    QList<QVariantMap> m_pointRecords;
    QList<QVariantMap> m_notifications;
    QList<QVariantMap> m_memberPlans;
    QVariantMap m_currentPlan;                   // 空 = 未订阅/用本地默认
    QList<QVariantMap> m_myReviews;
    QList<QVariantMap> m_walletTransactions;

    std::function<void(const QString&, const QVariantMap&)> m_sendBackend;
};
