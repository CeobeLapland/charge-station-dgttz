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
    // —— 全局设置（全用户共享，经 QSettings "prefs/*" 持久化）——
    Q_PROPERTY(QString mapTileSource READ mapTileSource WRITE setMapTileSource NOTIFY mapTileSourceChanged)
    Q_PROPERTY(QString mapTileCustomUrl READ mapTileCustomUrl WRITE setMapTileCustomUrl NOTIFY mapTileCustomUrlChanged)

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

    // —— 账号状态（对应 user.status：normal / frozen；未接服务端前本地模拟）——
    Q_INVOKABLE QString accountStatus(const QString& account) const;
    Q_INVOKABLE bool isFrozen(const QString& account) const;
    Q_INVOKABLE void setAccountStatus(const QString& account, const QString& status);

    // 重置账号密码（找回密码成功后更新本地配置）
    Q_INVOKABLE void resetPassword(const QString& account, const QString& newPassword);
    // 提交申诉（本地模拟，对应 work_order：type=user_complaint、status=pending，后续接服务端）
    Q_INVOKABLE void submitAppeal(const QString& account, const QString& description,
                                  const QStringList& images);

    // —— 全局设置：地图瓦片源 ——
    QString mapTileSource() const;
    void setMapTileSource(const QString& id);
    QString mapTileCustomUrl() const;
    void setMapTileCustomUrl(const QString& url);
    // 预置瓦片源字典（JSON 数组字符串，QML 直接用）
    Q_INVOKABLE QString mapTilePresetsJson() const;

signals:
    void currentAccountChanged();
    void mapTileSourceChanged();
    void mapTileCustomUrlChanged();

private:
    QString groupKey(const QString& account) const;
    void reloadAccounts();

    QSettings m_settings;
    QStringList m_accounts;
    QString m_current;
};