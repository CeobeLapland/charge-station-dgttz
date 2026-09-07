#pragma once

#include <QObject>
#include <QTimer>
#include <QWebSocket>

// 用户端与服务端的统一 WebSocket 通信层。
// 遵循 docs/spec-协议.md：信封 {type, seq, payload}，服务端地址 ws://127.0.0.1:9000（端口待定项建议初值）。
// 增强（接线用）：
//  - 心跳：连接后每 15s 发 system.ping；
//  - 断线重连：断线后每 3s 重试 connectServer 时的地址；
//  - sendMap：把 QVariantMap payload 转成 QJsonObject 发出，QML 调用更方便。
class UserClient : public QObject {
    Q_OBJECT

public:
    explicit UserClient(QObject* parent = nullptr);

    Q_INVOKABLE void connectServer(const QString& url);
    Q_INVOKABLE void login(const QString& phone);
    Q_INVOKABLE void send(const QString& type, const QJsonObject& payload);
    Q_INVOKABLE void sendMap(const QString& type, const QVariantMap& payload);

    Q_INVOKABLE bool isConnected() const { return m_socket.isValid(); }

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString& type, int code, const QString& message,
                         const QJsonObject& payload);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);
    void onHeartbeat();
    void onReconnectTimer();

private:
    QWebSocket m_socket;
    QTimer m_heartbeat;
    QTimer m_reconnect;
    QString m_url;
    int m_seq = 0;
};
