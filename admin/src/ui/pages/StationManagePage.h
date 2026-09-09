#pragma once
#include <QHash>
#include <QSet>
#include <QJsonArray>
#include <QWidget>

class QTableWidget;
class QWebEngineView;
class QLabel;
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
    void selectStationOnMap(int stationId);
    void selectRowByStationId(int stationId);
    void updateStationMap(const QJsonArray& stations);
    void onPauseStation();
    void onResumeStation();

private:
    ApiClient* m_api = nullptr;
    QTableWidget* m_table = nullptr;
    QWebEngineView* m_mapView = nullptr;
    QLabel* m_mapFallback = nullptr;
    QSet<int> m_frozenStations;
    QHash<int, QList<int>> m_pausedChargers;
};


