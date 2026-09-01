import QtQuick
import QtQuick.Controls
import UserClient

// 消息页：站内通知列表（notification）。数据来自 UserData.notifications()，未读消息以圆点/加粗区分。
Item {
    id: root
    property int navIndex: 2
    readonly property var stackView: StackView.view
    readonly property var items: UserData.notifications()

    function typeText(t) {
        if (t === "reservation") return qsTr("预约")
        if (t === "order")       return qsTr("订单")
        if (t === "point")       return qsTr("积分")
        if (t === "coupon")      return qsTr("优惠券")
        if (t === "work_order")  return qsTr("工单")
        if (t === "system")      return qsTr("系统")
        return qsTr("通知")
    }
    function typeIcon(t) {
        if (t === "reservation") return "\u{23F0}"
        if (t === "order")       return "\u{1F50C}"
        if (t === "point")       return "\u{2B50}"
        if (t === "coupon")      return "\u{1F3AB}"
        if (t === "work_order")  return "\u{1F527}"
        if (t === "system")      return "\u{1F4E2}"
        return "\u{1F514}"
    }
    function typeColor(t) {
        if (t === "point" || t === "coupon") return Theme.accent
        if (t === "work_order") return Theme.warn
        if (t === "system")     return Theme.textSecondary
        return Theme.primary
    }
    function isUnread(it) { return Number(it.is_read || 0) === 0 }
    function unreadCount() {
        var n = 0
        for (var i = 0; i < items.length; i++)
            if (isUnread(items[i])) n++
        return n
    }

    // 不透明背景
    Rectangle { anchors.fill: parent; color: Theme.background }

    Column {
        anchors.fill: parent
        spacing: 0

        // 顶部标题栏
        Rectangle {
            width: parent.width
            height: 72
            color: Theme.primary
            Row {
                anchors.left: parent.left; anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 20; anchors.rightMargin: 20
                spacing: 10
                Text {
                    text: qsTr("消息")
                    color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true
                }
                Item { width: 1; height: 1 }
                Text {
                    anchors.verticalCenter: parent.verticalCenter; anchors.right: parent.right
                    text: root.unreadCount() > 0 ? (qsTr("未读 ") + root.unreadCount()) : qsTr("全部已读")
                    color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall
                }
            }
        }

        // 列表
        ListView {
            id: listView
            width: parent.width
            height: parent.height - 72
            clip: true
            model: root.items
            spacing: 10
            topMargin: 12
            bottomMargin: 16

            delegate: Rectangle {
                width: listView.width - 32
                x: 16
                height: msgCol.implicitHeight + 20
                color: Theme.card
                radius: Theme.radiusSmall
                border.color: Theme.border

                // 未读圆点
                Rectangle {
                    visible: root.isUnread(modelData)
                    anchors.left: parent.left; anchors.top: parent.top
                    anchors.leftMargin: 8; anchors.topMargin: 10
                    width: 8; height: 8; radius: 4; color: Theme.danger
                }

                Column {
                    id: msgCol
                    width: parent.width - 24
                    anchors.left: parent.left; anchors.leftMargin: 12
                    anchors.top: parent.top; anchors.topMargin: 10
                    spacing: 6

                    // 图标 + 标题 + 时间
                    Row {
                        width: parent.width
                        spacing: 8
                        Rectangle {
                            width: 34; height: 34; radius: 17
                            color: root.typeColor(modelData.type) + "1A"
                            Text {
                                anchors.centerIn: parent
                                text: root.typeIcon(modelData.type)
                                font.pixelSize: 17
                            }
                        }
                        Column {
                            width: parent.width - 44 - 60
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text {
                                width: parent.width
                                text: modelData.title || ""
                                font.pixelSize: Theme.fontSizeSmall + 1
                                font.bold: root.isUnread(modelData)
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                            }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter; anchors.right: parent.right
                            text: modelData.create_time || ""
                            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                        }
                    }

                    // 内容
                    Text {
                        width: parent.width
                        text: modelData.content || ""
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    // 类型标签
                    Text {
                        text: root.typeText(modelData.type)
                        color: root.typeColor(modelData.type)
                        font.pixelSize: Theme.fontSizeTiny
                        font.bold: true
                    }
                }
            }
        }
    }

    // 空态
    Text {
        visible: root.items.length === 0
        anchors.centerIn: parent
        text: qsTr("暂无消息")
        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
    }
}