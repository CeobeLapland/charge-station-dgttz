#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

// 消息中心会话数据层（内存示例态，仿微信/QQ 会话列表）。
// 一个「会话」= 一个对话卡片（按消息类型聚合），内部由多条消息(msg)组成：
//   conversations()  -> 会话卡片列表 {id, category, title, icon, color, interactive, unread, last_time}
//   messages(id)     -> 该会话的消息列表 {id, dir(in|out), content, time}
// 消息 dir 语义：in=对面(服务端)发来，out=用户自己发出（仅 interactive 会话可发）。
// 会话分两类：interactive=true(如客服) 允许用户回复；interactive=false(纯通知) 只能右侧呈现，无输入栏。
// 受污染/运行时变更走内存态，并经 dataChanged() 通知 QML 刷新界面。
class ChatData : public QObject {
    Q_OBJECT
public:
    explicit ChatData(QObject* parent = nullptr);

    // —— 读 ——
    Q_INVOKABLE bool isInteractive(int conversationId) const;
    Q_INVOKABLE int unreadTotal() const;
    Q_INVOKABLE QVariantList conversations() const;
    Q_INVOKABLE QVariantList messages(int conversationId) const;

    // —— 写（返回是否成功，触发 dataChanged） ——
    Q_INVOKABLE bool sendMessage(int conversationId, const QString& content);
    Q_INVOKABLE bool deleteMessage(int conversationId, int messageId);
    Q_INVOKABLE bool clearConversation(int conversationId);
    Q_INVOKABLE void markRead(int conversationId);

signals:
    void dataChanged();

private:
    struct Conversation {
        int id = 0;
        QString category;   // 消息类型分组（对齐 notification.type，告警用 alarm）
        QString title;      // 会话名
        QString icon;       // 头像 emoji
        QColor color;       // 头像底色
        bool interactive = false;
        int unread = 0;
        QList<QVariantMap> msgs;  // {id, dir, content, time}
    };
    QList<Conversation> m_convs;
    int m_nextMsgId = 1;
};