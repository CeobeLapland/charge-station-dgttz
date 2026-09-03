#include "ui/pages/ChargerStatusPage.h"

#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include "network/Protocol.h"
#include "services/ApiClient.h"

ChargerStatusPage::ChargerStatusPage(ApiClient* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    auto* title = new QLabel(QStringLiteral("电桩状态分布"));
    title->setObjectName(QStringLiteral("pageTitle"));

    m_table = new QTableWidget(0, 3);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("状态"), QStringLiteral("数量"), QStringLiteral("占比")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(m_table);

    refresh();
}

void ChargerStatusPage::refresh() {
    m_api->fetchStationStatus([this](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        const QJsonObject dist = payload.value(QStringLiteral("distribution")).toObject();
        const int total = payload.value(QStringLiteral("total")).toInt(0);

        struct Row {
            QString label;
            QString key;
        };
        const QList<Row> rows = {
            {QStringLiteral("在用"), QStringLiteral("charging")},
            {QStringLiteral("闲置"), QStringLiteral("idle")},
            {QStringLiteral("故障"), QStringLiteral("fault")},
            {QStringLiteral("离线"), QStringLiteral("offline")},
        };
        m_table->setRowCount(rows.size());
        for (int i = 0; i < rows.size(); ++i) {
            const int n = dist.value(rows[i].key).toInt(0);
            const double pct = total > 0 ? (n * 100.0 / total) : 0.0;
            m_table->setItem(i, 0, new QTableWidgetItem(rows[i].label));
            m_table->setItem(i, 1, new QTableWidgetItem(QString::number(n)));
            m_table->setItem(i, 2, new QTableWidgetItem(
                QString::number(pct, 'f', 1) + QStringLiteral("%")));
        }
    });
}
