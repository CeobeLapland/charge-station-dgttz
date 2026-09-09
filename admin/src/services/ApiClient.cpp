#include "services/ApiClient.h"

#include "AppConfig.h"
#include "network/Protocol.h"

ApiClient::ApiClient(QObject* parent)
    : QObject(parent) {
    connect(&m_connection, &ServerConnection::connected, this, [this]() {
        m_adminAuthenticated = false;
        if (!m_account.isEmpty() && !m_password.isEmpty()) {
            authenticateCachedAdmin();
            return;
        }
        emit connectionStateChanged(true);
    });
    connect(&m_connection, &ServerConnection::disconnected, this, [this]() {
        m_adminAuthenticated = false;
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

void ApiClient::authenticateCachedAdmin() {
    QJsonObject payload;
    payload.insert(QStringLiteral("account"), m_account);
    payload.insert(QStringLiteral("password"), m_password);
    m_connection.sendRequest(proto::type::kAdminLogin, payload,
                             [this](int code, const QString&, const QJsonObject&) {
        m_adminAuthenticated = (code == proto::code::Ok);
        emit connectionStateChanged(m_adminAuthenticated);
    });
}

void ApiClient::login(const QString& account, const QString& password, ResponseCb cb) {
    m_account = account;
    m_password = password;
    QJsonObject payload;
    payload.insert(QStringLiteral("account"), account);
    payload.insert(QStringLiteral("password"), password);
    dispatch(proto::type::kAdminLogin, payload, MockDataProvider::adminLogin(account, password),
             [this, cb](int code, const QString& message, const QJsonObject& payload) {
        m_adminAuthenticated = (code == proto::code::Ok && m_connection.isConnected());
        if (cb) cb(code, message, payload);
    });
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

void ApiClient::pauseCharger(int chargerId, ResponseCb cb) {
    QJsonObject payload;
    payload.insert(QStringLiteral("charger_id"), chargerId);
    dispatch(proto::type::kAdminChargerPause, payload,
             MockDataProvider::chargerPause(chargerId), cb);
}

void ApiClient::addCharger(const QJsonObject& charger, ResponseCb cb) {
    dispatch(proto::type::kAdminChargerAdd, charger, MockDataProvider::addCharger(charger), cb);
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

void ApiClient::fetchWorkOrders(const QString& status, ResponseCb cb) {
    QJsonObject payload;
    if (!status.isEmpty()) {
        payload.insert(QStringLiteral("status"), status);
    }
    dispatch(proto::type::kAdminWorkOrderList, payload, MockDataProvider::workOrders(status), cb);
}

void ApiClient::handleWorkOrder(int workOrderId, const QString& status, const QString& result,
                                ResponseCb cb) {
    QJsonObject payload;
    payload.insert(QStringLiteral("work_order_id"), workOrderId);
    payload.insert(QStringLiteral("status"), status);
    payload.insert(QStringLiteral("result"), result);
    dispatch(proto::type::kAdminWorkOrderHandle, payload,
             MockDataProvider::handleWorkOrder(workOrderId, status, result), cb);
}

void ApiClient::fetchOrderDailyStats(ResponseCb cb) {
    static const QString kType = QStringLiteral("admin.order_daily_stats");
    dispatch(kType, QJsonObject(), MockDataProvider::orderDailyStats(), cb);
}

void ApiClient::fetchStationRevenueShare(ResponseCb cb) {
    static const QString kType = QStringLiteral("admin.station_revenue_share");
    dispatch(kType, QJsonObject(), MockDataProvider::stationRevenueShare(), cb);
}
void ApiClient::pauseStation(int stationId, ResponseCb cb) {
    QJsonObject payload;
    payload.insert(QStringLiteral("station_id"), stationId);
    dispatch(proto::type::kAdminStationPause, payload, MockDataProvider::stationPause(stationId), cb);
}

void ApiClient::resumeStation(int stationId, ResponseCb cb) {
    QJsonObject payload;
    payload.insert(QStringLiteral("station_id"), stationId);
    dispatch(proto::type::kAdminStationResume, payload, MockDataProvider::stationResume(stationId), cb);
}

void ApiClient::resumeCharger(int chargerId, ResponseCb cb) {
    QJsonObject payload;
    payload.insert(QStringLiteral("charger_id"), chargerId);
    dispatch(proto::type::kAdminChargerResume, payload, MockDataProvider::chargerResume(chargerId), cb);
}
void ApiClient::fetchHealthRanks(ResponseCb cb) {
    dispatch(proto::type::kAdminFaultRisk, QJsonObject(), MockDataProvider::healthRanks(), cb);
}
