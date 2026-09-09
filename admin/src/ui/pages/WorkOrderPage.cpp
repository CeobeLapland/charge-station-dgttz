#include "ui/pages/WorkOrderPage.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSize>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>

#include "network/Protocol.h"
#include "services/ApiClient.h"

namespace {

QString priorityText(const QString& priority) {
    if (priority == QStringLiteral("high")) return QStringLiteral("高优先级");
    if (priority == QStringLiteral("low")) return QStringLiteral("低优先级");
    return QStringLiteral("普通");
}

QString previewText(const QString& text) {
    QString oneLine = text.simplified();
    if (oneLine.size() > 42)
        oneLine = oneLine.left(42) + QStringLiteral("…");
    return oneLine;
}

QJsonArray sortedOrders(const QJsonArray& orders, bool newestFirst) {
    QVector<QJsonObject> vec;
    vec.reserve(orders.size());
    for (const QJsonValue& v : orders)
        vec.append(v.toObject());
    std::sort(vec.begin(), vec.end(), [newestFirst](const QJsonObject& a, const QJsonObject& b) {
        const int aid = a.value(QStringLiteral("id")).toInt();
        const int bid = b.value(QStringLiteral("id")).toInt();
        return newestFirst ? aid > bid : aid < bid;
    });
    QJsonArray out;
    for (const QJsonObject& obj : vec)
        out.append(obj);
    return out;
}

}  // namespace

WorkOrderPage::WorkOrderPage(ApiClient* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(12);

    auto* header = new QHBoxLayout;
    auto* title = new QLabel(QStringLiteral("工单客服"));
    title->setStyleSheet(QStringLiteral("font-size:22px;font-weight:700;"));
    m_filterCombo = new QComboBox;
    m_filterCombo->addItem(QStringLiteral("全部会话"), QString());
    m_filterCombo->addItem(QStringLiteral("待处理"), QStringLiteral("pending"));
    m_filterCombo->addItem(QStringLiteral("处理中"), QStringLiteral("processing"));
    m_filterCombo->addItem(QStringLiteral("已完成"), QStringLiteral("completed"));
    m_filterCombo->addItem(QStringLiteral("已关闭"), QStringLiteral("closed"));
    auto* refreshBtn = new QPushButton(QStringLiteral("刷新"));
    header->addWidget(title);
    header->addStretch();
    header->addWidget(new QLabel(QStringLiteral("筛选：")));
    header->addWidget(m_filterCombo);
    header->addWidget(refreshBtn);
    root->addLayout(header);

    auto* body = new QHBoxLayout;
    body->setSpacing(12);
    root->addLayout(body, 1);

    m_conversationList = new QListWidget;
    m_conversationList->setFixedWidth(300);
    m_conversationList->setObjectName(QStringLiteral("card"));
    body->addWidget(m_conversationList);

    auto* chatPanel = new QWidget;
    chatPanel->setObjectName(QStringLiteral("card"));
    auto* chatLayout = new QVBoxLayout(chatPanel);
    chatLayout->setContentsMargins(14, 14, 14, 14);
    chatLayout->setSpacing(10);
    body->addWidget(chatPanel, 1);

    m_chatTitle = new QLabel(QStringLiteral("请选择左侧会话"));
    m_chatTitle->setStyleSheet(QStringLiteral("font-size:18px;font-weight:700;"));
    chatLayout->addWidget(m_chatTitle);

    m_messageArea = new QScrollArea;
    m_messageArea->setWidgetResizable(true);
    m_messageArea->setFrameShape(QFrame::NoFrame);
    m_messageContent = new QWidget;
    m_messageLayout = new QVBoxLayout(m_messageContent);
    m_messageLayout->setContentsMargins(6, 6, 6, 6);
    m_messageLayout->setSpacing(10);
    m_messageLayout->addStretch();
    m_messageArea->setWidget(m_messageContent);
    chatLayout->addWidget(m_messageArea, 1);

    m_hintLabel = new QLabel(QStringLiteral("用户消息会显示在左侧，客服回复会显示在右侧。"));
    m_hintLabel->setStyleSheet(QStringLiteral("color:#6b7280;"));
    m_hintLabel->setWordWrap(true);
    chatLayout->addWidget(m_hintLabel);

    m_replyEdit = new QTextEdit;
    m_replyEdit->setPlaceholderText(QStringLiteral("输入客服回复…"));
    m_replyEdit->setMinimumHeight(78);
    chatLayout->addWidget(m_replyEdit);

    auto* actionRow = new QHBoxLayout;
    m_statusCombo = new QComboBox;
    m_statusCombo->addItem(QStringLiteral("回复后标记为已完成"), QStringLiteral("completed"));
    m_statusCombo->addItem(QStringLiteral("回复后保持处理中"), QStringLiteral("processing"));
    m_statusCombo->addItem(QStringLiteral("回复并关闭工单"), QStringLiteral("closed"));
    m_submitBtn = new QPushButton(QStringLiteral("发送"));
    m_submitBtn->setObjectName(QStringLiteral("primaryButton"));
    actionRow->addWidget(m_statusCombo);
    actionRow->addStretch();
    actionRow->addWidget(m_submitBtn);
    chatLayout->addLayout(actionRow);

    connect(refreshBtn, &QPushButton::clicked, this, &WorkOrderPage::refresh);
    connect(m_filterCombo, &QComboBox::currentIndexChanged, this, &WorkOrderPage::refresh);
    connect(m_conversationList, &QListWidget::currentRowChanged, this, &WorkOrderPage::selectConversation);
    connect(m_submitBtn, &QPushButton::clicked, this, &WorkOrderPage::submitReply);
    connect(m_api, &ApiClient::pushReceived, this, [this](const QJsonObject& message) {
        if (message.value(QStringLiteral("type")).toString() == proto::type::kPushWorkOrder)
            refresh();
    });
    connect(m_api, &ApiClient::connectionStateChanged, this, [this](bool connected) {
        if (connected)
            refresh();
    });

    refresh();
}

QString WorkOrderPage::statusText(const QString& status) const {
    if (status == QStringLiteral("pending")) return QStringLiteral("待处理");
    if (status == QStringLiteral("processing")) return QStringLiteral("处理中");
    if (status == QStringLiteral("completed")) return QStringLiteral("已完成");
    if (status == QStringLiteral("closed")) return QStringLiteral("已关闭");
    return status.isEmpty() ? QStringLiteral("-") : status;
}

void WorkOrderPage::refresh() {
    const int keepUserId = m_currentUserId;
    const QString status = m_filterCombo->currentData().toString();
    m_api->fetchWorkOrders(status, [this, keepUserId](int code, const QString& message, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            m_hintLabel->setText(message.isEmpty()
                                     ? QStringLiteral("暂时无法获取工单，请确认管理端已连接服务端。")
                                     : message);
            return;
        }
        renderConversationList(payload.value(QStringLiteral("work_orders")).toArray(), keepUserId);
    });
}

void WorkOrderPage::renderConversationList(const QJsonArray& rows, int keepUserId) {
    m_conversationList->clear();
    int rowToSelect = -1;
    QMap<int, QJsonArray> byUser;
    for (int i = 0; i < rows.size(); ++i) {
        const QJsonObject w = rows.at(i).toObject();
        const int userId = w.value(QStringLiteral("user_id")).toInt();
        QJsonArray grouped = byUser.value(userId);
        grouped.append(w);
        byUser.insert(userId, grouped);
    }

    int rowIndex = 0;
    for (auto it = byUser.cbegin(); it != byUser.cend(); ++it, ++rowIndex) {
        QJsonArray orders = sortedOrders(it.value(), true);
        const QJsonObject latest = orders.first().toObject();
        const int userId = it.key();
        QJsonObject conversation;
        conversation.insert(QStringLiteral("user_id"), userId);
        conversation.insert(QStringLiteral("orders"), orders);

        const QString text = QStringLiteral("用户 %1\n%2\n%3 · 共 %4 条")
                                 .arg(userId)
                                 .arg(previewText(latest.value(QStringLiteral("description")).toString()))
                                 .arg(statusText(latest.value(QStringLiteral("status")).toString()))
                                 .arg(orders.size());
        auto* rowItem = new QListWidgetItem(text, m_conversationList);
        rowItem->setData(Qt::UserRole, conversation.toVariantMap());
        rowItem->setSizeHint(QSize(280, 76));
        if (userId == keepUserId)
            rowToSelect = rowIndex;
    }

    if (rows.isEmpty()) {
        m_currentWorkOrderId = 0;
        m_currentUserId = 0;
        renderMessages({});
        m_hintLabel->setText(QStringLiteral("当前筛选下没有客服会话。"));
        return;
    }
    if (rowToSelect < 0)
        rowToSelect = 0;
    m_conversationList->setCurrentRow(rowToSelect);
}

QJsonObject WorkOrderPage::selectedConversation() const {
    auto* current = m_conversationList->currentItem();
    if (!current)
        return {};
    return QJsonObject::fromVariantMap(current->data(Qt::UserRole).toMap());
}

QJsonObject WorkOrderPage::selectedWorkOrder() const {
    const QJsonArray orders = selectedConversation().value(QStringLiteral("orders")).toArray();
    for (const QJsonValue& v : orders) {
        const QJsonObject w = v.toObject();
        const QString status = w.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("pending") || status == QStringLiteral("processing"))
            return w;
    }
    return orders.isEmpty() ? QJsonObject() : orders.first().toObject();
}

void WorkOrderPage::selectConversation() {
    const QJsonObject conversation = selectedConversation();
    m_currentUserId = conversation.value(QStringLiteral("user_id")).toInt();
    renderMessages(conversation);
}

void WorkOrderPage::appendBubble(QVBoxLayout* layout, const QString& sender, const QString& content,
                                 const QString& time, bool outgoing) {
    auto* row = new QWidget;
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    if (outgoing)
        rowLayout->addStretch();

    auto* bubble = new QLabel(QStringLiteral("%1\n%2\n%3").arg(sender, content, time));
    bubble->setWordWrap(true);
    bubble->setMinimumWidth(220);
    bubble->setMaximumWidth(520);
    bubble->setStyleSheet(outgoing
                              ? QStringLiteral("background:#0e7dff;color:white;border-radius:10px;padding:10px;")
                              : QStringLiteral("background:white;color:#111827;border:1px solid #d0d5dd;border-radius:10px;padding:10px;"));
    rowLayout->addWidget(bubble);
    if (!outgoing)
        rowLayout->addStretch();
    layout->addWidget(row);
}

void WorkOrderPage::renderMessages(const QJsonObject& conversation) {
    while (QLayoutItem* child = m_messageLayout->takeAt(0)) {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }

    if (conversation.isEmpty()) {
        m_chatTitle->setText(QStringLiteral("请选择左侧会话"));
        m_messageLayout->addStretch();
        m_replyEdit->clear();
        m_replyEdit->setEnabled(false);
        m_submitBtn->setEnabled(false);
        return;
    }

    const int userId = conversation.value(QStringLiteral("user_id")).toInt();
    QJsonArray orders = sortedOrders(conversation.value(QStringLiteral("orders")).toArray(), false);
    m_chatTitle->setText(QStringLiteral("用户 %1 的客服会话  ·  共 %2 条消息")
                             .arg(userId).arg(orders.size()));
    m_hintLabel->setText(QStringLiteral("用户消息来自用户端“客服消息”。底部输入框发送后，用户端在线会立即收到。"));

    for (const QJsonValue& v : orders) {
        const QJsonObject workOrder = v.toObject();
        appendBubble(m_messageLayout, QStringLiteral("用户 %1").arg(userId),
                     workOrder.value(QStringLiteral("description")).toString(),
                     workOrder.value(QStringLiteral("create_time")).toString(), false);
        const QString result = workOrder.value(QStringLiteral("result")).toString();
        if (!result.isEmpty()) {
            appendBubble(m_messageLayout,
                         workOrder.value(QStringLiteral("handler")).toString(QStringLiteral("客服")),
                         result,
                         workOrder.value(QStringLiteral("handle_time")).toString(),
                         true);
        }
    }

    m_messageLayout->addStretch();
    m_replyEdit->setEnabled(true);
    m_submitBtn->setEnabled(true);
    m_replyEdit->clear();
    QTimer::singleShot(0, this, [this]() {
        m_messageArea->verticalScrollBar()->setValue(m_messageArea->verticalScrollBar()->maximum());
    });
}

void WorkOrderPage::submitReply() {
    const QJsonObject w = selectedWorkOrder();
    const int workOrderId = w.value(QStringLiteral("id")).toInt();
    const QString reply = m_replyEdit->toPlainText().trimmed();
    if (workOrderId <= 0) {
        QMessageBox::information(this, QStringLiteral("请选择会话"), QStringLiteral("请先在左侧选择一条客服会话。"));
        return;
    }
    if (reply.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("回复为空"), QStringLiteral("请填写要发给用户的客服回复。"));
        return;
    }

    m_submitBtn->setEnabled(false);
    m_api->handleWorkOrder(workOrderId, m_statusCombo->currentData().toString(), reply,
                           [this](int code, const QString& message, const QJsonObject&) {
        m_submitBtn->setEnabled(true);
        if (code != proto::code::Ok) {
            QMessageBox::warning(this, QStringLiteral("发送失败"), message);
            return;
        }
        m_hintLabel->setText(message.isEmpty() ? QStringLiteral("客服回复已发送。") : message);
        refresh();
    });
}
