#pragma once
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QWebSocketServer>

class QWebSocket;

// ============================================================
// WsServer — WebSocket 服务主循环("接线员")
// 职责: 监听端口 → 接收各端连接 → 解析 JSON 信封 → 分发到处理函数
//       (处理函数内部调 DAO 查库) → 按协议拼响应发回。
// 协议: docs/content/spec-协议.md
//   请求  {type, seq, payload}
//   响应  {type: "<请求type>_resp", seq(回填), code, message, payload}
//   推送  {type: "push.xxx", payload}          ← 无 seq, 见 broadcast()
//
// 错误码分段(新增错误码请按此分段, 并同步 spec-协议.md):
//   0     成功
//   1xxx  用户相关    1004未登录 1002手机号格式错误 1003用户被冻结
//   2xxx  订单/充电相关
//   3xxx  电站/电桩相关
//   4xxx  数据不存在  4001数据不存在
//   9xxx  协议层      9001消息格式错误 9002未知消息类型
// ============================================================
class WsServer : public QObject
{
    Q_OBJECT
public:
    explicit WsServer(quint16 port, QObject *parent = nullptr);
    bool isListening() const;

    // 向所有在线连接推送一条消息(无 seq)。所有 push.* 都走这里。
    // 例: broadcast("push.charger_status", {{"charger_id",7},{"status","charging"}});
    void broadcast(const QString &type, const QJsonObject &payload);

private slots:
    void onNewConnection();                       // 有新客户端连上来
    void onTextMessage(const QString &message);   // 收到一条文本消息
    void onDisconnected();                        // 客户端断开

private:
    // ---- 会话状态 ----
    // 一条 WebSocket 连接的身份。登录成功时写入, 之后服务端只认这里的身份,
    // 不再相信客户端 payload 里上报的 user_id/admin_id —— 否则任何人把
    // user_id 改成别人的, 就能用别人的余额下单。
    struct Session {
        int     userId  = 0;   // user.login  成功后写入; 0 = 未登录
        int     adminId = 0;   // admin.login 成功后写入; 0 = 未登录
        QString adminAccount;  // 管理员账号, 写 device_log.operator 用
    };

    // 取这条连接已登录的身份(未登录返回 0)。需要"我是谁"的 handler 一律用它。
    int userIdOf(QWebSocket *sock) const { return m_clients.value(sock).userId; }
    int adminIdOf(QWebSocket *sock) const { return m_clients.value(sock).adminId; }
    QString adminAccountOf(QWebSocket *sock) const { return m_clients.value(sock).adminAccount; }

    // admin.* 的统一准入: 未登录时回填 1004 并返回 false。
    bool requireAdmin(QWebSocket *sock, int &code, QString &message) const;

    // 分发中心: 按 type 调用对应处理函数。
    // 返回响应的 payload; code/message 通过引用参数回填。
    QJsonObject dispatch(QWebSocket *sock, const QString &type, const QJsonObject &payload,
                         int &code, QString &message);

    // ---- 各消息处理函数(每支持一种消息, 就在这里加一个) ----
    // 统一带 sock: 需要身份的用 userIdOf(sock) 取, 不需要的忽略即可。
    QJsonObject handlePing(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleUserLogin(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleUserInfo(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleStationNearby(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleStationDetail(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);

    // ---- 管理端 admin.* ----
    QJsonObject handleAdminLogin(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleAdminRevenue(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleAdminStationStatus(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleAdminStationList(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleAdminStationDetail(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleAdminStationAdd(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleAdminChargerList(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleAdminChargerAction(QWebSocket *sock, const QJsonObject &payload, int &code,
                                         QString &message, const QString &action);
    QJsonObject handleAdminUserList(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleAdminUserToggleStatus(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleAdminDeviceLog(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleAdminFaultRisk(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);

    QWebSocketServer m_server;
    QHash<QWebSocket *, Session> m_clients;   // 在线连接 → 该连接的身份
};
