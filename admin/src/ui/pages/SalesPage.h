#pragma once
#include <QWidget>
#include <QJsonObject>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

class QComboBox;
class QLabel;
class ApiClient;

class SalesPage : public QWidget {
    Q_OBJECT
public:
    explicit SalesPage(ApiClient* api, QWidget* parent = nullptr);

private slots:
    void refresh(int days);

private:
    void updateMetrics(const QJsonObject& payload);

    ApiClient* m_api = nullptr;
    QComboBox* m_rangeCombo = nullptr;
    QLabel* m_todayLabel = nullptr;
    QLabel* m_monthLabel = nullptr;
    QLabel* m_totalLabel = nullptr;
    QChartView* m_chartView = nullptr;
    QLineSeries* m_series = nullptr;
    QValueAxis* m_axisX = nullptr;
    QValueAxis* m_axisY = nullptr;
};
