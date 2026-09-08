#pragma once
#include <QHash>
#include <QSet>
#include <QWidget>

class QTableWidget;
class ApiClient;

class StationManagePage : public QWidget {
    Q_OBJECT
public:
    explicit StationManagePage(ApiClient* api, QWidget* parent = nullptr);
signals:
    void openChargerRequested(int chargerId);

private slots:
    void refresh();
    void onAddStation();
    void onShowDetail();
    void onPauseStation();
    void onResumeStation();

private:
    ApiClient* m_api = nullptr;
    QTableWidget* m_table = nullptr;
    QSet<int> m_frozenStations;
    QHash<int, QList<int>> m_pausedChargers;
};
