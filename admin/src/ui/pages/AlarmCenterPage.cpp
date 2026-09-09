#include "ui/pages/AlarmCenterPage.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QTableWidget>
#include <QVBoxLayout>

#include "network/Protocol.h"
#include "services/ApiClient.h"

namespace {
QString nowStr() {
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}
}  // namespace

AlarmCenterPage::AlarmCenterPage(ApiClient* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    auto* title = new QLabel(QStringLiteral("运营告警中心"));
    title->setObjectName(QStringLiteral("pageTitle"));

    m_summaryLabel = new QLabel;
    m_summaryLabel->setStyleSheet(QStringLiteral("color:#8a97a5;"));

    auto* handleBtn = new QPushButton(QStringLiteral("处理选中告警"));
    handleBtn->setObjectName(QStringLiteral("primaryButton"));
    auto* refreshBtn = new QPushButton(QStringLiteral("刷新"));

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(title);
    topRow->addStretch();
    topRow->addWidget(refreshBtn);
    topRow->addWidget(handleBtn);

    m_table = new QTableWidget(0, 6);
    m_table->setHorizontalHeaderLabels({QStringLiteral("级别"), QStringLiteral("类型"),
                                        QStringLiteral("对象"), QStringLiteral("详情"),
                                        QStringLiteral("状态"), QStringLiteral("发生时间")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addWidget(m_summaryLabel);
    layout->addWidget(m_table, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &AlarmCenterPage::refresh);
    connect(handleBtn, &QPushButton::clicked, this, &AlarmCenterPage::onHandle);
    // 每 5 秒自动刷新，配合实时运营
    auto* timer = new QTimer(this);
    timer->setInterval(5000);
    connect(timer, &QTimer::timeout, this, &AlarmCenterPage::refresh);
    timer->start();

    refresh();
}

void AlarmCenterPage::refresh() {
    // 由现有接口推导：先按电站，再按桩
    m_alarms.clear();
    m_api->fetchStations([this](int code, const QString&, const QJsonObject& payload) {
        if (code == proto::code::Ok) {
            const QJsonArray stations = payload.value(QStringLiteral("stations")).toArray();
            for (const QJsonValue& sv : stations) {
                const QJsonObject s = sv.toObject();
                const int sid = s.value(QStringLiteral("id")).toInt();
                const QString name = s.value(QStringLiteral("name")).toString();
                const double online = s.value(QStringLiteral("online_rate")).toDouble(100.0);
                if (online < 80.0) {
                    AlarmRow r;
                    r.level = online < 60.0 ? QStringLiteral("高") : QStringLiteral("中");
                    r.type = QStringLiteral("站在线率低");
                    r.object = name;
                    r.detail = QStringLiteral("在线率 %1%").arg(online, 0, 'f', 1);
                    r.stationId = sid;
                    m_alarms.append(r);
                }
            }
            renderAlarms();
        }
    });
    m_api->fetchChargers(0, [this](int code, const QString&, const QJsonObject& payload) {
        if (code == proto::code::Ok) {
            const QJsonArray chargers = payload.value(QStringLiteral("chargers")).toArray();
            for (const QJsonValue& cv : chargers) {
                const QJsonObject c = cv.toObject();
                const QString st = c.value(QStringLiteral("status")).toString();
                const QString code2 = c.value(QStringLiteral("code")).toString();
                const QString stName = c.value(QStringLiteral("station_name")).toString();
                AlarmRow r;
                r.stationId = c.value(QStringLiteral("station_id")).toInt();
                r.chargerId = c.value(QStringLiteral("id")).toInt();
                r.object = QStringLiteral("%1 / %2").arg(stName).arg(code2);
                if (st == QStringLiteral("fault")) {
                    r.level = QStringLiteral("高");
                    r.type = QStringLiteral("设备故障");
                    r.detail = QStringLiteral("故障码：%1")
                                    .arg(c.value(QStringLiteral("fault_code")).toString());
                    m_alarms.append(r);
                } else if (st == QStringLiteral("offline")) {
                    r.level = QStringLiteral("中");
                    r.type = QStringLiteral("设备离线");
                    r.detail = QStringLiteral("通信中断，无法上报");
                    m_alarms.append(r);
                }
            }
            renderAlarms();
        }
    });
}

void AlarmCenterPage::renderAlarms() {
    m_table->setRowCount(m_alarms.size());
    for (int i = 0; i < m_alarms.size(); ++i) {
        const AlarmRow& r = m_alarms.at(i);
        auto* levelItem = new QTableWidgetItem(r.level);
        levelItem->setForeground(r.level == QStringLiteral("高")
                                     ? QColor(0xf2, 0x5f, 0x5c)
                                     : QColor(0xff, 0xb0, 0x4d));
        levelItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 0, levelItem);
        m_table->setItem(i, 1, new QTableWidgetItem(r.type));
        m_table->setItem(i, 2, new QTableWidgetItem(r.object));
        m_table->setItem(i, 3, new QTableWidgetItem(r.detail));
        m_table->setItem(i, 4, new QTableWidgetItem(QStringLiteral("待处理")));
        m_table->setItem(i, 5, new QTableWidgetItem(nowStr()));
    }
    int high = 0, medium = 0;
    for (const AlarmRow& r : m_alarms) {
        if (r.level == QStringLiteral("高")) {
            ++high;
        } else {
            ++medium;
        }
    }
    if (m_alarms.isEmpty()) {
        m_summaryLabel->setText(QStringLiteral("<span style='color:#34c98e; font-size:14px;'>当前无异常，一切正常 ✔</span>"));
    } else {
        m_summaryLabel->setText(
            QStringLiteral("<span style='color:#f25f5c; font-size:16px; font-weight:bold;'>高 %1</span>"
                           "&nbsp;&nbsp;<span style='color:#ffb04d; font-size:16px; font-weight:bold;'>中 %2</span>"
                           "&nbsp;&nbsp;<span style='color:#9fb0c1;'>共 %3 条异常，请及时处理</span>")
                .arg(high).arg(medium).arg(m_alarms.size()));
    }
}

void AlarmCenterPage::onHandle() {
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中一条告警"));
        return;
    }
    const AlarmRow& r = m_alarms.at(row);
    if (r.type == QStringLiteral("设备故障") || r.type == QStringLiteral("设备离线")) {
        QMessageBox box(QMessageBox::Question, QStringLiteral("处理告警"),
                        QStringLiteral("对 %1 执行「远程重启」？").arg(r.object),
                        QMessageBox::NoButton, this);
        QPushButton* yes = box.addButton(QStringLiteral("重启"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() != yes) {
            return;
        }
        m_api->restartCharger(r.chargerId, [this, row](int code, const QString& msg, const QJsonObject&) {
            if (code == proto::code::Ok) {
                m_alarms.removeAt(row);
                renderAlarms();
            } else {
                QMessageBox::warning(this, QStringLiteral("处理失败"), msg);
            }
        });
    } else {   // 站在线率低 -> 建议停用? 这里仅提示人工介入
        QMessageBox::information(this, QStringLiteral("处理告警"),
                                 QStringLiteral("该告警建议人工处理（检查供电/网络）。可到「充电站管理」停用该站。"));
    }
}
