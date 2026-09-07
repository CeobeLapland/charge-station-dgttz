#pragma once

#include <QHash>
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

    // 服务端接线：station.nearby_resp 覆盖/合并站表（服务端缺的展示字段本地兜底）
    Q_INVOKABLE void applyStations(const QVariantList& stations);
    // 服务端接线：station.detail_resp 回填某电站电桩（缺的字段按 type 兜底）
    Q_INVOKABLE void applyChargers(int stationId, const QVariantList& chargers);

    // 某电站的电桩列表（对齐 charger 字段）
    Q_INVOKABLE QVariantList chargersForStation(int stationId) const;

    // 某电站的分时电价（对齐 price_rule）
    Q_INVOKABLE QVariantList priceRulesForStation(int stationId) const;

    // 某电站的评价（对齐 review，含种子 + 用户新发，每条含 id/station_name/is_mine/liked_by_me/replies）
    Q_INVOKABLE QVariantList reviewsForStation(int stationId) const;

    // 全部电站评价聚合（社区页）。
    // sortMode: 0=最新(时间) 1=最热(有用数) 2=综合评分；mineOnly=true 只含当前用户新发
    Q_INVOKABLE QVariantList allReviews(int sortMode, bool mineOnly) const;

    // 写评价（r 需含 station_id + 6 维评分 + tags[] + content + nickname），成功返回 true
    Q_INVOKABLE bool addReview(const QVariantMap& r);

    // 点赞/取消「有用」，返回最新有用数（未命中返回 -1）
    Q_INVOKABLE int toggleUseful(int reviewId);

    // 追加回复（author 为当前用户昵称）
    Q_INVOKABLE bool addReply(int reviewId, const QString& author, const QString& content);

    // 某区域天气（对齐 weather，缺省给北京）
    Q_INVOKABLE QVariantMap weatherForArea(const QString& area) const;

    // 筛选常量（UI 下拉项直接用）
    Q_INVOKABLE QStringList ownerTypeOptions() const; // 全部/自营/具体商户名按 id
    Q_INVOKABLE QStringList ratingThresholdOptions() const; // ≥4.5 / ≥4 / ≥3 / 不限

signals:
    // 评论仓库变化（发评论/点赞/回复）→ 各页面刷新
    void reviewsChanged();
    // 服务端站表/电桩回写 → 探索/首页/详情刷新
    void stationsChanged();
    void chargersChanged();

private:
    QVariantList m_stations;                 // 站表缓存（种子 → 服务端 nearby 覆盖）
    QHash<int, QVariantList> m_chargers;     // stationId → 电桩（种子 → 服务端 detail 回填）
    QHash<int, QVariantList> m_priceRules;   // stationId → 分时电价
};
