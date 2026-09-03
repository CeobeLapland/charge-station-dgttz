#pragma once
#include <QJsonObject>
#include <QList>
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
//   错误码: 0成功 1002手机号格式错误 1003用户被冻结
//           4001数据不存在 9001消息格式错误 9002未知消息类型
// ============================================================
class WsServer : public QObject
{
    Q_OBJECT
public:
    explicit WsServer(quint16 port, QObject *parent = nullptr);
    bool isListening() const;

private slots:
    void onNewConnection();                       // 有新客户端连上来
    void onTextMessage(const QString &message);   // 收到一条文本消息
    void onDisconnected();                        // 客户端断开

private:
    // 分发中心: 按 type 调用对应处理函数。
    // 返回响应的 payload; code/message 通过引用参数回填。
    QJsonObject dispatch(const QString &type, const QJsonObject &payload,
                         int &code, QString &message);

    // ---- 各消息处理函数(每支持一种消息, 就在这里加一个) ----
    QJsonObject handlePing(const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleUserLogin(const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleStationNearby(const QJsonObject &payload, int &code, QString &message);
    QJsonObject handleStationDetail(const QJsonObject &payload, int &code, QString &message);

    QWebSocketServer m_server;
    QList<QWebSocket *> m_clients;   // 当前在线的连接(将来做广播推送用)
};
