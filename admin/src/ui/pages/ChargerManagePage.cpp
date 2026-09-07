#include "ui/pages/ChargerManagePage.h"

#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
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
    auto* pauseBtn = new QPushButton(QStringLiteral("暂停使用选中桩"));
    pauseBtn->setObjectName(QStringLiteral("dangerButton"));
    auto* addBtn = new QPushButton(QStringLiteral("新增充电桩"));
    auto* refreshBtn = new QPushButton(QStringLiteral("刷新"));

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(title);
    topRow->addStretch();
    topRow->addWidget(addBtn);
    topRow->addWidget(restartBtn);
    topRow->addWidget(pauseBtn);
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
    connect(pauseBtn, &QPushButton::clicked, this, &ChargerManagePage::onPause);
    connect(addBtn, &QPushButton::clicked, this, &ChargerManagePage::onAddCharger);
    connect(refreshBtn, &QPushButton::clicked, this, &ChargerManagePage::refresh);

    refresh();
}

void ChargerManagePage::appendLog(const QString& text) {
    m_logView->append(QStringLiteral("[%1] %2")
                          .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), text));
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
            m_table->setItem(i, 2,
                             new QTableWidgetItem(c.value(QStringLiteral("station_name")).toString()));
            const QString type = c.value(QStringLiteral("type")).toString();
            m_table->setItem(i, 3, new QTableWidgetItem(
                type == QStringLiteral("fast") ? QStringLiteral("快充") : QStringLiteral("慢充")));
            m_table->setItem(i, 4, new QTableWidgetItem(
                QString::number(c.value(QStringLiteral("power")).toDouble())));
            m_table->setItem(i, 5,
                             new QTableWidgetItem(c.value(QStringLiteral("status")).toString()));
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
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中一台充电桩"));
        return;
    }
    const int chargerId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    const QString code = m_table->item(row, 1)->text();

    m_table->item(row, 5)->setText(QStringLiteral("rebooting"));
    appendLog(QStringLiteral("向 %1 发送重启指令…").arg(code));

    m_api->restartCharger(chargerId, [this, row, code](int result, const QString& message,
                                                       const QJsonObject& payload) {
        if (result != proto::code::Ok) {
            appendLog(QStringLiteral("重启失败：%1").arg(message));
            return;
        }
        const QJsonObject log = payload.value(QStringLiteral("device_log")).toObject();
        appendLog(QStringLiteral("设备重启成功，result=%1（已写入 device_log）")
                      .arg(log.value(QStringLiteral("result")).toString()));

        QTimer::singleShot(2000, this, [this, row]() {
            m_table->item(row, 5)->setText(QStringLiteral("idle"));
        });
        Q_UNUSED(code);
    });
}

void ChargerManagePage::onPause() {
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中一台充电桩"));
        return;
    }
    const int chargerId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    const QString code = m_table->item(row, 1)->text();

    QMessageBox box(QMessageBox::Question, QStringLiteral("确认暂停"),
                    QStringLiteral("确定要暂停使用充电桩 %1 吗？\n暂停后该桩将转为 offline（不可用）。").arg(code),
                    QMessageBox::NoButton, this);
    QPushButton* yesBtn = box.addButton(QStringLiteral("确定暂停"), QMessageBox::AcceptRole);
    QPushButton* noBtn = box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != yesBtn) {
        return;
    }
    Q_UNUSED(noBtn);

    m_table->item(row, 5)->setText(QStringLiteral("offline"));
    appendLog(QStringLiteral("暂停 %1 …").arg(code));

    m_api->pauseCharger(chargerId, [this, row, code](int result, const QString& message,
                                                     const QJsonObject& payload) {
        if (result != proto::code::Ok) {
            appendLog(QStringLiteral("暂停失败：%1").arg(message));
            return;
        }
        const QJsonObject log = payload.value(QStringLiteral("device_log")).toObject();
        appendLog(QStringLiteral("已暂停，result=%1（已写入 device_log）")
                      .arg(log.value(QStringLiteral("result")).toString()));
        Q_UNUSED(code);
    });
}

void ChargerManagePage::onAddCharger() {
    // 先取电站列表，再弹新增对话框
    m_api->fetchStations([this](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        const QJsonArray stations = payload.value(QStringLiteral("stations")).toArray();
        if (stations.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("还没有电站，请先新增电站"));
            return;
        }
        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("新增充电桩"));
        auto* form = new QFormLayout(&dlg);

        auto* stationCombo = new QComboBox;
        for (const QJsonValue& sv : stations) {
            const QJsonObject s = sv.toObject();
            stationCombo->addItem(QStringLiteral("%1（#%2）")
                                      .arg(s.value(QStringLiteral("name")).toString())
                                      .arg(s.value(QStringLiteral("id")).toInt()),
                                  s.value(QStringLiteral("id")).toInt());
        }
        auto* typeCombo = new QComboBox;
        typeCombo->addItem(QStringLiteral("快充"), QStringLiteral("fast"));
        typeCombo->addItem(QStringLiteral("慢充"), QStringLiteral("slow"));
        auto* powerEdit = new QDoubleSpinBox;
        powerEdit->setRange(7.0, 600.0);
        powerEdit->setValue(60.0);
        powerEdit->setDecimals(1);

        form->addRow(QStringLiteral("所属电站"), stationCombo);
        form->addRow(QStringLiteral("类型"), typeCombo);
        form->addRow(QStringLiteral("功率(kW)"), powerEdit);

        auto* okBtn = new QPushButton(QStringLiteral("新增"));
        auto* cancelBtn = new QPushButton(QStringLiteral("取消"));
        auto* btnRow = new QHBoxLayout;
        btnRow->addStretch();
        btnRow->addWidget(cancelBtn);
        btnRow->addWidget(okBtn);
        form->addRow(btnRow);
        connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
        connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        QJsonObject addPayload;
        addPayload.insert(QStringLiteral("station_id"), stationCombo->currentData().toInt());
        addPayload.insert(QStringLiteral("type"), typeCombo->currentData().toString());
        addPayload.insert(QStringLiteral("power"), powerEdit->value());
        appendLog(QStringLiteral("请求新增充电桩：电站 #%1 / %2 / %3kW")
                      .arg(stationCombo->currentData().toInt())
                      .arg(typeCombo->currentText())
                      .arg(powerEdit->value(), 0, 'f', 1));
        m_api->addCharger(addPayload,
                          [this](int result, const QString& message, const QJsonObject&) {
            if (result != proto::code::Ok) {
                appendLog(QStringLiteral("新增失败：%1").arg(message));
                return;
            }
            appendLog(QStringLiteral("新增充电桩成功（Mock）；真服务端需 server 支持 admin.charger_add）"));
            refresh();
        });
    });
}