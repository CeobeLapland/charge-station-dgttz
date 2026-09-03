#include "WsServer.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QWebSocket>
#include <QtMath>
#include <cmath>

#include "StationDao.h"
#include "UserDao.h"

namespace {

// 两个经纬度点之间的球面距离(公里) —— haversine 公式
double distanceKm(double lng1, double lat1, double lng2, double lat2)
{
    const double R = 6371.0;
    const double dLat = qDegreesToRadians(lat2 - lat1);
    const double dLng = qDegreesToRadians(lng2 - lng1);
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2)
                   + std::cos(qDegreesToRadians(lat1)) * std::cos(qDegreesToRadians(lat2))
                     * std::sin(dLng / 2) * std::sin(dLng / 2);
    return R * 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}

QJsonObject userToJson(const User &u)
{
    return QJsonObject{
        {"id", u.id}, {"phone", u.phone}, {"nickname", u.nickname},
        {"avatar_path", u.avatarPath}, {"balance", u.balance},
        {"points", u.points}, {"level", u.level}, {"status", u.status},
    };
}

}  // namespace

WsServer::WsServer(quint16 port, QObject *parent)
    : QObject(parent),
      m_server(QStringLiteral("charge-server"), QWebSocketServer::NonSecureMode)
{
    if (m_server.listen(QHostAddress::Any, port)) {
        connect(&m_server, &QWebSocketServer::newConnection,
                this, &WsServer::onNewConnection);
        qInfo().noquote() << QStringLiteral("[启动] WebSocket 服务端监听端口 %1").arg(port);
    } else {
        qCritical().noquote()
            << QStringLiteral("[失败] 端口 %1 监听失败: %2 (是否已被其他进程占用?)")
                   .arg(port).arg(m_server.errorString());
    }
}

bool WsServer::isListening() const { return m_server.isListening(); }

void WsServer::onNewConnection()
{
    QWebSocket *sock = m_server.nextPendingConnection();
    m_clients.insert(sock, Session{});          // 新连接: 身份为空, 等待登录
    connect(sock, &QWebSocket::textMessageReceived, this, &WsServer::onTextMessage);
    connect(sock, &QWebSocket::disconnected, this, &WsServer::onDisconnected);
    qInfo().noquote() << QStringLiteral("[连接] 新客户端: %1 (当前在线 %2)")
                             .arg(sock->peerAddress().toString()).arg(m_clients.size());
}

void WsServer::onDisconnected()
{
    auto *sock = qobject_cast<QWebSocket *>(sender());
    if (!sock) return;
    m_clients.remove(sock);                     // 连接没了, 它的身份也一起清掉
    sock->deleteLater();
    qInfo().noquote() << QStringLiteral("[断开] 客户端离线 (当前在线 %1)").arg(m_clients.size());
}

// ---------- 服务端主动推送: 群发给所有在线连接 ----------
void WsServer::broadcast(const QString &type, const QJsonObject &payload)
{
    // 推送不是任何请求的回应, 所以没有 seq(见 spec-协议.md)
    const QJsonObject msg{{"type", type}, {"payload", payload}};
    const QString text = QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it)
        it.key()->sendTextMessage(text);
    qInfo().noquote() << QStringLiteral("[推送] %1 → %2 个连接").arg(type).arg(m_clients.size());
}

void WsServer::onTextMessage(const QString &message)
{
    auto *sock = qobject_cast<QWebSocket *>(sender());
    if (!sock) return;

    // ---- 解析信封 ----
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject reply;
    if (!doc.isObject() || !doc.object().contains(QStringLiteral("type"))) {
        // 9001: 消息格式错误 —— 兜底而不是沉默, 让对端一眼看到问题
        reply = QJsonObject{{"type", "error"}, {"code", 9001},
                            {"message", "消息格式错误: 需要含 type 字段的 JSON 对象"},
                            {"payload", QJsonObject{}}};
        sock->sendTextMessage(QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact)));
        return;
    }
    const QJsonObject req     = doc.object();
    const QString     type    = req.value(QStringLiteral("type")).toString();
    const qint64      seq     = req.value(QStringLiteral("seq")).toInteger();
    const QJsonObject payload = req.value(QStringLiteral("payload")).toObject();

    qInfo().noquote() << QStringLiteral("[收到] %1 (seq=%2, user=%3)")
                             .arg(type).arg(seq).arg(userIdOf(sock));

    // ---- 分发处理 ----
    int code = 0;
    QString msg = QStringLiteral("ok");
    const QJsonObject respPayload = dispatch(sock, type, payload, code, msg);

    // ---- 拼响应信封并回发 ----
    reply = QJsonObject{{"type", type + QStringLiteral("_resp")}, {"seq", seq},
                        {"code", code}, {"message", msg}, {"payload", respPayload}};
    sock->sendTextMessage(QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact)));
}

QJsonObject WsServer::dispatch(QWebSocket *sock, const QString &type, const QJsonObject &payload,
                               int &code, QString &message)
{
    if (type == QStringLiteral("system.ping"))     return handlePing(sock, payload, code, message);
    if (type == QStringLiteral("user.login"))      return handleUserLogin(sock, payload, code, message);
    if (type == QStringLiteral("user.info"))       return handleUserInfo(sock, payload, code, message);
    if (type == QStringLiteral("station.nearby"))  return handleStationNearby(sock, payload, code, message);
    if (type == QStringLiteral("station.detail"))  return handleStationDetail(sock, payload, code, message);

    // 9002: 未知/未实现的消息 —— 响亮地失败, 并说清找谁
    code = 9002;
    message = QStringLiteral("未知或尚未实现的消息类型: %1 (联系数据库/服务端负责人)").arg(type);
    return {};
}

// ---------- system.ping: 心跳 ----------
QJsonObject WsServer::handlePing(QWebSocket *, const QJsonObject &, int &, QString &)
{
    return QJsonObject{{"timestamp",
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))}};
}

// ---------- user.login: 手机号免密登录 ----------
QJsonObject WsServer::handleUserLogin(QWebSocket *sock, const QJsonObject &payload,
                                      int &code, QString &message)
{
    const QString phone = payload.value(QStringLiteral("phone")).toString();
    bool created = false;
    const auto user = dao::loginOrRegister(phone, &created);
    if (!user) {
        code = 1002;
        message = QStringLiteral("手机号格式错误: 需要11位数字");
        return {};
    }
    if (user->status == QStringLiteral("frozen")) {
        code = 1003;
        message = QStringLiteral("该账号已被冻结, 请联系管理员");
        return {};
    }

    // ★ 会话绑定: 把身份记在这条连接上。之后这条连接的所有请求都用这个 id,
    //   不再读客户端 payload 里的 user_id。
    m_clients[sock].userId = user->id;
    qInfo().noquote() << QStringLiteral("[登录] 连接 %1 绑定 user_id=%2 (%3)")
                             .arg(sock->peerAddress().toString()).arg(user->id).arg(user->nickname);

    message = created ? QStringLiteral("注册并登录成功") : QStringLiteral("登录成功");
    return QJsonObject{{"user", userToJson(*user)}, {"created", created}};
}

// ---------- user.info: 获取当前登录用户信息(不带任何 id, 身份来自连接) ----------
QJsonObject WsServer::handleUserInfo(QWebSocket *sock, const QJsonObject &,
                                     int &code, QString &message)
{
    const int uid = userIdOf(sock);
    if (uid == 0) {
        code = 1004;
        message = QStringLiteral("尚未登录: 请先发送 user.login");
        return {};
    }
    const auto user = dao::findUserById(uid);
    if (!user) {
        code = 4001;
        message = QStringLiteral("用户不存在: id=%1").arg(uid);
        return {};
    }
    // portrait(充电画像) 待实现, 先返回空对象, 字段名按 spec-协议.md 预留
    return QJsonObject{{"user", userToJson(*user)}, {"portrait", QJsonObject{}}};
}

// ---------- station.nearby: 附近电站(按距离升序) ----------
QJsonObject WsServer::handleStationNearby(QWebSocket *, const QJsonObject &payload,
                                          int &code, QString &message)
{
    if (!payload.contains(QStringLiteral("longitude")) ||
        !payload.contains(QStringLiteral("latitude"))) {
        code = 9001;
        message = QStringLiteral("缺少 longitude / latitude 字段");
        return {};
    }
    const double lng = payload.value(QStringLiteral("longitude")).toDouble();
    const double lat = payload.value(QStringLiteral("latitude")).toDouble();

    auto stations = dao::listStations();
    // 算距离 → 排序(近→远)
    QList<QPair<double, StationView>> ranked;
    for (const auto &s : stations)
        ranked.append({distanceKm(lng, lat, s.longitude, s.latitude), s});
    std::sort(ranked.begin(), ranked.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });

    QJsonArray arr;
    for (const auto &[dist, s] : ranked) {
        arr.append(QJsonObject{
            {"id", s.id}, {"name", s.name}, {"address", s.address}, {"area", s.area},
            {"longitude", s.longitude}, {"latitude", s.latitude},
            {"total_chargers", s.totalChargers}, {"free_chargers", s.freeChargers},
            {"service_fee", s.serviceFee},
            {"price", dao::currentPrice(s.id)},          // 当前时段电价(元/kWh)
            {"distance", std::round(dist * 10) / 10.0},  // 距离(公里, 1位小数)
        });
    }
    return QJsonObject{{"stations", arr}};
}

// ---------- station.detail: 电站 + 站内电桩明细 ----------
QJsonObject WsServer::handleStationDetail(QWebSocket *, const QJsonObject &payload,
                                          int &code, QString &message)
{
    const int stationId = payload.value(QStringLiteral("station_id")).toInt();
    const auto station = dao::findStationById(stationId);
    if (!station) {
        code = 4001;
        message = QStringLiteral("电站不存在: id=%1").arg(stationId);
        return {};
    }
    QJsonArray chargers;
    for (const auto &c : dao::listChargersOfStation(stationId)) {
        chargers.append(QJsonObject{
            {"id", c.id}, {"code", c.code}, {"type", c.type}, {"power", c.power},
            {"status", c.status}, {"health_score", c.healthScore},
        });
    }
    return QJsonObject{
        {"station", QJsonObject{
            {"id", station->id}, {"name", station->name}, {"address", station->address},
            {"area", station->area}, {"longitude", station->longitude},
            {"latitude", station->latitude}, {"total_chargers", station->totalChargers},
            {"free_chargers", station->freeChargers}, {"service_fee", station->serviceFee},
            {"price", dao::currentPrice(station->id)},
        }},
        {"chargers", chargers},
    };
}
