#pragma once
#include <QJsonObject>
#include <QObject>
#include <functional>

#include "network/MockDataProvider.h"
#include "network/ServerConnection.h"

// 统一业务请求入口：服务端在线走 WebSocket，离线走 MockDataProvider。
class ApiClient : public QObject {
    Q_OBJECT
public:
    explicit ApiClient(QObject* parent = nullptr);

    void start();  // 尝试连接服务端进程
    void stop();
    bool isMockMode() const { return !m_connection.isConnected(); }

    using ResponseCb = std::function<void(int code, const QString& message, const QJsonObject& payload)>;

    // 基础模块
    void login(const QString& account, const QString& password, ResponseCb cb);
    void fetchRevenue(int days, ResponseCb cb);
    void fetchStationStatus(ResponseCb cb);
    void fetchStations(ResponseCb cb);
    void fetchChargers(int stationId, ResponseCb cb);
    void fetchUsers(const QString& keyword, ResponseCb cb);
    void addStation(const QJsonObject& station, ResponseCb cb);
    void restartCharger(int chargerId, ResponseCb cb);
    void toggleUserStatus(int userId, const QString& status, ResponseCb cb);
    void fetchDeviceLogs(int chargerId, ResponseCb cb);

    // 增强模块（占位）
    void fetchHealthRanks(ResponseCb cb);

signals:
    void connectionStateChanged(bool connected);
    void pushReceived(const QJsonObject& message);

private:
    void dispatch(const QString& type, const QJsonObject& payload,
                  const QJsonObject& mockResult, ResponseCb cb);

    ServerConnection m_connection;
};
