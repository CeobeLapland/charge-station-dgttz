#pragma once
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>
#include <functional>

// WebSocket 客户端封装：连接服务端进程、心跳、断线重连、请求/响应配对、推送转发。
class ServerConnection : public QObject {
    Q_OBJECT
public:
    explicit ServerConnection(QObject* parent = nullptr);

    void connectToServer(const QUrl& url);
    void disconnectFromServer();
    bool isConnected() const { return m_connected; }

    using ResponseCb = std::function<void(int code, const QString& message, const QJsonObject& payload)>;
    void sendRequest(const QString& type, const QJsonObject& payload, ResponseCb onResponse = nullptr);

signals:
    void connected();
    void disconnected();
    void pushReceived(const QJsonObject& message);  // 服务端主动推送

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);
    void sendHeartbeat();

private:
    QWebSocket m_socket;
    QUrl m_url;
    bool m_connected = false;
    int m_seq = 0;
    int m_missedHeartbeats = 0;
    QTimer m_heartbeatTimer;
    QTimer m_reconnectTimer;
    QHash<int, ResponseCb> m_pending;
};
