#pragma once
#include <QHash>
#include <QThread>
#include <QJsonObject>
#include <QObject>
#include <QWebSocketServer>

class QWebSocket;
class ChargeSimulator;

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
//   2xxx  订单/充电   2001有未完成订单 2002余额不足 2003状态不允许该操作
//   3xxx  电站/电桩   3002电桩非空闲
//   4xxx  数据不存在  4001数据不存在
//   5xxx  模型层      5001预测/模型不可用
//   9xxx  协议层      9001消息格式错误 9002未知消息类型
// ============================================================
class WsServer : public QObject
{
    Q_OBJECT
public:
    explicit WsServer(quint16 port, QObject *parent = nullptr);
    ~WsServer() override;
    bool isListening() const;

    // 只推给某个已登录用户的连接(可能有多条)。push.order_progress 这类
    // "只跟一个人有关"的消息走这里, 不广播给所有端。
    void sendToUser(int userId, const QString &type, const QJsonObject &payload);

    // 向所有在线连接推送一条消息(无 seq)。所有 push.* 都走这里。
    // 例: broadcast("push.charger_status", {{"charger_id",7},{"status","charging"}});
    void broadcast(const QString &type, const QJsonObject &payload);

private slots:
    // ---- 来自充电仿真工作线程的信号(队列连接, 在主线程执行) ----
    void onSimProgress(int orderId, int userId, int chargerId, int stationId,
                       double soc, double powerKw, double energyKwh, double cost, int etaMin);
    void onSimMeasure(int chargerId, int stationId, double powerKw, double soc, double energyDelta);
    void onSimReachedTarget(int orderId, int userId, double endSoc);

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
    // user.*/order.* 的统一准入: 未登录时回填 1004 并返回 false。
    bool requireUser(QWebSocket *sock, int &code, QString &message) const;

    // 分发中心: 按 type 调用对应处理函数。
    // 返回响应的 payload; code/message 通过引用参数回填。
    QJsonObject dispatch(QWebSocket *sock, const QString &type, const QJsonObject &payload,
                         int &code, QString &message);

    // ---- 各消息处理函数(每支持一种消息, 就在这里加一个) ----
    // 统一带 sock: 需要身份的用 userIdOf(sock) 取, 不需要的忽略即可。
    QJsonObject handlePing(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleUserLogin(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleUserInfo(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleUserRecharge(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
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

    // ---- 订单 order.* ----
    QJsonObject handleOrderCreate(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleOrderStart(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleOrderFinish(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleOrderSettle(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleOrderCancel(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleOrderList(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleOrderDetail(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);

    // ---- 数据大屏 screen.* / ml.* ----
    QJsonObject handleScreenSnapshot(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleMlForecast(QWebSocket *sock, const QJsonObject &payload, int &code, QString &message);

    // 把一笔订单交给仿真线程 / 从仿真线程撤下(跨线程调用, 队列投递)
    void simAddOrder(int orderId, int userId, int chargerId, int stationId,
                     double startSoc, double targetSoc, double powerKw,
                     double batteryKwh, double unitPrice);
    void simRemoveOrder(int orderId);

    QWebSocketServer m_server;
    QHash<QWebSocket *, Session> m_clients;   // 在线连接 → 该连接的身份

    // 仿真器报上来的最新进度(orderId → SOC)。
    // 用户手动点"结束充电"时如果没带 end_soc, 就用这里的值 ——
    // 否则会按"真实经过了几秒"去算电量, 金额会严重偏低。
    QHash<int, double> m_liveSoc;

    // ---- 充电仿真: 工作线程 + 仿真器对象 ----
    QThread          m_simThread;
    ChargeSimulator *m_sim = nullptr;
};
