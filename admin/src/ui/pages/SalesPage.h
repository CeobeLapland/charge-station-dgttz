#pragma once
#include <QHash>
#include <QList>
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
class QEvent;
class ApiClient;

class SalesPage : public QWidget {
    Q_OBJECT
public:
    explicit SalesPage(ApiClient* api, QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void refresh(int days);

private:
    void updateMetrics(const QJsonObject& payload, int days, double rangeSum);
    void showTipText(const QString& text);
    void hideTip();

    ApiClient* m_api = nullptr;
    QComboBox* m_rangeCombo = nullptr;
    QGroupBox* m_rangeBox = nullptr;
    QLabel* m_todayLabel = nullptr;
    QLabel* m_rangeLabel = nullptr;
    QLabel* m_monthLabel = nullptr;
    QLabel* m_totalLabel = nullptr;

    QChartView* m_lineChartView = nullptr;
    QLineSeries* m_lineSeries = nullptr;
    QScatterSeries* m_scatterSeries = nullptr;
    QBarCategoryAxis* m_lineAxisX = nullptr;
    QValueAxis* m_lineAxisY = nullptr;

    QChartView* m_barView = nullptr;
    QBarSeries* m_energySeries = nullptr;
    QBarSeries* m_ordersSeries = nullptr;
    QBarSet* m_energySet = nullptr;
    QBarSet* m_ordersSet = nullptr;
    QBarCategoryAxis* m_barAxisX = nullptr;
    QValueAxis* m_energyAxisY = nullptr;
    QValueAxis* m_ordersAxisY = nullptr;

    QChartView* m_pieView = nullptr;
    QPieSeries* m_pieSeries = nullptr;

    QLabel* m_tip = nullptr;      // 自绘气泡：Qt/系统不会自动隐藏
    int m_lastEnergyIdx = -1;     // 当前被放大的柱
    int m_lastOrdersIdx = -1;

    QStringList m_dayDates;
    QList<double> m_dayAmounts;
    QList<double> m_dayEnergies;
    QList<double> m_dayOrders;
    QHash<QString, int> m_ordersByDate;
    QHash<QString, double> m_energyByDate;
    QList<QPair<QString, double>> m_stationRows;
};