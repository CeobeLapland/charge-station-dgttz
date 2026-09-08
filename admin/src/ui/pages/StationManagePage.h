#pragma once
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

private slots:
    void refresh();
    void onAddStation();
    void onShowDetail();
    void selectStationOnMap(int stationId);
    void selectRowByStationId(int stationId);
    void updateStationMap(const QJsonArray& stations);

private:
    ApiClient* m_api = nullptr;
    QTableWidget* m_table = nullptr;
    QWebEngineView* m_mapView = nullptr;   // Linux+QWebEngine 的地图
    QLabel* m_mapFallback = nullptr;
};
