#pragma once
#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QPieSeries>

class QLabel;
class QTableWidget;
class ApiClient;

class ChargerStatusPage : public QWidget {
    Q_OBJECT
public:
    explicit ChargerStatusPage(ApiClient* api, QWidget* parent = nullptr);
signals:
    void openChargerRequested(int chargerId);

private:
    void refresh();
    void showTipText(const QString& text);
    void showStatusChargers(const QString& statusKey, const QString& statusLabel);

    ApiClient* m_api = nullptr;
    QTableWidget* m_table = nullptr;
    QTableWidget* m_riskTable = nullptr;
    QPieSeries* m_pieSeries = nullptr;
    QChartView* m_pieView = nullptr;
    QLabel* m_tip = nullptr;
};