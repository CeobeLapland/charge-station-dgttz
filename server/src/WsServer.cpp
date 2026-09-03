#include "WsServer.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QWebSocket>
#include <QtMath>
#include <cmath>

#include "AdminDao.h"
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

// ---- 管理端: 结构体 → JSON(字段名与 admin 的 MockDataProvider 一比一对齐) ----

QJsonObject stationFullToJson(const StationFull &s)
{
    // facilities 在库里是 JSON 文本, 这里还原成真正的数组再发出去
    QJsonArray facilities =
        QJsonDocument::fromJson(s.facilitiesJson.toUtf8()).array();
    return QJsonObject{
        {"id", s.id}, {"name", s.name}, {"address", s.address}, {"area", s.area},
        {"longitude", s.longitude}, {"latitude", s.latitude},
        {"total_chargers", s.totalChargers}, {"free_chargers", s.freeChargers},
        {"online_rate", std::round(s.onlineRate * 10) / 10.0},
        {"service_fee", s.serviceFee}, {"parking_fee", s.parkingFee},
        {"business_hours", s.businessHours}, {"facilities", facilities},
        {"owner_type", s.ownerType}, {"has_swap", s.hasSwap},
    };
}

QJsonObject chargerFullToJson(const ChargerFull &c)
{
    return QJsonObject{
        {"id", c.id}, {"code", c.code}, {"station_id", c.stationId},
        {"station_name", c.stationName}, {"type", c.type}, {"power", c.power},
        {"status", c.status}, {"voltage", c.voltage}, {"current", c.current},
        {"temperature", c.temperature}, {"fault_code", c.faultCode},
        {"comm_status", c.commStatus}, {"health_score", c.healthScore},
        {"total_charge_count", c.totalChargeCount},
        {"total_charge_duration", c.totalChargeDuration},
        {"created_time", c.createdTime},
    };
}

QJsonObject userRowToJson(const UserRow &u)
{
    return QJsonObject{
        {"id", u.id}, {"phone", u.phone}, {"nickname", u.nickname},
        {"avatar_path", u.avatarPath}, {"balance", u.balance}, {"points", u.points},
        {"level", u.level}, {"status", u.status},
        {"register_time", u.registerTime}, {"last_login_time", u.lastLoginTime},
    };
}

QJsonObject deviceLogToJson(const DeviceLogRow &l)
{
    return QJsonObject{
        {"id", l.id}, {"charger_id", l.chargerId}, {"action", l.action},
        {"operator", l.op}, {"op_time", l.opTime}, {"result", l.result},
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

    // ---- 管理端 ----
    if (type == QStringLiteral("admin.login"))          return handleAdminLogin(sock, payload, code, message);
    if (type == QStringLiteral("admin.revenue"))        return handleAdminRevenue(sock, payload, code, message);
    if (type == QStringLiteral("admin.station_status")) return handleAdminStationStatus(sock, payload, code, message);
    if (type == QStringLiteral("admin.station_list"))   return handleAdminStationList(sock, payload, code, message);
    if (type == QStringLiteral("admin.station_detail")) return handleAdminStationDetail(sock, payload, code, message);
    if (type == QStringLiteral("admin.station_add"))    return handleAdminStationAdd(sock, payload, code, message);
    if (type == QStringLiteral("admin.charger_list"))   return handleAdminChargerList(sock, payload, code, message);
    if (type == QStringLiteral("admin.charger_restart"))
        return handleAdminChargerAction(sock, payload, code, message, QStringLiteral("restart"));
    if (type == QStringLiteral("admin.charger_pause"))
        return handleAdminChargerAction(sock, payload, code, message, QStringLiteral("pause"));
    if (type == QStringLiteral("admin.user_list"))      return handleAdminUserList(sock, payload, code, message);
    if (type == QStringLiteral("admin.user_toggle_status")) return handleAdminUserToggleStatus(sock, payload, code, message);
    if (type == QStringLiteral("admin.device_log"))     return handleAdminDeviceLog(sock, payload, code, message);
    if (type == QStringLiteral("admin.fault_risk"))     return handleAdminFaultRisk(sock, payload, code, message);

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

// ==================================================================
//                          管理端 admin.*
// ==================================================================

// 统一准入: 除 admin.login 外, 所有 admin.* 都必须先登录
bool WsServer::requireAdmin(QWebSocket *sock, int &code, QString &message) const
{
    if (adminIdOf(sock) != 0)
        return true;
    code = 1004;
    message = QStringLiteral("管理员未登录: 请先发送 admin.login");
    return false;
}

// ---------- admin.login ----------
QJsonObject WsServer::handleAdminLogin(QWebSocket *sock, const QJsonObject &payload,
                                       int &code, QString &message)
{
    const QString account  = payload.value(QStringLiteral("account")).toString();
    const QString password = payload.value(QStringLiteral("password")).toString();
    const auto admin = dao::adminLogin(account, password);
    if (!admin) {
        code = 1001;                                   // spec: 账号或密码错误
        message = QStringLiteral("账号或密码错误");
        return {};
    }
    // 会话绑定: 之后这条连接就是管理员身份
    m_clients[sock].adminId      = admin->id;
    m_clients[sock].adminAccount = admin->account;
    qInfo().noquote() << QStringLiteral("[登录] 连接 %1 绑定 admin_id=%2 (%3)")
                             .arg(sock->peerAddress().toString()).arg(admin->id).arg(admin->account);
    message = QStringLiteral("登录成功");
    return QJsonObject{{"admin", QJsonObject{{"id", admin->id}, {"account", admin->account}}}};
}

// ---------- admin.revenue: 销售业绩 ----------
QJsonObject WsServer::handleAdminRevenue(QWebSocket *sock, const QJsonObject &payload,
                                         int &code, QString &message)
{
    if (!requireAdmin(sock, code, message)) return {};
    const int days = payload.value(QStringLiteral("days")).toInt(7);
    const auto r = dao::revenue(days);
    QJsonArray trend;
    for (const auto &p : r.trend)
        trend.append(QJsonObject{{"date", p.date}, {"amount", p.amount}});
    return QJsonObject{{"trend", trend}, {"today", r.today},
                       {"month", r.month}, {"total", r.total}};
}

// ---------- admin.station_status: 电桩状态分布 ----------
QJsonObject WsServer::handleAdminStationStatus(QWebSocket *sock, const QJsonObject &,
                                               int &code, QString &message)
{
    if (!requireAdmin(sock, code, message)) return {};
    const auto d = dao::chargerStatusDistribution();
    return QJsonObject{
        {"distribution", QJsonObject{
            {"charging", d.charging}, {"idle", d.idle}, {"fault", d.fault},
            {"offline", d.offline}, {"reserved", d.reserved}, {"rebooting", d.rebooting},
        }},
        {"total", d.total},
    };
}

// ---------- admin.station_list ----------
QJsonObject WsServer::handleAdminStationList(QWebSocket *sock, const QJsonObject &,
                                             int &code, QString &message)
{
    if (!requireAdmin(sock, code, message)) return {};
    QJsonArray arr;
    for (const auto &s : dao::listStationsFull())
        arr.append(stationFullToJson(s));
    return QJsonObject{{"stations", arr}};
}

// ---------- admin.station_detail: 电站 + 站内电桩(全字段) ----------
QJsonObject WsServer::handleAdminStationDetail(QWebSocket *sock, const QJsonObject &payload,
                                               int &code, QString &message)
{
    if (!requireAdmin(sock, code, message)) return {};
    const int stationId = payload.value(QStringLiteral("station_id")).toInt();
    const auto station = dao::findStationFullById(stationId);
    if (!station) {
        code = 4001;
        message = QStringLiteral("电站不存在: id=%1").arg(stationId);
        return {};
    }
    QJsonArray chargers;
    for (const auto &c : dao::listChargers(stationId))
        chargers.append(chargerFullToJson(c));
    return QJsonObject{{"station", stationFullToJson(*station)}, {"chargers", chargers}};
}

// ---------- admin.station_add ----------
QJsonObject WsServer::handleAdminStationAdd(QWebSocket *sock, const QJsonObject &payload,
                                            int &code, QString &message)
{
    if (!requireAdmin(sock, code, message)) return {};
    StationFull s;
    s.name          = payload.value(QStringLiteral("name")).toString();
    s.address       = payload.value(QStringLiteral("address")).toString();
    s.area          = payload.value(QStringLiteral("area")).toString();
    s.longitude     = payload.value(QStringLiteral("longitude")).toDouble();
    s.latitude      = payload.value(QStringLiteral("latitude")).toDouble();
    s.totalChargers = payload.value(QStringLiteral("total_chargers")).toInt();
    s.serviceFee    = payload.value(QStringLiteral("service_fee")).toDouble();
    s.parkingFee    = payload.value(QStringLiteral("parking_fee")).toDouble();
    s.businessHours = payload.value(QStringLiteral("business_hours")).toString();
    s.ownerType     = payload.value(QStringLiteral("owner_type")).toString();
    s.hasSwap       = payload.value(QStringLiteral("has_swap")).toInt();
    // facilities 前端传数组, 库里存 JSON 文本
    s.facilitiesJson = QString::fromUtf8(QJsonDocument(
        payload.value(QStringLiteral("facilities")).toArray()).toJson(QJsonDocument::Compact));

    if (s.name.trimmed().isEmpty()) {
        code = 9001;
        message = QStringLiteral("缺少 name 字段");
        return {};
    }
    const auto created = dao::addStation(s);
    if (!created) {
        code = 4002;
        message = QStringLiteral("新增电站失败");
        return {};
    }
    message = QStringLiteral("新增成功");
    return QJsonObject{{"station", stationFullToJson(*created)}};
}

// ---------- admin.charger_list: station_id 可空(空=全部) ----------
QJsonObject WsServer::handleAdminChargerList(QWebSocket *sock, const QJsonObject &payload,
                                             int &code, QString &message)
{
    if (!requireAdmin(sock, code, message)) return {};
    const int stationId = payload.value(QStringLiteral("station_id")).toInt(0);
    QJsonArray arr;
    for (const auto &c : dao::listChargers(stationId))
        arr.append(chargerFullToJson(c));
    return QJsonObject{{"chargers", arr}};
}

// ---------- admin.charger_restart / admin.charger_pause ----------
QJsonObject WsServer::handleAdminChargerAction(QWebSocket *sock, const QJsonObject &payload,
                                               int &code, QString &message, const QString &action)
{
    if (!requireAdmin(sock, code, message)) return {};
    const int chargerId = payload.value(QStringLiteral("charger_id")).toInt();
    const auto log = dao::chargerAction(chargerId, action, adminAccountOf(sock));
    if (!log) {
        code = 4001;
        message = QStringLiteral("电桩不存在或操作失败: id=%1").arg(chargerId);
        return {};
    }
    const QString newStatus = (action == QStringLiteral("restart"))
                                  ? QStringLiteral("rebooting") : QStringLiteral("offline");

    // 运维动作影响所有端看到的状态, 所以主动推送(spec: push.charger_status / push.device_log)
    broadcast(QStringLiteral("push.charger_status"),
              QJsonObject{{"charger_id", chargerId}, {"status", newStatus}});
    broadcast(QStringLiteral("push.device_log"),
              QJsonObject{{"device_log", deviceLogToJson(*log)}});

    message = (action == QStringLiteral("restart")) ? QStringLiteral("重启指令已下发")
                                                    : QStringLiteral("已暂停使用");
    return QJsonObject{{"charger_id", chargerId}, {"status", newStatus},
                       {"device_log", deviceLogToJson(*log)}};
}

// ---------- admin.user_list: keyword 可空 ----------
QJsonObject WsServer::handleAdminUserList(QWebSocket *sock, const QJsonObject &payload,
                                          int &code, QString &message)
{
    if (!requireAdmin(sock, code, message)) return {};
    const QString keyword = payload.value(QStringLiteral("keyword")).toString();
    QJsonArray arr;
    for (const auto &u : dao::listUsers(keyword))
        arr.append(userRowToJson(u));
    return QJsonObject{{"users", arr}};
}

// ---------- admin.user_toggle_status: 冻结/解冻 ----------
QJsonObject WsServer::handleAdminUserToggleStatus(QWebSocket *sock, const QJsonObject &payload,
                                                  int &code, QString &message)
{
    if (!requireAdmin(sock, code, message)) return {};
    const int     userId = payload.value(QStringLiteral("user_id")).toInt();
    const QString status = payload.value(QStringLiteral("status")).toString();
    if (status != QStringLiteral("normal") && status != QStringLiteral("frozen")) {
        code = 9001;
        message = QStringLiteral("status 只能是 normal 或 frozen, 收到: %1").arg(status);
        return {};
    }
    const auto user = dao::setUserStatus(userId, status);
    if (!user) {
        code = 4001;
        message = QStringLiteral("用户不存在: id=%1").arg(userId);
        return {};
    }
    message = (status == QStringLiteral("frozen")) ? QStringLiteral("已冻结") : QStringLiteral("已解冻");
    return QJsonObject{{"user", userRowToJson(*user)}};
}

// ---------- admin.device_log: 某桩运维日志 ----------
QJsonObject WsServer::handleAdminDeviceLog(QWebSocket *sock, const QJsonObject &payload,
                                           int &code, QString &message)
{
    if (!requireAdmin(sock, code, message)) return {};
    const int chargerId = payload.value(QStringLiteral("charger_id")).toInt();
    QJsonArray arr;
    for (const auto &l : dao::listDeviceLogs(chargerId))
        arr.append(deviceLogToJson(l));
    return QJsonObject{{"logs", arr}};
}

// ---------- admin.fault_risk: 故障风险排序 ----------
QJsonObject WsServer::handleAdminFaultRisk(QWebSocket *sock, const QJsonObject &payload,
                                           int &code, QString &message)
{
    if (!requireAdmin(sock, code, message)) return {};
    const int limit = payload.value(QStringLiteral("limit")).toInt(5);
    QJsonArray arr;
    for (const auto &r : dao::faultRisks(limit))
        arr.append(QJsonObject{{"charger_id", r.chargerId}, {"health_score", r.healthScore},
                               {"risk_level", r.riskLevel}, {"suggestion", r.suggestion}});
    return QJsonObject{{"risks", arr}};
}
