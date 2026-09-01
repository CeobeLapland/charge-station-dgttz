#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// Mock 数据层：服务端进程未就绪时，用它跑通管理端界面。
// 联调阶段由 ApiClient 切换到真实 WebSocket；本类保留用于离线演示。
class MockDataProvider {
public:
    static QJsonObject adminLogin(const QString& account, const QString& password);
    static QJsonObject revenue(int days);
    static QJsonObject stationStatus();
    static QJsonObject stations();
    static QJsonObject chargers(int stationId);
    static QJsonObject users(const QString& keyword);
    static QJsonObject addStation(const QJsonObject& station);
    static QJsonObject chargerRestart(int chargerId);
    static QJsonObject toggleUserStatus(int userId, const QString& status);
    static QJsonObject deviceLogs(int chargerId);
    static QJsonObject healthRanks();

private:
    static QJsonObject okPayload(const QJsonObject& payload);
    static QJsonObject errPayload(int code, const QString& message);
};
