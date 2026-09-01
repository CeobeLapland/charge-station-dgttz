#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSettings>
#include <QQmlEngine>
#include <QQmlError>
#include <QtQml/qqml.h>

#include "UserClient.h"
#include "AuthStore.h"
#include "Theme.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("chargeStation"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("charge.local"));
    QCoreApplication::setApplicationName(QStringLiteral("charger_user_client"));

    // 用户端按手机端交互设计（竖屏），控件库选用 Material 风格更接近移动端
    QQuickStyle::setStyle(QStringLiteral("Material"));

    QQmlApplicationEngine engine;

    // 诊断：把 QML 运行期所有错误/警告统一打到 stdout，fist 排查白屏
    QObject::connect(&engine, &QQmlEngine::warnings,
                     [](const QList<QQmlError>& warnings) {
                         for (const QQmlError& e : warnings)
                             qWarning().noquote() << "[QML]" << e.toString();
                     });

    // 暴露与服务端的 WebSocket 通信层供 QML 调用
    UserClient client;
    engine.rootContext()->setContextProperty("backend", &client);

    // 本地账号存储（登录/注册，示例数据阶段本地持久化，后续可切换服务端）
    AuthStore authStore;
    engine.rootContext()->setContextProperty("authStore", &authStore);

    qInfo().noquote() << "[boot] authStore accounts="
                      << authStore.accounts().join(",")
                      << " autoLogin=" << authStore.autoLoginAccount();

    // 启动自动登录：若存在勾选「自动登录」的账号，直接进入主界面，跳过登录页
    if (!authStore.autoLoginAccount().isEmpty())
        authStore.login(authStore.autoLoginAccount());

    // 全局主题：纯 C++ 对象，注册为 UserClient 模块的 singleton "Theme"。
    // 不用 setContextProperty / QML 组件实例：Qt 6.2 的 qmlcache（AOT）在属性绑定里读
    // context property 或 QML 组件实例可能得到 null/undefined（导致白屏）。C++ 单例走
    // 编译期类型路径，AOT 安全，且无 QQmlContext 冲突。
    static Theme theme;   // 静态生命周期，存活的时长覆盖整个程序
    qmlRegisterSingletonInstance("UserClient", 1, 0, "Theme", &theme);

    const QUrl url(QStringLiteral("qrc:/UserClient/qml/Main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}