#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

// 本地账号存储：用于登录/注册（未接入服务端前的示例数据）。
// 存储多个账号，每个账号独立配置「记住密码」与「自动登录」。
// 数据经 QSettings 持久化到本机，键小写蛇形。
class AuthStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentAccount READ currentAccount NOTIFY currentAccountChanged)
    Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY currentAccountChanged)

public:
    explicit AuthStore(QObject* parent = nullptr);

    QString currentAccount() const { return m_current; }
    bool isLoggedIn() const { return !m_current.isEmpty(); }

    // 已注册账号列表
    Q_INVOKABLE QStringList accounts() const { return m_accounts; }
    // 该账号是否已注册
    Q_INVOKABLE bool hasAccount(const QString& account) const;
    // 校验账号+密码
    Q_INVOKABLE bool verifyLogin(const QString& account, const QString& password) const;
    // 读取账号存储的密码（用于记住密码回填）
    Q_INVOKABLE QString passwordFor(const QString& account) const;
    // 读取账号的记住密码/自动登录配置
    Q_INVOKABLE bool rememberPasswordFor(const QString& account) const;
    Q_INVOKABLE bool autoLoginFor(const QString& account) const;

    // 注册新账号并保存该账号配置
    Q_INVOKABLE void registerAccount(const QString& account, const QString& password,
                                     bool remember, bool autoLogin);
    // 更新某账号的记住密码/自动登录配置（登录时同步）
    Q_INVOKABLE void updateOptions(const QString& account, bool remember, bool autoLogin);
    // 登录：记录当前账号（触发 isLoggedIn 变化）
    Q_INVOKABLE void login(const QString& account);
    // 退出/切换账号：清空当前账号，回到登录页
    Q_INVOKABLE void logout();
    // 启动时自动登录的账号（若存在勾选自动登录的账号则返回其账号，否则空串）
    Q_INVOKABLE QString autoLoginAccount() const;

signals:
    void currentAccountChanged();

private:
    QString groupKey(const QString& account) const;
    void reloadAccounts();

    QSettings m_settings;
    QStringList m_accounts;
    QString m_current;
};