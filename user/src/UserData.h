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

signals:
    void profileChanged();

private:
    QString m_nickname;
    double m_balance = 0.0;
};