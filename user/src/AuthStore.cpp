#include "AuthStore.h"

#include <QDateTime>

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