#include "ui/pages/SalesPage.h"

#include <QCursor>
#include <QColor>
#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPoint>
#include <QScreen>
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
void styleCategoryAxis(QBarCategoryAxis* axis) {
    axis->setLabelsAngle(-60);
    QFont f = axis->labelsFont();
    f.setPointSize(7);
    axis->setLabelsFont(f);
}
void dropBarSets(QBarSeries* series) {
    while (!series->barSets().isEmpty()) {
        QBarSet* s = series->barSets().first();
        series->take(s);
        delete s;
    }
}
QBarSet* addBarSet(QBarSeries* series, const QString& label, const QList<double>& values,
                   const QColor& color) {
    auto* set = new QBarSet(label);
    set->setColor(color);
    for (double v : values) {
        *set << v;
    }
    series->append(set);
    return set;
}
}  // namespace

SalesPage::SalesPage(ApiClient* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    auto* title = new QLabel(QStringLiteral("销售业绩"));
    title->setObjectName(QStringLiteral("pageTitle"));

    // 自绘气泡：鼠标穿透 + 无边框置顶，显示/隐藏完全由代码控制
    m_tip = new QLabel(this, Qt::ToolTip | Qt::FramelessWindowHint);
    m_tip->setAttribute(Qt::WA_ShowWithoutActivating);
    m_tip->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_tip->setStyleSheet(QStringLiteral(
        "QLabel { color:#e8eef4; background-color:#1e2733;"
        " border:1px solid #ffffff; padding:5px; }"));
    m_tip->hide();

    m_rangeCombo = new QComboBox;
    m_rangeCombo->addItem(QStringLiteral("近 7 日"), 7);
    m_rangeCombo->addItem(QStringLiteral("近 30 日"), 30);

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

    auto* lineChart = new QChart;
    lineChart->setTitle(QStringLiteral("营收趋势"));
    lineChart->setAnimationOptions(QChart::AllAnimations);
    lineChart->legend()->hide();
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

    connect(m_lineSeries, &QLineSeries::hovered, this, [this](const QPointF& pt, bool state) {
        if (!state) {
            return;   // 悬停期间不隐藏气泡
        }
        const int idx = qRound(pt.x());
        if (idx < 0 || idx >= m_dayAmounts.size() || qAbs(pt.x() - idx) > 0.5) {
            return;
        }
        showTipText(QStringLiteral("%1\n营收：%2")
                        .arg(m_dayDates.at(idx), money(m_dayAmounts.at(idx))));
    });
    connect(m_scatterSeries, &QScatterSeries::hovered, this, [this](const QPointF& pt, bool state) {
        if (!state) {
            return;
        }
        const int idx = qRound(pt.x());
        if (idx < 0 || idx >= m_dayAmounts.size() || qAbs(pt.x() - idx) > 0.5) {
            return;
        }
        showTipText(QStringLiteral("%1\n营收：%2")
                        .arg(m_dayDates.at(idx), money(m_dayAmounts.at(idx))));
    });

    auto* barChart = new QChart;
    barChart->setTitle(QStringLiteral("每日充电量与订单量"));
    barChart->setAnimationOptions(QChart::AllAnimations);
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

    auto* pieChart = new QChart;
    pieChart->setTitle(QStringLiteral("站点营收占比"));
    pieChart->setAnimationOptions(QChart::AllAnimations);
    pieChart->legend()->hide();
    m_pieSeries = new QPieSeries;
    pieChart->addSeries(m_pieSeries);
    m_pieView = new QChartView(pieChart);
    m_pieView->setRenderHint(QPainter::Antialiasing);

    // 鼠标真正离开图表才隐藏气泡
    m_lineChartView->installEventFilter(this);
    m_barView->installEventFilter(this);
    m_pieView->installEventFilter(this);

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
    m_api->fetchOrderDailyStats([this, days](int code, const QString&, const QJsonObject& stats) {
        m_ordersByDate.clear();
        m_energyByDate.clear();
        if (code == proto::code::Ok) {
            const QJsonArray daysArr = stats.value(QStringLiteral("days")).toArray();
            for (const QJsonValue& v : daysArr) {
                const QJsonObject d = v.toObject();
                m_ordersByDate.insert(d.value(QStringLiteral("date")).toString(),
                                      d.value(QStringLiteral("order_count")).toInt());
                m_energyByDate.insert(d.value(QStringLiteral("date")).toString(),
                                      d.value(QStringLiteral("energy_kwh")).toDouble());
            }
        }
        m_api->fetchStationRevenueShare([this, days](int c2, const QString&, const QJsonObject& share) {
            m_stationRows.clear();
            if (c2 == proto::code::Ok) {
                const QJsonArray st = share.value(QStringLiteral("stations")).toArray();
                for (const QJsonValue& v : st) {
                    const QJsonObject s = v.toObject();
                    m_stationRows.append({s.value(QStringLiteral("station_name")).toString(),
                                          s.value(QStringLiteral("revenue")).toDouble()});
                }
            }
            m_api->fetchRevenue(days,
                        [this, days](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        const QJsonArray trend = payload.value(QStringLiteral("trend")).toArray();

        m_lastEnergyIdx = -1;
        m_lastOrdersIdx = -1;
        m_tip->hide();

        QStringList categories, fullDates;
        QList<double> amounts, energyVals, orderVals;
        for (int i = 0; i < trend.size(); ++i) {
            const QJsonObject item = trend.at(i).toObject();
            const QString date = item.value(QStringLiteral("date")).toString();
            fullDates << date;
            categories << (date.size() >= 10 ? date.mid(5) : date);
            amounts << item.value(QStringLiteral("amount")).toDouble();
            energyVals << (m_energyByDate.contains(date)
                                     ? m_energyByDate.value(date)
                                     : item.value(QStringLiteral("energy")).toDouble());
            orderVals << (m_ordersByDate.contains(date)
                                    ? m_ordersByDate.value(date)
                                    : item.value(QStringLiteral("orders")).toDouble());
        }
        m_dayDates = fullDates;
        m_dayAmounts = amounts;
        m_dayEnergies = energyVals;
        m_dayOrders = orderVals;

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

        dropBarSets(m_energySeries);
        dropBarSets(m_ordersSeries);
        m_energySet = addBarSet(m_energySeries, QStringLiteral("充电量(kWh)"),
                                energyVals, QColor(0x4f, 0x9e, 0xff));
        m_ordersSet = addBarSet(m_ordersSeries, QStringLiteral("订单量(单)"),
                                orderVals, QColor(0xff, 0xb0, 0x4d));
        m_barAxisX->setCategories(categories);
        double maxE = 1.0, maxO = 1.0;
        for (double v : energyVals) maxE = qMax(maxE, v);
        for (double v : orderVals) maxO = qMax(maxO, v);
        m_energyAxisY->setRange(0, maxE * 1.2);
        m_ordersAxisY->setRange(0, maxO * 1.2);

        auto bindBarHover = [this](QBarSet* set, bool isEnergy, int& lastIdx) {
            connect(set, &QBarSet::hovered, this,
                    [this, set, isEnergy, &lastIdx](bool state, int index) {
                const QList<double>& vals = isEnergy ? m_dayEnergies : m_dayOrders;
                if (!state) {
                    if (lastIdx >= 0 && lastIdx < set->count()) {
                        set->replace(lastIdx, vals.at(lastIdx));
                    }
                    lastIdx = -1;
                    return;
                }
                if (index < 0 || index >= m_dayDates.size()) {
                    return;
                }
                if (lastIdx >= 0 && lastIdx < set->count()) {
                    set->replace(lastIdx, vals.at(lastIdx));
                }
                set->replace(index, vals.at(index) * 1.20);
                lastIdx = index;
                const QString text =
                    isEnergy
                        ? QStringLiteral("%1\n充电量：%2 kWh")
                              .arg(m_dayDates.at(index), QString::number(vals.at(index), 'f', 1))
                        : QStringLiteral("%1\n订单量：%2 单")
                              .arg(m_dayDates.at(index), QString::number(vals.at(index), 'f', 0));
                showTipText(text);
            });
        };
        bindBarHover(m_energySet, true, m_lastEnergyIdx);
        bindBarHover(m_ordersSet, false, m_lastOrdersIdx);

        updateMetrics(payload, days, rangeSum);

        while (!m_pieSeries->isEmpty()) {
            m_pieSeries->remove(m_pieSeries->slices().first());
        }
        QJsonArray shares;
        for (const auto& row : m_stationRows) {
            QJsonObject o;
            o.insert(QStringLiteral("name"), row.first);
            o.insert(QStringLiteral("value"), row.second);
            shares.append(o);
        }
        if (shares.isEmpty()) {
            shares = payload.value(QStringLiteral("station_share")).toArray();
        }
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
            const QString name = s.value(QStringLiteral("name")).toString();
            const double v = s.value(QStringLiteral("value")).toDouble();
            const double pct = shareTotal > 0.0 ? (v / shareTotal * 100.0) : 0.0;
            QPieSlice* slice = m_pieSeries->append(name, v);
            slice->setColor(pieColors.at(i % pieColors.size()));
            slice->setBorderColor(QColor(0xff, 0xff, 0xff));
            slice->setLabel(QStringLiteral("%1\n%2%").arg(name).arg(pct, 0, 'f', 1));
            slice->setLabelVisible(true);
            connect(slice, &QPieSlice::hovered, this,
                    [this, slice, name, v, pct](bool state) {
                slice->setExploded(state);
                if (state) {
                    slice->setLabelVisible(true);
                    showTipText(QStringLiteral("%1\n营收：%2\n占比：%3%")
                                    .arg(name, money(v)).arg(pct, 0, 'f', 1));
                } else {
                    slice->setExploded(false);
                }
            });
        }
    });
        });
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

void SalesPage::showTipText(const QString& text) {
    m_tip->setText(text);
    m_tip->adjustSize();
    QPoint pos = QCursor::pos() + QPoint(14, 16);
    if (QScreen* screen = QGuiApplication::screenAt(QCursor::pos())) {
        const QRect g = screen->availableGeometry();
        if (pos.x() + m_tip->width() > g.right()) pos.setX(g.right() - m_tip->width());
        if (pos.y() + m_tip->height() > g.bottom()) pos.setY(g.bottom() - m_tip->height());
        if (pos.x() < g.left()) pos.setX(g.left());
        if (pos.y() < g.top()) pos.setY(g.top());
    }
    m_tip->move(pos);
    m_tip->show();
    m_tip->raise();
}

void SalesPage::hideTip() {
    m_tip->hide();
}

bool SalesPage::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Leave &&
        (watched == m_lineChartView || watched == m_barView || watched == m_pieView)) {
        if (auto* view = qobject_cast<QWidget*>(watched)) {
            const QRect globalRect(view->mapToGlobal(QPoint(0, 0)), view->size());
            if (!globalRect.contains(QCursor::pos())) {   // 鼠标真在图表外才隐藏
                m_tip->hide();
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
