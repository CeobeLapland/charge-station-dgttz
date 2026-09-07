#include "UserClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

namespace {
// 默认服务端地址：spec-协议.md 待定项建议初值
const QString kDefaultServerUrl = QStringLiteral("ws://127.0.0.1:9000");
// 心跳间隔 / 断线重连间隔（毫秒）
constexpr int kHeartbeatMs = 15000;
constexpr int kReconnectMs = 3000;
}  // namespace

UserClient::UserClient(QObject* parent)
    : QObject(parent), m_socket(), m_heartbeat(this), m_reconnect(this) {
    connect(&m_socket, &QWebSocket::connected,
            this, &UserClient::onConnected);
    connect(&m_socket, &QWebSocket::disconnected,
            this, &UserClient::onDisconnected);
    connect(&m_socket, &QWebSocket::textMessageReceived,
            this, &UserClient::onTextMessageReceived);

    m_heartbeat.setInterval(kHeartbeatMs);
    connect(&m_heartbeat, &QTimer::timeout, this, &UserClient::onHeartbeat);

    m_reconnect.setInterval(kReconnectMs);
    connect(&m_reconnect, &QTimer::timeout, this, &UserClient::onReconnectTimer);
}

void UserClient::connectServer(const QString& url) {
    m_url = url.isEmpty() ? kDefaultServerUrl : url;
    m_socket.open(QUrl(m_url));
}

void UserClient::login(const QString& phone) {
    QJsonObject payload;
    payload[QStringLiteral("phone")] = phone;
    send(QStringLiteral("user.login"), payload);
}

void UserClient::send(const QString& type, const QJsonObject& payload) {
    if (!m_socket.isValid())
        return;  // 未连接：静默丢弃（各数据源保留本地 mock 兜底）

    QJsonObject envelope;
    envelope[QStringLiteral("type")] = type;
    envelope[QStringLiteral("seq")] = ++m_seq;
    envelope[QStringLiteral("payload")] = payload;

    const QByteArray data = QJsonDocument(envelope).toJson(QJsonDocument::Compact);
    m_socket.sendTextMessage(QString::fromUtf8(data));
}

void UserClient::sendMap(const QString& type, const QVariantMap& payload) {
    send(type, QJsonObject::fromVariantMap(payload));
}

void UserClient::onConnected() {
    qInfo().noquote() << "[UserClient] connected:" << m_url;
    m_heartbeat.start();
    emit connected();
}

void UserClient::onDisconnected() {
    qInfo().noquote() << "[UserClient] disconnected";
    m_heartbeat.stop();
    if (!m_url.isEmpty())
        m_reconnect.start();  // 断线自动重连
    emit disconnected();
}

void UserClient::onHeartbeat() {
    send(QStringLiteral("system.ping"), QJsonObject());
}

void UserClient::onReconnectTimer() {
    if (!m_url.isEmpty() && !m_socket.isValid()) {
        qInfo().noquote() << "[UserClient] reconnecting...";
        m_socket.open(QUrl(m_url));
    }
}

void UserClient::onTextMessageReceived(const QString& message) {
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject())
        return;

    const QJsonObject obj = doc.object();
    const QString type = obj[QStringLiteral("type")].toString();
    const int code = obj[QStringLiteral("code")].toInt();
    const QString text = obj[QStringLiteral("message")].toString();
    const QJsonObject payload = obj[QStringLiteral("payload")].toObject();

    emit messageReceived(type, code, text, payload);
}
