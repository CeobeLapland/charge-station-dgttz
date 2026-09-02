#include "AuthStore.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

AuthStore::AuthStore(QObject* parent)
    : QObject(parent), m_settings() {
    reloadAccounts();
    m_current = m_settings.value(QStringLiteral("session/current")).toString();
}

QString AuthStore::groupKey(const QString& account) const {
    return QStringLiteral("accounts/") + account;
}

void AuthStore::reloadAccounts() {
    m_accounts = m_settings.value(QStringLiteral("accounts/list")).toStringList();
    m_accounts.removeAll({});
}

bool AuthStore::hasAccount(const QString& account) const {
    return m_accounts.contains(account);
}

QString AuthStore::passwordFor(const QString& account) const {
    return m_settings.value(groupKey(account) + QStringLiteral("/password")).toString();
}

bool AuthStore::rememberPasswordFor(const QString& account) const {
    return m_settings.value(groupKey(account) + QStringLiteral("/remember")).toBool();
}

bool AuthStore::autoLoginFor(const QString& account) const {
    return m_settings.value(groupKey(account) + QStringLiteral("/auto_login")).toBool();
}

bool AuthStore::verifyLogin(const QString& account, const QString& password) const {
    if (!hasAccount(account))
        return false;
    return passwordFor(account) == password;
}

void AuthStore::registerAccount(const QString& account, const QString& password,
                                bool remember, bool autoLogin) {
    if (!m_accounts.contains(account))
        m_accounts.append(account);
    m_settings.setValue(QStringLiteral("accounts/list"), m_accounts);
    updateOptions(account, remember, autoLogin);
    m_settings.setValue(groupKey(account) + QStringLiteral("/password"), password);
    m_settings.sync();
}

void AuthStore::updateOptions(const QString& account, bool remember, bool autoLogin) {
    m_settings.setValue(groupKey(account) + QStringLiteral("/remember"), remember);
    m_settings.setValue(groupKey(account) + QStringLiteral("/auto_login"), autoLogin);
    m_settings.sync();
}

void AuthStore::login(const QString& account) {
    if (m_current == account)
        return;
    m_current = account;
    m_settings.setValue(QStringLiteral("session/current"), account);
    m_settings.setValue(QStringLiteral("session/current_login_time"),
                        QDateTime::currentDateTime().toString(Qt::ISODate));
    m_settings.sync();
    emit currentAccountChanged();
}

void AuthStore::logout() {
    if (m_current.isEmpty())
        return;
    m_current.clear();
    m_settings.remove(QStringLiteral("session/current"));
    m_settings.sync();
    emit currentAccountChanged();
}

QString AuthStore::autoLoginAccount() const {
    // 优先当前会话账号，其次任意勾选自动登录的账号
    if (!m_current.isEmpty() && autoLoginFor(m_current))
        return m_current;
    for (const QString& account : m_accounts) {
        if (autoLoginFor(account))
            return account;
    }
    return QString();
}

QString AuthStore::accountStatus(const QString& account) const {
    return m_settings.value(groupKey(account) + QStringLiteral("/status"),
                            QStringLiteral("normal")).toString();
}

bool AuthStore::isFrozen(const QString& account) const {
    return accountStatus(account) == QStringLiteral("frozen");
}

void AuthStore::setAccountStatus(const QString& account, const QString& status) {
    if (!hasAccount(account))
        return;
    m_settings.setValue(groupKey(account) + QStringLiteral("/status"), status);
    m_settings.sync();
}

void AuthStore::resetPassword(const QString& account, const QString& newPassword) {
    if (!hasAccount(account) || newPassword.isEmpty())
        return;
    m_settings.setValue(groupKey(account) + QStringLiteral("/password"), newPassword);
    m_settings.sync();
}

void AuthStore::submitAppeal(const QString& account, const QString& description,
                             const QStringList& images) {
    QJsonObject rec;
    rec.insert(QStringLiteral("account"), account);
    rec.insert(QStringLiteral("description"), description);
    rec.insert(QStringLiteral("images"), QJsonArray::fromStringList(images));
    rec.insert(QStringLiteral("status"), QStringLiteral("pending"));
    rec.insert(QStringLiteral("create_time"),
               QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));

    QJsonArray list = QJsonDocument::fromJson(
                          m_settings.value(QStringLiteral("appeals/list")).toString().toUtf8())
                          .array();
    list.append(rec);
    m_settings.setValue(QStringLiteral("appeals/list"),
                        QString::fromUtf8(QJsonDocument(list).toJson(QJsonDocument::Compact)));
    m_settings.sync();
}

// —— 全局设置：地图瓦片源 ——

namespace {
// 预置瓦片源字典。注意：template 里 {x} {y} {z} 由 MapLibre 替换；
// 若高德风格有多域负载均衡（{1-4}），在 MapLibre 会按 tiles 数组展开。
// 为简化代码，每个 preset 的 template 必须是单 URL（多域名用若干 {a..d} 占位符）。
QStringList kPresetRows() {
    return {
        // id, name, tileUrlTemplate
        "amap_std|高德街道（国内推荐）|https://webrd0{1,2,3,4}.is.autonavi.com/appmaptile?lang=zh_cn&size=1&scale=1&style=8&x={x}&y={y}&z={z}",
        "amap_sat|高德卫星|https://webst0{1,2,3,4}.is.autonavi.com/appmaptile?style=6&x={x}&y={y}&z={z}",
        "osm_hot|OSM 人道版（街道文字丰富）|https://{a,b,c}.tile.openstreetmap.fr/hot/{z}/{x}/{y}.png",
        "osm_std|OSM 标准（海外网络可用）|https://tile.openstreetmap.org/{z}/{x}/{y}.png",
        "custom|自定义（填自定义 URL 模板）|<custom>"
    };
}
const QString kDefaultId = QStringLiteral("amap_std");
const QString kPrefGroup = QStringLiteral("prefs/map/");
}

QString AuthStore::mapTileSource() const {
    return m_settings.value(kPrefGroup + QStringLiteral("source"), kDefaultId).toString();
}

void AuthStore::setMapTileSource(const QString& id) {
    const QString old = mapTileSource();
    if (id.isEmpty() || id == old)
        return;
    m_settings.setValue(kPrefGroup + QStringLiteral("source"), id);
    m_settings.sync();
    emit mapTileSourceChanged();
}

QString AuthStore::mapTileCustomUrl() const {
    return m_settings.value(kPrefGroup + QStringLiteral("custom_url"), QString()).toString();
}

void AuthStore::setMapTileCustomUrl(const QString& url) {
    const QString old = mapTileCustomUrl();
    if (url == old)
        return;
    m_settings.setValue(kPrefGroup + QStringLiteral("custom_url"), url);
    m_settings.sync();
    emit mapTileCustomUrlChanged();
}

QString AuthStore::mapTilePresetsJson() const {
    QJsonArray arr;
    for (const QString& row : kPresetRows()) {
        const QStringList parts = row.split(QLatin1Char('|'));
        if (parts.size() < 3) continue;
        QJsonObject o;
        o.insert(QStringLiteral("id"),       parts.at(0));
        o.insert(QStringLiteral("name"),     parts.at(1));
        o.insert(QStringLiteral("template"), parts.at(2));
        arr.append(o);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}