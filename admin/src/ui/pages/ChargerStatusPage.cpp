#include "ui/pages/ChargerStatusPage.h"

#include <QCursor>
#include <QColor>
#include <QDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <QtCharts/QChart>
#include <QtCharts/QPieSlice>

#include "network/Protocol.h"
#include "services/ApiClient.h"

namespace {
const QList<QPair<QString, QString>>& statusMeta() {
    static const QList<QPair<QString, QString>> meta = {
        {QStringLiteral("在用"), QStringLiteral("charging")},
        {QStringLiteral("闲置"), QStringLiteral("idle")},
        {QStringLiteral("预约"), QStringLiteral("reserved")},
        {QStringLiteral("故障"), QStringLiteral("fault")},
        {QStringLiteral("离线"), QStringLiteral("offline")},
        {QStringLiteral("重启中"), QStringLiteral("rebooting")},
    };
    return meta;
}
QColor colorOf(int index) {
    static const QList<QColor> colors = {
        QColor(0x4f, 0x9e, 0xff), QColor(0x34, 0xc9, 0x8e),
        QColor(0xb3, 0x88, 0xff), QColor(0xf2, 0x5f, 0x5c),
        QColor(0x8a, 0x97, 0xa5), QColor(0xff, 0xb0, 0x4d)};
    return colors.at(index % colors.size());
}
}  // namespace

ChargerStatusPage::ChargerStatusPage(ApiClient* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    auto* title = new QLabel(QStringLiteral("电桩状态分布"));
    title->setObjectName(QStringLiteral("pageTitle"));

    m_tip = new QLabel(this, Qt::ToolTip | Qt::FramelessWindowHint);
    m_tip->setAttribute(Qt::WA_ShowWithoutActivating);
    m_tip->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_tip->setStyleSheet(QStringLiteral(
        "QLabel { color:#e8eef4; background-color:#1e2733;"
        " border:1px solid #ffffff; padding:5px; }"));
    m_tip->hide();

    // 右上：数量/占比表（双击行=看该状态桩）
    m_table = new QTableWidget(0, 3);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("状态"), QStringLiteral("数量"), QStringLiteral("占比")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setMaximumWidth(360);

    // 左上：环形饼图（点扇区=看该状态桩）
    auto* pieChart = new QChart;
    pieChart->setTitle(QStringLiteral("电桩状态饼图"));
    pieChart->setAnimationOptions(QChart::AllAnimations);
    pieChart->legend()->setVisible(true);
    pieChart->legend()->setAlignment(Qt::AlignRight);
    m_pieSeries = new QPieSeries;
    m_pieSeries->setHoleSize(0.42);
    pieChart->addSeries(m_pieSeries);
    m_pieView = new QChartView(pieChart);
    m_pieView->setRenderHint(QPainter::Antialiasing);

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(m_pieView, 1);
    topRow->addWidget(m_table);

    // 右下：故障风险 TOP（健康度最低）
    auto* riskLabel = new QLabel(QStringLiteral("故障风险 TOP（健康度最低）"));
    riskLabel->setObjectName(QStringLiteral("pageTitle"));
    m_riskTable = new QTableWidget(0, 4);
    m_riskTable->setHorizontalHeaderLabels({QStringLiteral("桩ID"), QStringLiteral("健康度"),
                                            QStringLiteral("风险等级"), QStringLiteral("建议")});
    m_riskTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_riskTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_riskTable->setMaximumHeight(150);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addLayout(topRow, 3);
    layout->addWidget(riskLabel);
    layout->addWidget(m_riskTable);

    connect(m_riskTable, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int) {
        if (row < 0 || row >= m_riskTable->rowCount()) {
            return;
        }
        emit openChargerRequested(m_riskTable->item(row, 0)->text().toInt());
    });
    connect(m_table, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int) {
        if (row < 0 || row >= m_table->rowCount()) {
            return;
        }
        showStatusChargers(m_table->item(row, 0)->data(Qt::UserRole).toString(),
                           m_table->item(row, 0)->text());
    });

    refresh();
}

void ChargerStatusPage::showTipText(const QString& text) {
    m_tip->setText(text);
    m_tip->adjustSize();
    QPoint pos = QCursor::pos() + QPoint(14, 16);
    m_tip->move(pos);
    m_tip->show();
    m_tip->raise();
}

void ChargerStatusPage::refresh() {
    m_api->fetchStationStatus([this](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        const QJsonObject dist = payload.value(QStringLiteral("distribution")).toObject();
        const int total = payload.value(QStringLiteral("total")).toInt(0);
        const QList<QPair<QString, QString>>& meta = statusMeta();

        m_table->setRowCount(meta.size());
        for (int i = 0; i < meta.size(); ++i) {
            const int n = dist.value(meta[i].second).toInt(0);
            const double pct = total > 0 ? (n * 100.0 / total) : 0.0;
            auto* keyItem = new QTableWidgetItem(meta[i].first);
            keyItem->setData(Qt::UserRole, meta[i].second);
            m_table->setItem(i, 0, keyItem);
            m_table->setItem(i, 1, new QTableWidgetItem(QString::number(n)));
            m_table->setItem(i, 2, new QTableWidgetItem(
                QString::number(pct, 'f', 1) + QStringLiteral("%")));
        }

        while (!m_pieSeries->isEmpty()) {
            m_pieSeries->remove(m_pieSeries->slices().first());
        }
        int shown = 0;
        for (int i = 0; i < meta.size(); ++i) {
            const int n = dist.value(meta[i].second).toInt(0);
            if (n <= 0) {
                continue;
            }
            ++shown;
            const QString label = meta[i].first;
            const QString key = meta[i].second;
            const double pct = total > 0 ? (n * 100.0 / total) : 0.0;
            QPieSlice* slice = m_pieSeries->append(label, n);
            slice->setColor(colorOf(i));
            slice->setBorderColor(QColor(0xff, 0xff, 0xff));
            slice->setLabel(QStringLiteral("%1 %2 (%3%)")
                                .arg(label).arg(n).arg(pct, 0, 'f', 1));
            slice->setLabelColor(QColor(0x10, 0x16, 0x1d));
            slice->setLabelVisible(true);
            connect(slice, &QPieSlice::hovered, this,
                    [this, slice, label, n, pct](bool state) {
                slice->setExploded(state);
                if (state) {
                    showTipText(QStringLiteral("%1\n数量：%2\n占比：%3%")
                                    .arg(label).arg(n).arg(pct, 0, 'f', 1));
                } else {
                    m_tip->hide();
                }
            });
            connect(slice, &QPieSlice::clicked, this,
                    [this, key, label]() { showStatusChargers(key, label); });
        }
        if (shown == 0) {
            m_pieSeries->append(QStringLiteral("暂无数据"), 1);
            m_pieSeries->slices().first()->setColor(QColor(0x2b, 0x39, 0x45));
            m_pieSeries->slices().first()->setLabelVisible(false);
        }
    });

    // 故障风险 TOP
    m_api->fetchHealthRanks([this](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        const QJsonArray risks = payload.value(QStringLiteral("risks")).toArray();
        m_riskTable->setRowCount(risks.size());
        for (int i = 0; i < risks.size(); ++i) {
            const QJsonObject r = risks.at(i).toObject();
            m_riskTable->setItem(i, 0, new QTableWidgetItem(
                QString::number(r.value(QStringLiteral("charger_id")).toInt())));
            m_riskTable->setItem(i, 1, new QTableWidgetItem(
                QString::number(r.value(QStringLiteral("health_score")).toInt())));
            m_riskTable->setItem(i, 2, new QTableWidgetItem(
                r.value(QStringLiteral("risk_level")).toString()));
            m_riskTable->setItem(i, 3, new QTableWidgetItem(
                r.value(QStringLiteral("suggestion")).toString()));
        }
    });
}

void ChargerStatusPage::showStatusChargers(const QString& statusKey, const QString& statusLabel) {
    m_api->fetchChargers(0, [this, statusKey, statusLabel]
                         (int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        QJsonArray matched;
        const QJsonArray chargers = payload.value(QStringLiteral("chargers")).toArray();
        for (const QJsonValue& cv : chargers) {
            if (cv.toObject().value(QStringLiteral("status")).toString() == statusKey) {
                matched.append(cv.toObject());
            }
        }
        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("%1状态的充电桩（%2 台）")
                               .arg(statusLabel).arg(matched.size()));
        auto* tbl = new QTableWidget(matched.size(), 4);
        tbl->setHorizontalHeaderLabels({QStringLiteral("桩ID"), QStringLiteral("编号"),
                                        QStringLiteral("所属电站"), QStringLiteral("功率(kW)")});
        tbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
        for (int i = 0; i < matched.size(); ++i) {
            const QJsonObject c = matched.at(i).toObject();
            tbl->setItem(i, 0, new QTableWidgetItem(
                QString::number(c.value(QStringLiteral("id")).toInt())));
            tbl->setItem(i, 1, new QTableWidgetItem(c.value(QStringLiteral("code")).toString()));
            tbl->setItem(i, 2, new QTableWidgetItem(
                c.value(QStringLiteral("station_name")).toString()));
            tbl->setItem(i, 3, new QTableWidgetItem(
                QString::number(c.value(QStringLiteral("power")).toDouble())));
        }
        int jumpId = 0;
        connect(tbl, &QTableWidget::cellDoubleClicked, &dlg,
                [&dlg, tbl, &jumpId](int r, int) {
            if (r < 0 || r >= tbl->rowCount()) {
                return;
            }
            jumpId = tbl->item(r, 0)->text().toInt();
            dlg.accept();
        });
        auto* closeBtn = new QPushButton(QStringLiteral("关闭"));
        auto* btnRow = new QHBoxLayout;
        btnRow->addStretch();
        btnRow->addWidget(closeBtn);
        auto* lay = new QVBoxLayout(&dlg);
        lay->addWidget(tbl);
        lay->addLayout(btnRow);
        connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
        dlg.resize(560, 380);
        dlg.exec();
        if (jumpId > 0) {
            emit openChargerRequested(jumpId);
        }
    });
}