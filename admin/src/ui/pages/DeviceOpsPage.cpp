#include "ui/pages/DeviceOpsPage.h"

#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include "network/Protocol.h"
#include "services/ApiClient.h"

DeviceOpsPage::DeviceOpsPage(ApiClient* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    auto* title = new QLabel(QStringLiteral("设备运维（增强模块）"));
    title->setObjectName(QStringLiteral("pageTitle"));

    auto* note = new QLabel(QStringLiteral(
        "占位页：设备健康度诊断、数字孪生、模拟故障与远程运维将在此实现。\n"
        "当前展示故障风险排序（健康度从低到高）的 Mock 数据。"));
    note->setStyleSheet(QStringLiteral("color:#8a97a5;"));

    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels({QStringLiteral("桩ID"), QStringLiteral("健康度(0-100)"),
                                        QStringLiteral("风险等级"), QStringLiteral("建议操作")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(note);
    layout->addWidget(m_table);

    refresh();
}

void DeviceOpsPage::refresh() {
    m_api->fetchHealthRanks([this](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        const QJsonArray risks = payload.value(QStringLiteral("risks")).toArray();
        m_table->setRowCount(risks.size());
        for (int i = 0; i < risks.size(); ++i) {
            const QJsonObject r = risks.at(i).toObject();
            m_table->setItem(i, 0, new QTableWidgetItem(
                QString::number(r.value(QStringLiteral("charger_id")).toInt())));
            m_table->setItem(i, 1, new QTableWidgetItem(
                QString::number(r.value(QStringLiteral("health_score")).toInt())));
            m_table->setItem(i, 2, new QTableWidgetItem(r.value(QStringLiteral("risk_level")).toString()));
            m_table->setItem(i, 3, new QTableWidgetItem(r.value(QStringLiteral("suggestion")).toString()));
        }
    });
}
