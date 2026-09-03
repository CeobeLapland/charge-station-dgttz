#include "ui/pages/StationManagePage.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "network/Protocol.h"
#include "services/ApiClient.h"

StationManagePage::StationManagePage(ApiClient* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    auto* title = new QLabel(QStringLiteral("充电站管理"));
    title->setObjectName(QStringLiteral("pageTitle"));

    auto* addBtn = new QPushButton(QStringLiteral("新增电站"));
    addBtn->setObjectName(QStringLiteral("primaryButton"));
    auto* detailBtn = new QPushButton(QStringLiteral("查看站内详情"));
    auto* refreshBtn = new QPushButton(QStringLiteral("刷新"));

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(title);
    topRow->addStretch();
    topRow->addWidget(addBtn);
    topRow->addWidget(detailBtn);
    topRow->addWidget(refreshBtn);

    m_table = new QTableWidget(0, 7);
    m_table->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("站名"),
                                        QStringLiteral("地址"), QStringLiteral("经度"),
                                        QStringLiteral("纬度"), QStringLiteral("总桩数"),
                                        QStringLiteral("在线率")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addWidget(m_table);

    connect(addBtn, &QPushButton::clicked, this, &StationManagePage::onAddStation);
    connect(detailBtn, &QPushButton::clicked, this, &StationManagePage::onShowDetail);
    connect(refreshBtn, &QPushButton::clicked, this, &StationManagePage::refresh);
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, [this](int, int) { onShowDetail(); });

    refresh();
}

void StationManagePage::refresh() {
    m_api->fetchStations([this](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        const QJsonArray stations = payload.value(QStringLiteral("stations")).toArray();
        m_table->setRowCount(stations.size());
        for (int i = 0; i < stations.size(); ++i) {
            const QJsonObject s = stations.at(i).toObject();
            auto* idItem = new QTableWidgetItem(QString::number(s.value(QStringLiteral("id")).toInt()));
            idItem->setData(Qt::UserRole, s.value(QStringLiteral("id")).toInt());
            m_table->setItem(i, 0, idItem);
            m_table->setItem(i, 1, new QTableWidgetItem(s.value(QStringLiteral("name")).toString()));
            m_table->setItem(i, 2, new QTableWidgetItem(s.value(QStringLiteral("address")).toString()));
            m_table->setItem(i, 3, new QTableWidgetItem(
                QString::number(s.value(QStringLiteral("longitude")).toDouble(), 'f', 6)));
            m_table->setItem(i, 4, new QTableWidgetItem(
                QString::number(s.value(QStringLiteral("latitude")).toDouble(), 'f', 6)));
            m_table->setItem(i, 5, new QTableWidgetItem(
                QString::number(s.value(QStringLiteral("total_chargers")).toInt())));
            m_table->setItem(i, 6, new QTableWidgetItem(
                QString::number(s.value(QStringLiteral("online_rate")).toDouble(), 'f', 1)
                + QStringLiteral("%")));
        }
    });
}

void StationManagePage::onAddStation() {
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("新增电站"));
    auto* form = new QFormLayout(&dlg);

    auto* nameEdit = new QLineEdit;
    auto* addressEdit = new QLineEdit;
    auto* lngEdit = new QDoubleSpinBox;
    lngEdit->setRange(-180.0, 180.0);
    lngEdit->setDecimals(6);
    lngEdit->setValue(116.30);
    auto* latEdit = new QDoubleSpinBox;
    latEdit->setRange(-90.0, 90.0);
    latEdit->setDecimals(6);
    latEdit->setValue(39.90);
    auto* totalEdit = new QSpinBox;
    totalEdit->setRange(1, 100);
    totalEdit->setValue(10);

    form->addRow(QStringLiteral("站名"), nameEdit);
    form->addRow(QStringLiteral("地址"), addressEdit);
    form->addRow(QStringLiteral("经度"), lngEdit);
    form->addRow(QStringLiteral("纬度"), latEdit);
    form->addRow(QStringLiteral("电桩数"), totalEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    if (nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("站名不能为空"));
        return;
    }
    QJsonObject station;
    station.insert(QStringLiteral("name"), nameEdit->text().trimmed());
    station.insert(QStringLiteral("address"), addressEdit->text().trimmed());
    station.insert(QStringLiteral("longitude"), lngEdit->value());
    station.insert(QStringLiteral("latitude"), latEdit->value());
    station.insert(QStringLiteral("total_chargers"), totalEdit->value());

    m_api->addStation(station, [this](int code, const QString& message, const QJsonObject&) {
        if (code == proto::code::Ok) {
            refresh();
        } else {
            QMessageBox::warning(this, QStringLiteral("新增失败"), message);
        }
    });
}

void StationManagePage::onShowDetail() {
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选中一座充电站"));
        return;
    }
    const int stationId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    const QString name = m_table->item(row, 1)->text();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("站内详情 · %1").arg(name));
    dlg.resize(720, 420);
    auto* layout = new QVBoxLayout(&dlg);
    auto* table = new QTableWidget(0, 6);
    table->setHorizontalHeaderLabels({QStringLiteral("编号"), QStringLiteral("类型"),
                                      QStringLiteral("功率(kW)"), QStringLiteral("状态"),
                                      QStringLiteral("温度(℃)"), QStringLiteral("健康度")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(table);

    // Mock 模式下回调为同步，可立即展示；真实模式下数据稍后到达，先弹窗。
    m_api->fetchChargers(stationId,
                         [table](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        const QJsonArray chargers = payload.value(QStringLiteral("chargers")).toArray();
        table->setRowCount(chargers.size());
        for (int i = 0; i < chargers.size(); ++i) {
            const QJsonObject c = chargers.at(i).toObject();
            const QString type = c.value(QStringLiteral("type")).toString();
            table->setItem(i, 0, new QTableWidgetItem(c.value(QStringLiteral("code")).toString()));
            table->setItem(i, 1, new QTableWidgetItem(
                type == QStringLiteral("fast") ? QStringLiteral("快充") : QStringLiteral("慢充")));
            table->setItem(i, 2, new QTableWidgetItem(
                QString::number(c.value(QStringLiteral("power")).toDouble())));
            table->setItem(i, 3, new QTableWidgetItem(c.value(QStringLiteral("status")).toString()));
            table->setItem(i, 4, new QTableWidgetItem(
                QString::number(c.value(QStringLiteral("temperature")).toDouble())));
            table->setItem(i, 5, new QTableWidgetItem(
                QString::number(c.value(QStringLiteral("health_score")).toInt())));
        }
    });
    dlg.exec();
}
