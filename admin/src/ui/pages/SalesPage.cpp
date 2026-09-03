#include "ui/pages/SalesPage.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>

#include "network/Protocol.h"
#include "services/ApiClient.h"

namespace {
QString money(double v) {
    return QString::number(v, 'f', 2) + QStringLiteral(" 元");
}
}  // namespace

SalesPage::SalesPage(ApiClient* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    auto* title = new QLabel(QStringLiteral("销售业绩"));
    title->setObjectName(QStringLiteral("pageTitle"));

    m_rangeCombo = new QComboBox;
    m_rangeCombo->addItem(QStringLiteral("近 7 日"), 7);
    m_rangeCombo->addItem(QStringLiteral("近 30 日"), 30);

    auto* metricRow = new QHBoxLayout;
    auto makeMetric = [](const QString& caption, QLabel*& valueLabel) {
        auto* box = new QGroupBox(caption);
        auto* v = new QVBoxLayout(box);
        valueLabel = new QLabel(QStringLiteral("--"));
        valueLabel->setObjectName(QStringLiteral("metricLabel"));
        v->addWidget(valueLabel);
        return box;
    };
    metricRow->addWidget(makeMetric(QStringLiteral("今日营收"), m_todayLabel));
    metricRow->addWidget(makeMetric(QStringLiteral("本月营收"), m_monthLabel));
    metricRow->addWidget(makeMetric(QStringLiteral("总营收"), m_totalLabel));

    auto* chart = new QChart;
    chart->setTitle(QStringLiteral("营收趋势"));
    chart->legend()->hide();
    m_series = new QLineSeries;
    chart->addSeries(m_series);
    m_axisX = new QValueAxis;
    m_axisY = new QValueAxis;
    chart->addAxis(m_axisX, Qt::AlignBottom);
    chart->addAxis(m_axisY, Qt::AlignLeft);
    m_series->attachAxis(m_axisX);
    m_series->attachAxis(m_axisY);
    m_chartView = new QChartView(chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(title);
    topRow->addStretch();
    topRow->addWidget(m_rangeCombo);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addLayout(metricRow);
    layout->addWidget(m_chartView, 1);

    connect(m_rangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        refresh(m_rangeCombo->itemData(index).toInt());
    });

    refresh(7);
}

void SalesPage::refresh(int days) {
    m_api->fetchRevenue(days,
                        [this](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        updateMetrics(payload);
        const QJsonArray trend = payload.value(QStringLiteral("trend")).toArray();
        m_series->clear();
        double maxY = 1.0;
        for (int i = 0; i < trend.size(); ++i) {
            const QJsonObject item = trend.at(i).toObject();
            const double amount = item.value(QStringLiteral("amount")).toDouble();
            m_series->append(i, amount);
            maxY = qMax(maxY, amount);
        }
        m_axisX->setRange(0, qMax(trend.size() - 1, 1));
        m_axisY->setRange(0, maxY * 1.2);
    });
}

void SalesPage::updateMetrics(const QJsonObject& payload) {
    m_todayLabel->setText(money(payload.value(QStringLiteral("today")).toDouble()));
    m_monthLabel->setText(money(payload.value(QStringLiteral("month")).toDouble()));
    m_totalLabel->setText(money(payload.value(QStringLiteral("total")).toDouble()));
}
