#include "services/ApiClient.h"

#include "AppConfig.h"
#include "network/Protocol.h"

ApiClient::ApiClient(QObject* parent)
    : QObject(parent) {
    connect(&m_connection, &ServerConnection::connected, this, [this]() {
        emit connectionStateChanged(true);
    });
    connect(&m_connection, &ServerConnection::disconnected, this, [this]() {
        emit connectionStateChanged(false);
    });
    connect(&m_connection, &ServerConnection::pushReceived, this, &ApiClient::pushReceived);
}

void ApiClient::start() {
    m_connection.connectToServer(QUrl(appconfig::kServerUrl));
}

void ApiClient::stop() {
    m_connection.disconnectFromServer();
}

void ApiClient::dispatch(const QString& type, const QJsonObject& payload,
                         const QJsonObject& mockResult, ResponseCb cb) {
    if (!m_connection.isConnected()) {
        if (cb) {
            cb(mockResult.value(proto::field::kCode).toInt(proto::code::Ok),
               mockResult.value(proto::field::kMessage).toString(QStringLiteral("ok")),
               mockResult.value(proto::field::kPayload).toObject());
        }
        return;
    }
    m_connection.sendRequest(type, payload, cb);
}

void ApiClient::login(const QString& account, const QString& password, ResponseCb cb) {
    QJsonObject payload;
    payload.insert(QStringLiteral("account"), account);
    payload.insert(QStringLiteral("password"), password);
    dispatch(proto::type::kAdminLogin, payload, MockDataProvider::adminLogin(account, password), cb);
}

void ApiClient::fetchRevenue(int days, ResponseCb cb) {
    QJsonObject payload;
    payload.insert(QStringLiteral("days"), days);
    dispatch(proto::type::kAdminRevenue, payload, MockDataProvider::revenue(days), cb);
}

void ApiClient::fetchStationStatus(ResponseCb cb) {
    dispatch(proto::type::kAdminStationStatus, QJsonObject(),
             MockDataProvider::stationStatus(), cb);
}

void ApiClient::fetchStations(ResponseCb cb) {
    dispatch(proto::type::kAdminStationList, QJsonObject(), MockDataProvider::stations(), cb);
}

void ApiClient::fetchChargers(int stationId, ResponseCb cb) {
    QJsonObject payload;
    if (stationId > 0) {
        payload.insert(QStringLiteral("station_id"), stationId);
    }
    dispatch(proto::type::kAdminChargerList, payload, MockDataProvider::chargers(stationId), cb);
}

void ApiClient::fetchUsers(const QString& keyword, ResponseCb cb) {
    QJsonObject payload;
    if (!keyword.isEmpty()) {
        payload.insert(QStringLiteral("keyword"), keyword);
    }
    dispatch(proto::type::kAdminUserList, payload, MockDataProvider::users(keyword), cb);
}

void ApiClient::addStation(const QJsonObject& station, ResponseCb cb) {
    dispatch(proto::type::kAdminStationAdd, station, MockDataProvider::addStation(station), cb);
}

void ApiClient::restartCharger(int chargerId, ResponseCb cb) {
    QJsonObject payload;
    payload.insert(QStringLiteral("charger_id"), chargerId);
    dispatch(proto::type::kAdminChargerRestart, payload,
             MockDataProvider::chargerRestart(chargerId), cb);
}

void ApiClient::toggleUserStatus(int userId, const QString& status, ResponseCb cb) {
    QJsonObject payload;
    payload.insert(QStringLiteral("user_id"), userId);
    payload.insert(QStringLiteral("status"), status);
    dispatch(proto::type::kAdminUserToggleStatus, payload,
             MockDataProvider::toggleUserStatus(userId, status), cb);
}

void ApiClient::fetchDeviceLogs(int chargerId, ResponseCb cb) {
    QJsonObject payload;
    payload.insert(QStringLiteral("charger_id"), chargerId);
    dispatch(proto::type::kAdminDeviceLog, payload, MockDataProvider::deviceLogs(chargerId), cb);
}

void ApiClient::fetchHealthRanks(ResponseCb cb) {
    dispatch(proto::type::kAdminFaultRisk, QJsonObject(), MockDataProvider::healthRanks(), cb);
}
