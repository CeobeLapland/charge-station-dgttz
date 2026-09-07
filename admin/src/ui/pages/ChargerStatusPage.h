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

private:
    void refresh();
    void showTipText(const QString& text);
    ApiClient* m_api = nullptr;
    QTableWidget* m_table = nullptr;
    QPieSeries* m_pieSeries = nullptr;
    QChartView* m_pieView = nullptr;
    QLabel* m_tip = nullptr;   // 自绘气泡
};