#include "ui/pages/UserManagePage.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "network/Protocol.h"
#include "services/ApiClient.h"

UserManagePage::UserManagePage(ApiClient* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    auto* title = new QLabel(QStringLiteral("用户管理"));
    title->setObjectName(QStringLiteral("pageTitle"));

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(QStringLiteral("按手机号/昵称模糊搜索"));
    m_searchEdit->setFixedWidth(260);

    auto* searchBtn = new QPushButton(QStringLiteral("搜索"));
    auto* toggleBtn = new QPushButton(QStringLiteral("冻结 / 解冻"));
    toggleBtn->setObjectName(QStringLiteral("dangerButton"));
    auto* refreshBtn = new QPushButton(QStringLiteral("刷新"));

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(title);
    topRow->addStretch();
    topRow->addWidget(m_searchEdit);
    topRow->addWidget(searchBtn);
    topRow->addWidget(toggleBtn);
    topRow->addWidget(refreshBtn);

    m_table = new QTableWidget(0, 6);
    m_table->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("手机号"),
                                        QStringLiteral("昵称"), QStringLiteral("余额(元)"),
                                        QStringLiteral("注册时间"), QStringLiteral("状态")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addWidget(m_table);

    connect(searchBtn, &QPushButton::clicked, this, &UserManagePage::refresh);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &UserManagePage::refresh);
    connect(toggleBtn, &QPushButton::clicked, this, &UserManagePage::onToggleStatus);
    connect(refreshBtn, &QPushButton::clicked, this, &UserManagePage::refresh);

    refresh();
}

void UserManagePage::refresh() {
    m_api->fetchUsers(m_searchEdit->text().trimmed(),
                      [this](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        const QJsonArray users = payload.value(QStringLiteral("users")).toArray();
        m_table->setRowCount(users.size());
        for (int i = 0; i < users.size(); ++i) {
            const QJsonObject u = users.at(i).toObject();
            auto* idItem = new QTableWidgetItem(QString::number(u.value(QStringLiteral("id")).toInt()));
            idItem->setData(Qt::UserRole, u.value(QStringLiteral("id")).toInt());
            m_table->setItem(i, 0, idItem);
            m_table->setItem(i, 1, new QTableWidgetItem(u.value(QStringLiteral("phone")).toString()));
            m_table->setItem(i, 2, new QTableWidgetItem(u.value(QStringLiteral("nickname")).toString()));
            m_table->setItem(i, 3, new QTableWidgetItem(
                QString::number(u.value(QStringLiteral("balance")).toDouble(), 'f', 2)));
            m_table->setItem(i, 4, new QTableWidgetItem(u.value(QStringLiteral("register_time")).toString()));
            m_table->setItem(i, 5, new QTableWidgetItem(u.value(QStringLiteral("status")).toString()));
        }
    });
}

void UserManagePage::onToggleStatus() {
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选中一个用户"));
        return;
    }
    const int userId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    const QString current = m_table->item(row, 5)->text();
    const QString next = current == QStringLiteral("normal")
                             ? QStringLiteral("frozen")
                             : QStringLiteral("normal");
    m_api->toggleUserStatus(userId, next,
                            [this, row, next](int code, const QString& message, const QJsonObject&) {
        if (code == proto::code::Ok) {
            m_table->item(row, 5)->setText(next);
        } else {
            QMessageBox::warning(this, QStringLiteral("操作失败"), message);
        }
    });
}
