#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "UserClient.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("charge_user_client"));

    // 用户端按手机端交互设计（竖屏），控件库选用 Material 风格更接近移动端
    QQuickStyle::setStyle(QStringLiteral("Material"));

    QQmlApplicationEngine engine;

    // 暴露与服务端的 WebSocket 通信层供 QML 调用
    UserClient client;
    engine.rootContext()->setContextProperty("backend", &client);

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