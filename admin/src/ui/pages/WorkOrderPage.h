#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class ApiClient;
class QComboBox;
class QLabel;
class QListWidget;
class QScrollArea;
class QVBoxLayout;
class QPushButton;
class QTextEdit;

class WorkOrderPage : public QWidget {
    Q_OBJECT
public:
    explicit WorkOrderPage(ApiClient* api, QWidget* parent = nullptr);

private slots:
    void refresh();
    void submitReply();
    void selectConversation();

private:
    void renderConversationList(const QJsonArray& rows, int keepWorkOrderId);
    void renderMessages(const QJsonObject& workOrder);
    void appendBubble(QVBoxLayout* layout, const QString& sender, const QString& content,
                      const QString& time, bool outgoing);
    QJsonObject selectedWorkOrder() const;
    QJsonObject selectedConversation() const;
    QString statusText(const QString& status) const;

    ApiClient* m_api = nullptr;
    QComboBox* m_filterCombo = nullptr;
    QListWidget* m_conversationList = nullptr;
    QScrollArea* m_messageArea = nullptr;
    QWidget* m_messageContent = nullptr;
    QVBoxLayout* m_messageLayout = nullptr;
    QLabel* m_chatTitle = nullptr;
    QLabel* m_hintLabel = nullptr;
    QTextEdit* m_replyEdit = nullptr;
    QComboBox* m_statusCombo = nullptr;
    QPushButton* m_submitBtn = nullptr;
    int m_currentWorkOrderId = 0;
    int m_currentUserId = 0;
};
