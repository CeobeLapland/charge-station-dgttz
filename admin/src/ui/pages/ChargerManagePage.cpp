#include "ui/pages/ChargerManagePage.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include "network/Protocol.h"
#include "services/ApiClient.h"

ChargerManagePage::ChargerManagePage(ApiClient* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    auto* title = new QLabel(QStringLiteral("充电桩管理"));
    title->setObjectName(QStringLiteral("pageTitle"));

    auto* restartBtn = new QPushButton(QStringLiteral("远程重启选中桩"));
    restartBtn->setObjectName(QStringLiteral("primaryButton"));
    auto* refreshBtn = new QPushButton(QStringLiteral("刷新"));

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(title);
    topRow->addStretch();
    topRow->addWidget(restartBtn);
    topRow->addWidget(refreshBtn);

    m_table = new QTableWidget(0, 7);
    m_table->setHorizontalHeaderLabels({QStringLiteral("桩ID"), QStringLiteral("编号"),
                                        QStringLiteral("所属电站"), QStringLiteral("类型"),
                                        QStringLiteral("功率(kW)"), QStringLiteral("状态"),
                                        QStringLiteral("累计次数/时长")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_logView = new QTextEdit;
    m_logView->setReadOnly(true);
    m_logView->setFixedHeight(120);
    m_logView->setPlaceholderText(QStringLiteral("设备操作日志（device_log）"));

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addWidget(m_table, 1);
    layout->addWidget(new QLabel(QStringLiteral("设备操作日志")));
    layout->addWidget(m_logView);

    connect(restartBtn, &QPushButton::clicked, this, &ChargerManagePage::onRestart);
    connect(refreshBtn, &QPushButton::clicked, this, &ChargerManagePage::refresh);

    refresh();
}

void ChargerManagePage::refresh() {
    m_api->fetchChargers(0, [this](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        const QJsonArray chargers = payload.value(QStringLiteral("chargers")).toArray();
        m_table->setRowCount(chargers.size());
        for (int i = 0; i < chargers.size(); ++i) {
            const QJsonObject c = chargers.at(i).toObject();
            auto* idItem = new QTableWidgetItem(QString::number(c.value(QStringLiteral("id")).toInt()));
            idItem->setData(Qt::UserRole, c.value(QStringLiteral("id")).toInt());
            m_table->setItem(i, 0, idItem);
            m_table->setItem(i, 1, new QTableWidgetItem(c.value(QStringLiteral("code")).toString()));
            m_table->setItem(i, 2, new QTableWidgetItem(c.value(QStringLiteral("station_name")).toString()));
            const QString type = c.value(QStringLiteral("type")).toString();
            m_table->setItem(i, 3, new QTableWidgetItem(
                type == QStringLiteral("fast") ? QStringLiteral("快充") : QStringLiteral("慢充")));
            m_table->setItem(i, 4, new QTableWidgetItem(
                QString::number(c.value(QStringLiteral("power")).toDouble())));
            m_table->setItem(i, 5, new QTableWidgetItem(c.value(QStringLiteral("status")).toString()));
            m_table->setItem(i, 6, new QTableWidgetItem(
                QStringLiteral("%1 次 / %2 分")
                    .arg(c.value(QStringLiteral("total_charge_count")).toInt())
                    .arg(c.value(QStringLiteral("total_charge_duration")).toInt())));
        }
    });
}

void ChargerManagePage::onRestart() {
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选中一台充电桩"));
        return;
    }
    const int chargerId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    const QString code = m_table->item(row, 1)->text();

    m_table->item(row, 5)->setText(QStringLiteral("rebooting"));
    m_logView->append(QStringLiteral("[%1] 向 %2 发送重启指令…").arg(
        QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), code));

    m_api->restartCharger(chargerId, [this, row, code](int result, const QString& message,
                                                       const QJsonObject& payload) {
        if (result != proto::code::Ok) {
            m_logView->append(QStringLiteral("重启失败：%1").arg(message));
            return;
        }
        const QJsonObject log = payload.value(QStringLiteral("device_log")).toObject();
        m_logView->append(QStringLiteral("[%1] 设备重启成功，result=%2（已写入 device_log）")
                              .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                                   log.value(QStringLiteral("result")).toString()));

        // 模拟：2 秒后桩恢复为 idle
        QTimer::singleShot(2000, this, [this, row]() {
            m_table->item(row, 5)->setText(QStringLiteral("idle"));
        });
        Q_UNUSED(code);
    });
}
