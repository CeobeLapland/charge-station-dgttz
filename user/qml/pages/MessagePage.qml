import QtQuick
import QtQuick.Controls
import UserClient

// 消息页：仿微信/QQ 的会话列表。每个卡片 = 一个会话（按消息类型聚合），
// 预览最近一条消息，点击进入聊天详情（ChatPage），长按卡片可清空该会话历史。
Item {
    id: root
    property int navIndex: 2
    readonly property var stackView: StackView.view

    // 会话模型（由 ChatData 数据重建，订阅 dataChanged 保持同步）
    ListModel { id: convModel }

    function reload() {
        convModel.clear()
        var list = ChatData.conversations()
        for (var i = 0; i < list.length; i++)
            convModel.append(list[i])
        unreadLbl.text = ChatData.unreadTotal() > 0
                        ? qsTr("未读 ") + ChatData.unreadTotal()
                        : qsTr("全部已读")
    }

    Connections {
        target: ChatData
        function onDataChanged() { root.reload() }
    }

    Component.onCompleted: root.reload()

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
                    id: unreadLbl
                    anchors.verticalCenter: parent.verticalCenter; anchors.right: parent.right
                    color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall
                }
            }
        }

        // 会话列表
        ListView {
            id: listView
            width: parent.width
            height: parent.height - 72
            clip: true
            model: convModel
            spacing: 10
            topMargin: 8
            bottomMargin: 12
            delegate: Rectangle {
                width: listView.width - 24
                height: 84
                x: 12
                color: Theme.card
                border.color: Theme.border
                border.width: 1
                radius: Theme.radiusSmall

                // 头像（彩色圆底 + emoji）
                Rectangle {
                    id: avatar
                    anchors.left: parent.left; anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 52; height: 52; radius: 26
                    color: model.color + "1A"
                    border.color: model.color
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: model.icon
                        font.pixelSize: 24
                    }
                }

                // 未读角标
                Rectangle {
                    visible: Number(model.unread) > 0
                    anchors.left: avatar.right; anchors.top: avatar.top
                    anchors.leftMargin: -12; anchors.topMargin: -6
                    width: Math.max(20, unreadTxt.implicitWidth + 12)
                    height: 20; radius: 10
                    color: Theme.danger
                    Text {
                        id: unreadTxt
                        anchors.centerIn: parent
                        text: Number(model.unread) > 99 ? "99+" : model.unread
                        color: "#ffffff"; font.pixelSize: Theme.fontSizeTiny; font.bold: true
                    }
                }

                // 标题
                Text {
                    id: titleText
                    anchors.left: avatar.right; anchors.leftMargin: 12
                    anchors.top: parent.top; anchors.topMargin: 16
                    text: model.title
                    color: Theme.textPrimary; font.pixelSize: Theme.fontSizeBase; font.bold: true
                }

                // 时间
                Text {
                    anchors.right: parent.right; anchors.rightMargin: 12
                    anchors.baseline: titleText.baseline
                    text: model.last_time || ""
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                }

                // 最近消息预览
                Text {
                    anchors.left: avatar.right; anchors.leftMargin: 12
                    anchors.top: titleText.bottom; anchors.topMargin: 7
                    anchors.right: parent.right; anchors.rightMargin: 12
                    text: model.last_dir === "out"
                          ? qsTr("我：") + (model.last_content || "")
                          : model.last_content || ""
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                // 点击进入聊天 / 长按清空会话
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        ChatData.markRead(model.id)
                        root.stackView.push("qrc:/UserClient/qml/pages/ChatPage.qml",
                                            { conversationId: model.id })
                    }
                    onPressAndHold: clearDlg.bind(model.id, model.title)
                }
            }
        }
    }

    // 空态
    Text {
        visible: convModel.count === 0
        anchors.centerIn: parent
        text: qsTr("暂无消息")
        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
    }

    // 长按会话卡片：清空该会话
    ConfirmDialog {
        id: clearDlg
        titleText: qsTr("删除会话")
        messageText: qsTr("确定要清空该会话的全部消息吗？")
        okText: qsTr("删除")
        property int convId: 0
        function bind(id) {
            convId = id
            open()
        }
        onConfirmed: ChatData.clearConversation(clearDlg.convId)
    }
}