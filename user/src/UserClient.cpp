#include "UserClient.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace {
// 默认服务端地址：spec-协议.md 待定项建议初值
const QString kDefaultServerUrl = QStringLiteral("ws://127.0.0.1:9000");
}  // namespace

UserClient::UserClient(QObject* parent)
    : QObject(parent), m_socket() {
    connect(&m_socket, &QWebSocket::connected,
            this, &UserClient::onConnected);
    connect(&m_socket, &QWebSocket::disconnected,
            this, &UserClient::onDisconnected);
    connect(&m_socket, &QWebSocket::textMessageReceived,
            this, &UserClient::onTextMessageReceived);
}

void UserClient::connectServer(const QString& url) {
    const QString target = url.isEmpty() ? kDefaultServerUrl : url;
    m_socket.open(QUrl(target));
}

void UserClient::login(const QString& phone) {
    QJsonObject payload;
    payload[QStringLiteral("phone")] = phone;
    send(QStringLiteral("user.login"), payload);
}

void UserClient::send(const QString& type, const QJsonObject& payload) {
    QJsonObject envelope;
    envelope[QStringLiteral("type")] = type;
    envelope[QStringLiteral("seq")] = ++m_seq;
    envelope[QStringLiteral("payload")] = payload;

    const QByteArray data = QJsonDocument(envelope).toJson(QJsonDocument::Compact);
    if (m_socket.isValid())
        m_socket.sendTextMessage(QString::fromUtf8(data));
}

void UserClient::onConnected() {
    emit connected();
}

void UserClient::onDisconnected() {
    emit disconnected();
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