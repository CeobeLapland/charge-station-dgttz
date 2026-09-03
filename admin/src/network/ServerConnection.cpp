#include "network/ServerConnection.h"

#include <QJsonDocument>
#include <QJsonValue>

#include "AppConfig.h"
#include "network/Protocol.h"

ServerConnection::ServerConnection(QObject* parent)
    : QObject(parent) {
    m_heartbeatTimer.setInterval(appconfig::kHeartbeatIntervalMs);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &ServerConnection::sendHeartbeat);

    m_reconnectTimer.setInterval(appconfig::kReconnectIntervalMs);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
        connectToServer(m_url);
    });

    connect(&m_socket, &QWebSocket::connected, this, &ServerConnection::onConnected);
    connect(&m_socket, &QWebSocket::disconnected, this, &ServerConnection::onDisconnected);
    connect(&m_socket, &QWebSocket::textMessageReceived,
            this, &ServerConnection::onTextMessageReceived);
}

void ServerConnection::connectToServer(const QUrl& url) {
    m_url = url;
    m_reconnectTimer.stop();
    m_socket.open(url);
}

void ServerConnection::disconnectFromServer() {
    m_reconnectTimer.stop();
    m_socket.close();
}

void ServerConnection::onConnected() {
    m_connected = true;
    m_missedHeartbeats = 0;
    m_heartbeatTimer.start();
    emit connected();
}

void ServerConnection::onDisconnected() {
    m_connected = false;
    m_heartbeatTimer.stop();
    m_missedHeartbeats = 0;
    emit disconnected();
    if (!m_url.isEmpty()) {
        m_reconnectTimer.start();
    }
}

void ServerConnection::sendRequest(const QString& type, const QJsonObject& payload,
                                   ResponseCb onResponse) {
    if (!m_connected) {
        if (onResponse) {
            onResponse(proto::code::DeviceOffline, QStringLiteral("未连接服务端"), QJsonObject());
        }
        return;
    }
    const int seq = ++m_seq;
    if (onResponse) {
        m_pending.insert(seq, onResponse);
    }
    QJsonObject envelope;
    envelope.insert(proto::field::kType, type);
    envelope.insert(proto::field::kSeq, seq);
    envelope.insert(proto::field::kPayload, payload);
    m_socket.sendTextMessage(
        QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
}

void ServerConnection::onTextMessageReceived(const QString& message) {
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }
    const QJsonObject obj = doc.object();

    // 心跳 pong
    if (obj.value(proto::field::kType).toString() == proto::type::kSystemPong) {
        m_missedHeartbeats = 0;
    }

    // 有 seq → 请求响应
    const QJsonValue seqVal = obj.value(proto::field::kSeq);
    if (seqVal.isDouble()) {
        const int seq = seqVal.toInt();
        auto it = m_pending.find(seq);
        if (it != m_pending.end()) {
            const int code = obj.value(proto::field::kCode).toInt(proto::code::BadMessageFormat);
            const QString messageText = obj.value(proto::field::kMessage).toString();
            const QJsonObject payload = obj.value(proto::field::kPayload).toObject();
            auto cb = it.value();
            m_pending.erase(it);
            if (cb) {
                cb(code, messageText, payload);
            }
            return;
        }
    }

    // 无 seq → 服务端主动推送
    emit pushReceived(obj);
}

void ServerConnection::sendHeartbeat() {
    if (!m_connected) {
        return;
    }
    if (++m_missedHeartbeats > appconfig::kHeartbeatTimeoutCount) {
        m_socket.close();  // 触发 onDisconnected → 自动重连
        return;
    }
    QJsonObject ping;
    ping.insert(proto::field::kType, proto::type::kSystemPing);
    m_socket.sendTextMessage(
        QString::fromUtf8(QJsonDocument(ping).toJson(QJsonDocument::Compact)));
}
