#pragma once
#include <QMainWindow>

class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class ApiClient;
class ChargerManagePage;
class ChargerStatusPage;
class DecisionPage;
class DeviceOpsPage;
class SalesPage;
class StationManagePage;
class UserManagePage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ApiClient* api, const QString& account, QWidget* parent = nullptr);

signals:
    void logoutRequested();

private slots:
    void onLogout();
    void onAiAssistant();

private:
    ApiClient* m_api = nullptr;
    QString m_account;
    QListWidget* m_navList = nullptr;
    QStackedWidget* m_pages = nullptr;
    QLabel* m_statusLabel = nullptr;

    SalesPage* m_salesPage = nullptr;
    ChargerStatusPage* m_chargerStatusPage = nullptr;
    ChargerManagePage* m_chargerManagePage = nullptr;
    StationManagePage* m_stationManagePage = nullptr;
    UserManagePage* m_userManagePage = nullptr;
    DeviceOpsPage* m_deviceOpsPage = nullptr;
    DecisionPage* m_decisionPage = nullptr;
};
