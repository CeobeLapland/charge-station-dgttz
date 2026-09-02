#include "ChatData.h"

#include <QDateTime>

namespace {

// 便捷构造一条消息
QVariantMap msg(const QString& dir, const QString& content, const QDateTime& time) {
    return {
        {"dir",     dir},
        {"content", content},
        {"time",    time.toString(QStringLiteral("MM-dd HH:mm"))}
    };
}

}  // namespace

ChatData::ChatData(QObject* parent)
    : QObject(parent), m_nextMsgId(1) {
    // 会话与消息为内存示例态，页面只读呈现。消息内容对齐 DATA_STRUCTURE.md 的 notification 语义。
    const auto now = QDateTime::currentDateTime();

    auto addMsg = [this](QList<QVariantMap>& msgs, const QVariantMap& m) {
        auto map = m;
        map.insert(QStringLiteral("id"), m_nextMsgId++);
        msgs.append(map);
    };

    // 1. 订单消息（纯通知）
    {
        Conversation c;
        c.id = 1; c.category = QStringLiteral("order");
        c.title = QStringLiteral("订单消息"); c.icon = QStringLiteral("⚡");
        c.color = QColor(QStringLiteral("#0e7dff"));
        c.interactive = false; c.unread = 2;
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("您在中关村软件园旗舰站的充电已完成，请及时结算。"),
                   now.addSecs(-40 * 60)));
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("您在三里屯太古里站的 A-01 号桩已开始充电，预计 35 分钟后充满。"),
                   now.addSecs(-3 * 3600)));
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("订单 9008 已自动结算，实付 31.40 元，已使用优惠券与积分。"),
                   now.addDays(-1).addSecs(-2 * 3600)));
        m_convs.append(c);
    }

    // 2. 客服消息（可回复）
    {
        Conversation c;
        c.id = 2; c.category = QStringLiteral("service");
        c.title = QStringLiteral("客服消息"); c.icon = QStringLiteral("🎧");
        c.color = QColor(QStringLiteral("#12b76a"));
        c.interactive = true; c.unread = 1;
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("您好，这里是充电平台在线客服，请问有什么可以帮您？"),
                   now.addDays(-3)));
        addMsg(c.msgs, msg(QStringLiteral("out"),
                   QStringLiteral("你好，我的订单 9007 充到一半停了，想问下是什么情况？"),
                   now.addDays(-3).addSecs(120)));
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("已为您查询到该桩存在运行异常，可能需检修，我已为您备注处理中。"),
                   now.addDays(-3).addSecs(300)));
        addMsg(c.msgs, msg(QStringLiteral("out"),
                   QStringLiteral("好的，请问大概多久能恢复？"),
                   now.addDays(-3).addSecs(420)));
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("已为您提交检修工单，预计 2 小时内完成，完成后会第一时间通知您。"),
                   now.addSecs(-2 * 3600)));
        m_convs.append(c);
    }

    // 3. 优惠券活动消息（纯通知）
    {
        Conversation c;
        c.id = 3; c.category = QStringLiteral("coupon");
        c.title = QStringLiteral("优惠券活动消息"); c.icon = QStringLiteral("🎁");
        c.color = QColor(QStringLiteral("#f79009"));
        c.interactive = false; c.unread = 2;
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("中秋限定立减 ¥12 优惠券已发放至您的卡包，中秋假期可用。"),
                   now.addSecs(-5 * 3600)));
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("您已获得「满 50 减 5」优惠券，有效期至 2025-10-31。"),
                   now.addDays(-2)));
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("818 品牌日来袭：8 月 18 日下单享满 80 减 18，全平台通用。"),
                   now.addDays(-6)));
        m_convs.append(c);
    }

    // 4. 告警通知（纯通知）
    {
        Conversation c;
        c.id = 4; c.category = QStringLiteral("alarm");
        c.title = QStringLiteral("告警通知"); c.icon = QStringLiteral("🚨");
        c.color = QColor(QStringLiteral("#f04438"));
        c.interactive = false; c.unread = 0;
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("您常去的「三里屯太古里站」A-03 桩检测到离线，正在抢修，请暂避使用。"),
                   now.addDays(-1)));
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("支付出现异常告警：订单 9012 结算中断，客服将尽快与您联系。"),
                   now.addDays(-1).addSecs(600)));
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("提示：今晚 22:00–24:00「南三环站」部分桩将进行检修升级。"),
                   now.addSecs(-8 * 3600)));
        m_convs.append(c);
    }

    // 5. 积分消息（纯通知）
    {
        Conversation c;
        c.id = 5; c.category = QStringLiteral("point");
        c.title = QStringLiteral("积分消息"); c.icon = QStringLiteral("⭐");
        c.color = QColor(QStringLiteral("#7a5af8"));
        c.interactive = false; c.unread = 0;
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("已连续签到 7 天，额外奖励 50 积分已入账。"),
                   now.addDays(-2)));
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("本单获得 32 积分，当前累计 1280 积分。"),
                   now.addDays(-5)));
        m_convs.append(c);
    }

    // 6. 预约排队消息（纯通知）
    {
        Conversation c;
        c.id = 6; c.category = QStringLiteral("reservation");
        c.title = QStringLiteral("预约排队消息"); c.icon = QStringLiteral("⏰");
        c.color = QColor(QStringLiteral("#0e9f6e"));
        c.interactive = false; c.unread = 0;
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("您预约的充电桩即将轮到，请尽快前往。"),
                   now.addDays(-1)));
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("已为您预约广州天河体育西站 B3 号快充桩，请于 23:40 前到达。"),
                   now.addDays(-3)));
        m_convs.append(c);
    }

    // 7. 系统消息（纯通知）
    {
        Conversation c;
        c.id = 7; c.category = QStringLiteral("system");
        c.title = QStringLiteral("系统消息"); c.icon = QStringLiteral("📢");
        c.color = QColor(QStringLiteral("#667085"));
        c.interactive = false; c.unread = 1;
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("检测到您的账号在上海浦东新设备登录，若非本人操作请及时修改密码。"),
                   now.addDays(-2)));
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("您的 VIP 会员将于 2025-12-31 到期，续费可享专属优惠。"),
                   now.addDays(-4)));
        addMsg(c.msgs, msg(QStringLiteral("in"),
                   QStringLiteral("8 月 22 日 02:00–04:00 将进行系统升级，期间部分服务可能短暂中断。"),
                   now.addSecs(-6 * 3600)));
        m_convs.append(c);
    }
}

bool ChatData::isInteractive(int conversationId) const {
    for (const auto& c : m_convs)
        if (c.id == conversationId)
            return c.interactive;
    return false;
}

int ChatData::unreadTotal() const {
    int n = 0;
    for (const auto& c : m_convs)
        n += c.unread;
    return n;
}

QVariantList ChatData::conversations() const {
    QVariantList out;
    for (const auto& c : m_convs) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), c.id);
        m.insert(QStringLiteral("category"), c.category);
        m.insert(QStringLiteral("title"), c.title);
        m.insert(QStringLiteral("icon"), c.icon);
        m.insert(QStringLiteral("color"), c.color.name());
        m.insert(QStringLiteral("interactive"), c.interactive);
        m.insert(QStringLiteral("unread"), c.unread);
        // 最近一条消息：内容 + 时间（用于卡片预览）
        if (!c.msgs.isEmpty()) {
            const auto& last = c.msgs.constLast();
            m.insert(QStringLiteral("last_content"), last.value(QStringLiteral("content")));
            m.insert(QStringLiteral("last_dir"), last.value(QStringLiteral("dir")));
            m.insert(QStringLiteral("last_time"), last.value(QStringLiteral("time")));
        } else {
            m.insert(QStringLiteral("last_content"), QString());
            m.insert(QStringLiteral("last_dir"), QStringLiteral("in"));
            m.insert(QStringLiteral("last_time"), QString());
        }
        out.append(m);
    }
    return out;
}

QVariantList ChatData::messages(int conversationId) const {
    for (const auto& c : m_convs) {
        if (c.id == conversationId) {
            QVariantList out;
            for (const auto& m : c.msgs)
                out.append(m);
            return out;
        }
    }
    return {};
}

bool ChatData::sendMessage(int conversationId, const QString& content) {
    const QString trimmed = content.trimmed();
    if (trimmed.isEmpty())
        return false;
    for (auto& c : m_convs) {
        if (c.id == conversationId && c.interactive) {
            auto m = msg(QStringLiteral("out"), trimmed, QDateTime::currentDateTime());
            m.insert(QStringLiteral("id"), m_nextMsgId++);
            c.msgs.append(m);
            c.unread = 0;
            emit dataChanged();
            return true;
        }
    }
    return false;
}

bool ChatData::deleteMessage(int conversationId, int messageId) {
    for (auto& c : m_convs) {
        if (c.id != conversationId)
            continue;
        for (int i = 0; i < c.msgs.size(); ++i) {
            if (c.msgs[i].value(QStringLiteral("id")).toInt() == messageId) {
                c.msgs.removeAt(i);
                emit dataChanged();
                return true;
            }
        }
    }
    return false;
}

bool ChatData::clearConversation(int conversationId) {
    for (auto& c : m_convs) {
        if (c.id == conversationId) {
            c.msgs.clear();
            c.unread = 0;
            emit dataChanged();
            return true;
        }
    }
    return false;
}

void ChatData::markRead(int conversationId) {
    for (auto& c : m_convs) {
        if (c.id == conversationId && c.unread > 0) {
            c.unread = 0;
            emit dataChanged();
            return;
        }
    }
}