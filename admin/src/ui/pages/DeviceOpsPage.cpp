#include "ui/pages/DeviceOpsPage.h"

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

DeviceOpsPage::DeviceOpsPage(ApiClient* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    auto* title = new QLabel(QStringLiteral("设备运维"));
    title->setObjectName(QStringLiteral("pageTitle"));

    auto* refreshBtn = new QPushButton(QStringLiteral("刷新"));
    refreshBtn->setObjectName(QStringLiteral("primaryButton"));
    auto* restartBtn = new QPushButton(QStringLiteral("远程重启选中桩"));
    auto* logBtn = new QPushButton(QStringLiteral("查看操作日志"));

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(title);
    topRow->addStretch();
    topRow->addWidget(restartBtn);
    topRow->addWidget(logBtn);
    topRow->addWidget(refreshBtn);

    m_table = new QTableWidget(0, 10);
    m_table->setHorizontalHeaderLabels({QStringLiteral("桩ID"), QStringLiteral("编号"),
                                        QStringLiteral("所属电站"), QStringLiteral("状态"),
                                        QStringLiteral("电压(V)"), QStringLiteral("电流(A)"),
                                        QStringLiteral("温度(℃)"), QStringLiteral("通信"),
                                        QStringLiteral("健康度"), QStringLiteral("故障码")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_riskTable = new QTableWidget(0, 4);
    m_riskTable->setHorizontalHeaderLabels({QStringLiteral("桩ID"), QStringLiteral("健康度(0-100)"),
                                            QStringLiteral("风险等级"), QStringLiteral("建议操作")});
    m_riskTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_riskTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_riskTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_riskTable->setFixedHeight(150);

    m_logView = new QTextEdit;
    m_logView->setReadOnly(true);
    m_logView->setFixedHeight(110);
    m_logView->setPlaceholderText(QStringLiteral("设备操作日志（device_log）"));

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addWidget(new QLabel(QStringLiteral(
        "设备实时参数（对齐 server/sql/schema.sql 的 charger 表）")));
    layout->addWidget(m_table, 1);
    layout->addWidget(new QLabel(QStringLiteral("故障风险 TOP（健康度最低）")));
    layout->addWidget(m_riskTable);
    layout->addWidget(new QLabel(QStringLiteral("设备操作日志")));
    layout->addWidget(m_logView);

    connect(refreshBtn, &QPushButton::clicked, this, &DeviceOpsPage::refresh);
    connect(restartBtn, &QPushButton::clicked, this, &DeviceOpsPage::onRestart);
    connect(logBtn, &QPushButton::clicked, this, &DeviceOpsPage::onShowLogs);

    refresh();
}

void DeviceOpsPage::refresh() {
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
            m_table->setItem(i, 2,
                             new QTableWidgetItem(c.value(QStringLiteral("station_name")).toString()));
            m_table->setItem(i, 3,
                             new QTableWidgetItem(c.value(QStringLiteral("status")).toString()));
            m_table->setItem(i, 4, new QTableWidgetItem(
                QString::number(c.value(QStringLiteral("voltage")).toDouble(), 'f', 1)));
            m_table->setItem(i, 5, new QTableWidgetItem(
                QString::number(c.value(QStringLiteral("current")).toDouble(), 'f', 1)));
            m_table->setItem(i, 6, new QTableWidgetItem(
                QString::number(c.value(QStringLiteral("temperature")).toDouble(), 'f', 1)));
            m_table->setItem(i, 7,
                             new QTableWidgetItem(c.value(QStringLiteral("comm_status")).toString()));
            m_table->setItem(i, 8, new QTableWidgetItem(
                QString::number(c.value(QStringLiteral("health_score")).toInt())));
            const QString faultCode = c.value(QStringLiteral("fault_code")).toString();
            m_table->setItem(i, 9, new QTableWidgetItem(
                faultCode.isEmpty() ? QStringLiteral("-") : faultCode));
        }
    });

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
            m_riskTable->setItem(i, 2,
                                 new QTableWidgetItem(r.value(QStringLiteral("risk_level")).toString()));
            m_riskTable->setItem(i, 3,
                                 new QTableWidgetItem(r.value(QStringLiteral("suggestion")).toString()));
        }
    });
}

void DeviceOpsPage::onRestart() {
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选中一台充电桩"));
        return;
    }
    const int chargerId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    const QString code = m_table->item(row, 1)->text();
    m_table->item(row, 3)->setText(QStringLiteral("rebooting"));
    appendLog(QStringLiteral("向 %1 发送重启指令…").arg(code));

    m_api->restartCharger(chargerId, [this, row](int result, const QString& message,
                                                 const QJsonObject& payload) {
        if (result != proto::code::Ok) {
            appendLog(QStringLiteral("重启失败：%1").arg(message));
            return;
        }
        const QJsonObject log = payload.value(QStringLiteral("device_log")).toObject();
        appendLog(QStringLiteral("设备重启成功，result=%1（已写入 device_log）")
                      .arg(log.value(QStringLiteral("result")).toString()));
        // 模拟：2 秒后桩恢复为 idle（真实模式由服务端 push.charger_status 驱动）
        QTimer::singleShot(2000, this, [this, row]() {
            m_table->item(row, 3)->setText(QStringLiteral("idle"));
        });
    });
}

void DeviceOpsPage::onShowLogs() {
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选中一台充电桩"));
        return;
    }
    const int chargerId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    const QString code = m_table->item(row, 1)->text();
    appendLog(QStringLiteral("查看 %1 操作日志：").arg(code));

    m_api->fetchDeviceLogs(chargerId,
                           [this](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        const QJsonArray logs = payload.value(QStringLiteral("logs")).toArray();
        if (logs.isEmpty()) {
            appendLog(QStringLiteral("（无操作日志）"));
        }
        for (const QJsonValue& lv : logs) {
            const QJsonObject log = lv.toObject();
            appendLog(QStringLiteral("%1 | %2 | %3 | %4")
                          .arg(log.value(QStringLiteral("action")).toString(),
                               log.value(QStringLiteral("op_time")).toString(),
                               log.value(QStringLiteral("operator")).toString(),
                               log.value(QStringLiteral("result")).toString()));
        }
    });
}

void DeviceOpsPage::appendLog(const QString& text) {
    m_logView->append(QStringLiteral("[%1] %2")
                          .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), text));
}