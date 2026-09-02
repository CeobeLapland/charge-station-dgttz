#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// 探索页 mock 数据（v1 未接服务端期间，本地构造一批"种子"数据满足 UI 展示）。
// 数据结构 1:1 映射 DATA_STRUCTURE.md 对应表字段。字段命名统一蛇形，与服务端后续落库一致。
// - 省-市-区 三级级联（regionsProvince()）；
// - 充电站 + 电桩 + 商户 + 分时电价 + 天气 + 评价（stations/chargers/merchants/...）。
// 注：本类是 C++ 单例，注册进 UserClient 模块 QML singleton，和 Theme 同样的生存方式。
class ExploreData : public QObject {
    Q_OBJECT
public:
    explicit ExploreData(QObject* parent = nullptr);

    // —— 省-市-区 三级级联 ——
    // 返回: [{ name:"北京市", type:"municipality", cities:[ {name:"北京市", districts:[{name:"东城区", lng,lat}, ...]} ] } , { name:"上海市", ...}, { name:"广东省", cities:[{name:"广州市", districts:[...]}, {name:"深圳市", districts:[...]}] } ]
    Q_INVOKABLE QVariantList regionsTree() const;

    // 各区县中心经纬度缓存（跳转用）
    Q_INVOKABLE QVariantMap districtCenter(const QString& province,
                                           const QString& city,
                                           const QString& district) const;

    // —— 商户字典（id→name）——
    Q_INVOKABLE QVariantList merchants() const;
    Q_INVOKABLE QString merchantNameFor(int merchantId) const;

    // —— 充电站（完整字段对齐 station + 聚合：rating/ratingCount/fastIdleCount/slowIdleCount） ——
    Q_INVOKABLE QVariantList stations() const;
    Q_INVOKABLE QVariantMap stationById(int stationId) const;

    // 某电站的电桩列表（对齐 charger 字段）
    Q_INVOKABLE QVariantList chargersForStation(int stationId) const;

    // 某电站的分时电价（对齐 price_rule）
    Q_INVOKABLE QVariantList priceRulesForStation(int stationId) const;

    // 某电站的评价（对齐 review）
    Q_INVOKABLE QVariantList reviewsForStation(int stationId) const;

    // 某区域天气（对齐 weather，缺省给北京）
    Q_INVOKABLE QVariantMap weatherForArea(const QString& area) const;

    // 筛选常量（UI 下拉项直接用）
    Q_INVOKABLE QStringList ownerTypeOptions() const; // 全部/自营/具体商户名按 id
    Q_INVOKABLE QStringList ratingThresholdOptions() const; // ≥4.5 / ≥4 / ≥3 / 不限
};
