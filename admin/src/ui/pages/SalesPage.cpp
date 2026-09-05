#include "ui/pages/SalesPage.h"

#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QPieSlice>

#include "network/Protocol.h"
#include "services/ApiClient.h"

namespace {
QString money(double v) {
    return QString::number(v, 'f', 2) + QStringLiteral(" 元");
}

// 让分类轴标签斜放 + 缩小，避免 X 轴糊成一片
void styleCategoryAxis(QBarCategoryAxis* axis) {
    axis->setLabelsAngle(-60);
    QFont f = axis->labelsFont();
    f.setPointSize(7);
    axis->setLabelsFont(f);
}

// 重建某个柱状 series（单个 QBarSet）
void rebuildBarSet(QBarSeries* series, const QString& setLabel, const QList<double>& values,
                   const QColor& color) {
    while (!series->barSets().isEmpty()) {
        QBarSet* s = series->barSets().first();
        series->take(s);
        delete s;
    }
    auto* set = new QBarSet(setLabel);
    set->setColor(color);
    for (double v : values) {
        *set << v;
    }
    series->append(set);
}
}  // namespace

SalesPage::SalesPage(ApiClient* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    auto* title = new QLabel(QStringLiteral("销售业绩"));
    title->setObjectName(QStringLiteral("pageTitle"));

    m_rangeCombo = new QComboBox;
    m_rangeCombo->addItem(QStringLiteral("近 7 日"), 7);
    m_rangeCombo->addItem(QStringLiteral("近 30 日"), 30);

    // ---- 4 张指标卡 ----
    auto* metricRow = new QHBoxLayout;
    auto makeMetric = [](const QString& caption, QLabel*& valueLabel, QGroupBox** boxPtr = nullptr) {
        auto* box = new QGroupBox(caption);
        auto* v = new QVBoxLayout(box);
        valueLabel = new QLabel(QStringLiteral("--"));
        valueLabel->setObjectName(QStringLiteral("metricLabel"));
        v->addWidget(valueLabel);
        if (boxPtr) {
            *boxPtr = box;
        }
        return box;
    };
    metricRow->addWidget(makeMetric(QStringLiteral("今日营收"), m_todayLabel));
    metricRow->addWidget(makeMetric(QStringLiteral("近 7 日营收"), m_rangeLabel, &m_rangeBox));
    metricRow->addWidget(makeMetric(QStringLiteral("本月营收"), m_monthLabel));
    metricRow->addWidget(makeMetric(QStringLiteral("总营收"), m_totalLabel));

    // ---- 折线：营收趋势（日期 X 轴 + 加粗线 + 每点大标记）----
    auto* lineChart = new QChart;
    lineChart->setTitle(QStringLiteral("营收趋势"));
    lineChart->legend()->hide();
    lineChart->setAnimationOptions(QChart::AllAnimations);
    m_lineSeries = new QLineSeries;
    QPen linePen(QColor(0x4f, 0x9e, 0xff));
    linePen.setWidth(2);
    m_lineSeries->setPen(linePen);
    lineChart->addSeries(m_lineSeries);
    m_scatterSeries = new QScatterSeries;
    m_scatterSeries->setMarkerSize(9.0);
    m_scatterSeries->setColor(QColor(0x4f, 0x9e, 0xff));
    m_scatterSeries->setBorderColor(QColor(0xff, 0xff, 0xff));
    lineChart->addSeries(m_scatterSeries);
    m_lineAxisX = new QBarCategoryAxis;
    styleCategoryAxis(m_lineAxisX);
    m_lineAxisY = new QValueAxis;
    lineChart->addAxis(m_lineAxisX, Qt::AlignBottom);
    lineChart->addAxis(m_lineAxisY, Qt::AlignLeft);
    m_lineSeries->attachAxis(m_lineAxisX);
    m_lineSeries->attachAxis(m_lineAxisY);
    m_scatterSeries->attachAxis(m_lineAxisX);
    m_scatterSeries->attachAxis(m_lineAxisY);
    m_lineChartView = new QChartView(lineChart);
    m_lineChartView->setRenderHint(QPainter::Antialiasing);

    // ---- 柱状（合并）：充电量(左轴) + 订单量(右轴) ----
    auto* barChart = new QChart;
    barChart->setAnimationOptions(QChart::AllAnimations);
    barChart->setTitle(QStringLiteral("每日充电量与订单量"));
    barChart->legend()->setVisible(true);
    barChart->legend()->setAlignment(Qt::AlignBottom);
    m_energySeries = new QBarSeries;
    m_ordersSeries = new QBarSeries;
    barChart->addSeries(m_energySeries);
    barChart->addSeries(m_ordersSeries);
    m_barAxisX = new QBarCategoryAxis;
    styleCategoryAxis(m_barAxisX);
    m_energyAxisY = new QValueAxis;
    m_ordersAxisY = new QValueAxis;
    m_energyAxisY->setTitleText(QStringLiteral("充电量(kWh)"));
    m_ordersAxisY->setTitleText(QStringLiteral("订单量(单)"));
    m_ordersAxisY->setLabelsColor(QColor(0xff, 0xb0, 0x4d));
    m_ordersAxisY->setTitleBrush(QColor(0xff, 0xb0, 0x4d));
    barChart->addAxis(m_barAxisX, Qt::AlignBottom);
    barChart->addAxis(m_energyAxisY, Qt::AlignLeft);
    barChart->addAxis(m_ordersAxisY, Qt::AlignRight);
    m_energySeries->attachAxis(m_barAxisX);
    m_energySeries->attachAxis(m_energyAxisY);
    m_ordersSeries->attachAxis(m_barAxisX);
    m_ordersSeries->attachAxis(m_ordersAxisY);
    m_barView = new QChartView(barChart);
    m_barView->setRenderHint(QPainter::Antialiasing);

    // ---- 饼图：站点营收占比 ----
    auto* pieChart = new QChart;
    pieChart->setTitle(QStringLiteral("站点营收占比"));
    pieChart->legend()->hide();
    pieChart->setAnimationOptions(QChart::AllAnimations);
    m_pieSeries = new QPieSeries;
    pieChart->addSeries(m_pieSeries);
    m_pieView = new QChartView(pieChart);
    m_pieView->setRenderHint(QPainter::Antialiasing);

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(title);
    topRow->addStretch();
    topRow->addWidget(m_rangeCombo);

    auto* chartsRow = new QHBoxLayout;
    chartsRow->addWidget(m_barView, 2);
    chartsRow->addWidget(m_pieView, 1);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addLayout(metricRow);
    layout->addWidget(m_lineChartView, 2);
    layout->addLayout(chartsRow, 2);

    connect(m_rangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        refresh(m_rangeCombo->itemData(index).toInt());
    });

    refresh(7);
}

void SalesPage::refresh(int days) {
    m_api->fetchRevenue(days,
                        [this, days](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        const QJsonArray trend = payload.value(QStringLiteral("trend")).toArray();

        QStringList categories;
        QList<double> amounts, energyVals, orderVals;
        for (int i = 0; i < trend.size(); ++i) {
            const QJsonObject item = trend.at(i).toObject();
            const QString date = item.value(QStringLiteral("date")).toString();
            categories << (date.size() >= 10 ? date.mid(5) : date);
            amounts << item.value(QStringLiteral("amount")).toDouble();
            energyVals << item.value(QStringLiteral("energy")).toDouble();
            orderVals << item.value(QStringLiteral("orders")).toDouble();
        }

        // 折线（每个点加标记）
        m_lineSeries->clear();
        m_scatterSeries->clear();
        double maxAmount = 1.0, rangeSum = 0.0;
        for (int i = 0; i < amounts.size(); ++i) {
            m_lineSeries->append(i, amounts.at(i));
            m_scatterSeries->append(i, amounts.at(i));
            maxAmount = qMax(maxAmount, amounts.at(i));
            rangeSum += amounts.at(i);
        }
        m_lineAxisX->setCategories(categories);
        m_lineAxisY->setRange(0, maxAmount * 1.2);

        // 柱状（双轴）
        rebuildBarSet(m_energySeries, QStringLiteral("充电量(kWh)"), energyVals,
                      QColor(0x4f, 0x9e, 0xff));
        rebuildBarSet(m_ordersSeries, QStringLiteral("订单量(单)"), orderVals,
                      QColor(0xff, 0xb0, 0x4d));
        m_barAxisX->setCategories(categories);
        double maxE = 1.0, maxO = 1.0;
        for (double v : energyVals) maxE = qMax(maxE, v);
        for (double v : orderVals) maxO = qMax(maxO, v);
        m_energyAxisY->setRange(0, maxE * 1.2);
        m_ordersAxisY->setRange(0, maxO * 1.2);

        updateMetrics(payload, days, rangeSum);

        // 饼图
        while (!m_pieSeries->isEmpty()) {
            m_pieSeries->remove(m_pieSeries->slices().first());
        }
        const QJsonArray shares = payload.value(QStringLiteral("station_share")).toArray();
        double shareTotal = 0.0;
        for (const QJsonValue& sv : shares) {
            shareTotal += sv.toObject().value(QStringLiteral("value")).toDouble();
        }
        const QList<QColor> pieColors = {
            QColor(0x4f, 0x9e, 0xff), QColor(0x34, 0xc9, 0x8e),
            QColor(0xff, 0xb0, 0x4d), QColor(0xf2, 0x5f, 0x5c),
            QColor(0xb3, 0x88, 0xff)};
        for (int i = 0; i < shares.size(); ++i) {
            const QJsonObject s = shares.at(i).toObject();
            const double v = s.value(QStringLiteral("value")).toDouble();
            const double pct = shareTotal > 0.0 ? (v / shareTotal * 100.0) : 0.0;
            QPieSlice* slice = m_pieSeries->append(s.value(QStringLiteral("name")).toString(), v);
            slice->setColor(pieColors.at(i % pieColors.size()));
            slice->setLabel(QStringLiteral("%1\n%2%")
                                .arg(s.value(QStringLiteral("name")).toString())
                                .arg(pct, 0, 'f', 1));
            slice->setLabelVisible(true);
        }
    });
}

void SalesPage::updateMetrics(const QJsonObject& payload, int days, double rangeSum) {
    m_todayLabel->setText(money(payload.value(QStringLiteral("today")).toDouble()));
    m_rangeBox->setTitle(QStringLiteral("近 %1 日营收").arg(days));
    const double range = payload.value(QStringLiteral("range")).toDouble(rangeSum);
    m_rangeLabel->setText(money(range));
    m_monthLabel->setText(money(payload.value(QStringLiteral("month")).toDouble()));
    m_totalLabel->setText(money(payload.value(QStringLiteral("total")).toDouble()));
}
