#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSettings>
#include <QQmlEngine>
#include <QQmlError>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include "UserClient.h"
#include "AuthStore.h"
#include "Theme.h"
#include "ExploreData.h"
#include "UserData.h"
#include "ChatData.h"
#include "ChargingFlow.h"

// 把 QJsonValue 里的数组转 QVariantList（桥接层用）
static QVariantList jsonArrayToList(const QJsonValue& v) {
    QVariantList out;
    const QJsonArray arr = v.toArray();
    for (const auto& e : arr)
        out.append(e.toVariant());
    return out;
}

int main(int argc, char *argv[]) {
    // 探索页地图使用 QtWebEngine 加载 MapLibre GL JS，需在创建 QGuiApplication 前初始化。
    // —— WebGL 软件渲染兜底 ——
    // 无 GPU / 虚拟机环境下 Chromium 建不了 WebGL 上下文，会导致地图"看得到但拖不动"。
    // 这里回退到 SwiftShader 软件渲染，让地图始终可渲染、可拖拽缩放、可选点。
    // 仅当用户未显式设置 QTWEBENGINE_CHROMIUM_FLAGS 时才生效，保留硬件加速与自定义覆盖的能力。
    if (qEnvironmentVariableIsEmpty("QTWEBENGINE_CHROMIUM_FLAGS"))
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
                "--use-gl=angle --use-angle=swiftshader --enable-unsafe-swiftshader ");

    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QtWebEngineQuick::initialize();
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

    // 探索页 mock 数据：充电站/商户/省市区/评价等。
    static ExploreData exploreData;
    qmlRegisterSingletonInstance("UserClient", 1, 0, "ExploreData", &exploreData);

    // 用户个人域 mock 数据：我的/订单/消息/结算等。
    static UserData userData;
    qmlRegisterSingletonInstance("UserClient", 1, 0, "UserData", &userData);

    // 消息中心会话 mock 数据：会话卡片 + 聊天消息（支持发送/删除/已读）。
    static ChatData chatData;
    qmlRegisterSingletonInstance("UserClient", 1, 0, "ChatData", &chatData);

    // 充电全流程状态机（mock 服务端）。注入 ExploreData(种子 DB) 与 UserData(用户域)，
    // 客户端负责表单/扫码/导航，服务端职责(排队/匹配/计价/违约)由本类 mock，接后端时替换为 WebSocket。
    static ChargingFlow chargingFlow;
    chargingFlow.setDataSources(&exploreData, &userData);
    qmlRegisterSingletonInstance("UserClient", 1, 0, "ChargingFlow", &chargingFlow);

    // ==================== 数据桥接（BackendBridge，main.cpp 内联实现）====================
    // 原则：QML 绑定不动。服务端响应经桥接到 UserData / ChargingFlow 的缓存 + changed 信号，
    // 页面监听刷新；服务端缺的字段/城市不匹配（电站/天气在沈阳，客户端在北京）保留本地种子兜底。
    // 启动即连服务端；登录成功后批量拉个人域数据。
    client.connectServer(QString());

    // 注入发送钩子：本地变更 → 同步发协议消息（未连接时 UserClient::send 静默丢弃）
    userData.setBackendSender([&client](const QString& type, const QVariantMap& payload) {
        if (client.isConnected())
            client.sendMap(type, payload);
    });
    chargingFlow.setBackendSender([&client](const QString& type, const QVariantMap& payload) {
        if (client.isConnected())
            client.sendMap(type, payload);
    });

    // 登录成功后批量拉取个人域数据（每类响应对应一条 apply* 回写）
    const auto pullUserData = [&client]() {
        client.send(QStringLiteral("user.info"), QJsonObject());
        client.send(QStringLiteral("order.list"), QJsonObject());
        client.send(QStringLiteral("vehicle.list"), QJsonObject());
        client.send(QStringLiteral("coupon.list"), QJsonObject());
        client.send(QStringLiteral("point.list"), QJsonObject());
        client.send(QStringLiteral("plan.list"), QJsonObject());
        client.send(QStringLiteral("plan.my"), QJsonObject());
        client.send(QStringLiteral("notification.list"), QJsonObject());
        client.send(QStringLiteral("favorite.list"), QJsonObject());
        client.send(QStringLiteral("review.list"), QJsonObject());  // 不带 station_id = 我的评价
    };

    // 连接状态 → 充电流程在线标志（在线走服务端，离线回退本地 mock）
    QObject::connect(&client, &UserClient::connected, &app, [&]() {
        chargingFlow.setBackendOnline(true);
        // 电站数据接线：拉取附近电站（探索/首页共用；坐标=客户端模拟定位北京，
        // 服务端按距离返回其库内电站，覆盖本地种子 → 预约 station_id 才与服务端一致）
        client.send(QStringLiteral("station.nearby"),
                    QJsonObject{{"longitude", 116.40}, {"latitude", 39.90}});
        // 自动登录：本地存了勾选「自动登录」的手机号 → 服务端免密登录
        const QString acc = authStore.autoLoginAccount();
        if (!acc.isEmpty())
            client.login(acc);
    });
    QObject::connect(&client, &UserClient::disconnected, &app, [&]() {
        chargingFlow.setBackendOnline(false);
    });

    // 服务端响应路由：type_resp → 各数据源 apply*；push.* → ChargingFlow
    QObject::connect(&client, &UserClient::messageReceived, &app,
                     [&](const QString& type, int code, const QString& message,
                         const QJsonObject& payload) {
        // 充电流程链路：无论成败都先喂给 ChargingFlow（错误走 abnormal 提示）
        if (type == QStringLiteral("reservation.join_resp")
            || type == QStringLiteral("reservation.cancel_resp")
            || type == QStringLiteral("push.reservation_notify")
            || type == QStringLiteral("order.create_resp")
            || type == QStringLiteral("order.start_resp")
            || type == QStringLiteral("order.finish_resp")
            || type == QStringLiteral("order.settle_resp")
            || type == QStringLiteral("order.cancel_resp")
            || type == QStringLiteral("push.order_progress")) {
            chargingFlow.onBackendMessage(type, code, message, payload.toVariantMap());
        }
        if (code != 0)
            return;  // 个人域数据只回写成功响应

        const auto obj = payload.toVariantMap();

        if (type == QStringLiteral("user.login_resp")) {
            const QVariantMap user = obj.value(QStringLiteral("user")).toMap();
            if (!user.isEmpty()) {
                userData.applyUser(user);
                // 服务端身份绑定在连接上；本地记当前账号以驱动 Main.qml 切主界面
                const QString phone = user.value(QStringLiteral("phone")).toString();
                if (!phone.isEmpty() && phone != authStore.currentAccount())
                    authStore.login(phone);
                pullUserData();
            }
        } else if (type == QStringLiteral("user.info_resp")) {
            userData.applyUser(obj.value(QStringLiteral("user")).toMap());
            userData.applyPortrait(obj.value(QStringLiteral("portrait")).toMap());
        } else if (type == QStringLiteral("user.update_profile_resp")) {
            userData.applyUser(obj.value(QStringLiteral("user")).toMap());
        } else if (type == QStringLiteral("user.recharge_resp")) {
            userData.applyUser(obj.value(QStringLiteral("user")).toMap());
            userData.applyBalance(obj.value(QStringLiteral("balance")).toDouble());
        } else if (type == QStringLiteral("order.list_resp")) {
            userData.applyOrders(jsonArrayToList(payload.value(QStringLiteral("orders"))));
        } else if (type == QStringLiteral("order.detail_resp")) {
            userData.applyOrderDetail(obj.value(QStringLiteral("order")).toMap(),
                                      jsonArrayToList(payload.value(QStringLiteral("timeline"))));
        } else if (type == QStringLiteral("order.settle_resp")) {
            userData.ingestOrder(obj.value(QStringLiteral("order")).toMap());
            userData.applyBalance(obj.value(QStringLiteral("balance")).toDouble());
            userData.applyPoints(obj.value(QStringLiteral("total_points")).toDouble());
        } else if (type == QStringLiteral("order.create_resp")
                   || type == QStringLiteral("order.start_resp")
                   || type == QStringLiteral("order.finish_resp")
                   || type == QStringLiteral("order.cancel_resp")) {
            userData.ingestOrder(obj.value(QStringLiteral("order")).toMap());
        } else if (type == QStringLiteral("vehicle.list_resp")) {
            userData.applyVehicles(jsonArrayToList(payload.value(QStringLiteral("vehicles"))));
        } else if (type == QStringLiteral("vehicle.add_resp")
                   || type == QStringLiteral("vehicle.update_resp")
                   || type == QStringLiteral("vehicle.delete_resp")) {
            client.send(QStringLiteral("vehicle.list"), QJsonObject());  // 变更后重拉
        } else if (type == QStringLiteral("coupon.list_resp")) {
            userData.applyCoupons(jsonArrayToList(payload.value(QStringLiteral("coupons"))));
        } else if (type == QStringLiteral("coupon.claim_resp")) {
            client.send(QStringLiteral("coupon.list"), QJsonObject());
        } else if (type == QStringLiteral("point.list_resp")) {
            userData.applyPointRecords(jsonArrayToList(payload.value(QStringLiteral("records"))),
                                       payload.value(QStringLiteral("total_points")).toDouble());
        } else if (type == QStringLiteral("notification.list_resp")) {
            userData.applyNotifications(jsonArrayToList(payload.value(QStringLiteral("notifications"))));
        } else if (type == QStringLiteral("notification.read_resp")
                   || type == QStringLiteral("notification.clear_resp")) {
            client.send(QStringLiteral("notification.list"), QJsonObject());
        } else if (type == QStringLiteral("plan.list_resp")) {
            userData.applyMemberPlans(jsonArrayToList(payload.value(QStringLiteral("plans"))));
        } else if (type == QStringLiteral("plan.my_resp")) {
            userData.applyCurrentPlan(obj.value(QStringLiteral("plan")).toMap());
        } else if (type == QStringLiteral("plan.subscribe_resp")) {
            userData.applyCurrentPlan(obj.value(QStringLiteral("plan")).toMap());
            userData.applyBalance(obj.value(QStringLiteral("balance")).toDouble());
        } else if (type == QStringLiteral("favorite.list_resp")) {
            userData.applyFavorites(jsonArrayToList(payload.value(QStringLiteral("favorites"))));
        } else if (type == QStringLiteral("favorite.add_resp")
                   || type == QStringLiteral("favorite.remove_resp")) {
            client.send(QStringLiteral("favorite.list"), QJsonObject());
        } else if (type == QStringLiteral("review.list_resp")) {
            userData.applyMyReviews(jsonArrayToList(payload.value(QStringLiteral("reviews"))));
        } else if (type == QStringLiteral("station.nearby_resp")) {
            // 附近电站 → 覆盖探索/首页站表（ExploreData 内补全展示兜底字段）
            exploreData.applyStations(jsonArrayToList(payload.value(QStringLiteral("stations"))));
        } else if (type == QStringLiteral("station.detail_resp")) {
            // 电站详情 → 回填该站电桩（真实桩 id/状态，预约匹配/详情展示用）
            const QVariantMap st = obj.value(QStringLiteral("station")).toMap();
            exploreData.applyChargers(st.value(QStringLiteral("id")).toInt(),
                                      jsonArrayToList(payload.value(QStringLiteral("chargers"))));
        }
    });

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