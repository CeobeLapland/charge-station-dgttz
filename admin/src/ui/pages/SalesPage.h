#pragma once
#include <QWidget>
#include <QJsonObject>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QValueAxis>

class QComboBox;
class QGroupBox;
class QLabel;
class ApiClient;

class SalesPage : public QWidget {
    Q_OBJECT
public:
    explicit SalesPage(ApiClient* api, QWidget* parent = nullptr);

private slots:
    void refresh(int days);

private:
    void updateMetrics(const QJsonObject& payload, int days, double rangeSum);

    ApiClient* m_api = nullptr;
    QComboBox* m_rangeCombo = nullptr;
    QGroupBox* m_rangeBox = nullptr;
    QLabel* m_todayLabel = nullptr;
    QLabel* m_rangeLabel = nullptr;
    QLabel* m_monthLabel = nullptr;
    QLabel* m_totalLabel = nullptr;

    // 折线：营收趋势（X 轴为日期）
    QChartView* m_lineChartView = nullptr;
    QLineSeries* m_lineSeries = nullptr;
    QScatterSeries* m_scatterSeries = nullptr;
    QBarCategoryAxis* m_lineAxisX = nullptr;
    QValueAxis* m_lineAxisY = nullptr;
    // 柱状（合并）：每日充电量 + 每日订单量（双 Y 轴）
    QChartView* m_barView = nullptr;
    QBarSeries* m_energySeries = nullptr;
    QBarSeries* m_ordersSeries = nullptr;
    QBarCategoryAxis* m_barAxisX = nullptr;
    QValueAxis* m_energyAxisY = nullptr;   // 左轴：kWh
    QValueAxis* m_ordersAxisY = nullptr;   // 右轴：单
    // 饼图：站点营收占比
    QChartView* m_pieView = nullptr;
    QPieSeries* m_pieSeries = nullptr;
};