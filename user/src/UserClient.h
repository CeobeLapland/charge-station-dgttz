#pragma once

#include <QObject>
#include <QWebSocket>

// 用户端与服务端的统一 WebSocket 通信层。
// 遵循 docs/spec-协议.md：信封 {type, seq, payload}，服务端地址 ws://127.0.0.1:9000（端口待定项建议初值）。
class UserClient : public QObject {
    Q_OBJECT

public:
    explicit UserClient(QObject* parent = nullptr);

    Q_INVOKABLE void connectServer(const QString& url);
    Q_INVOKABLE void login(const QString& phone);
    Q_INVOKABLE void send(const QString& type, const QJsonObject& payload);

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString& type, int code, const QString& message,
                         const QJsonObject& payload);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);

private:
    QWebSocket m_socket;
    int m_seq = 0;
};