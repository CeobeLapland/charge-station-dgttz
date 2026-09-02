import QtQuick
import QtQuick.Controls
import UserClient

// 聊天详情页：左侧头像区 + 消息气泡列表。
// 两类会话：
//  - 纯通知（interactive=false）：只展示对面(左)发来的消息，无输入栏。
//  - 客服等可回复会话（interactive=true）：带底部输入栏，用户发送的消息显示在右侧。
// 所有消息均可长按删除（弹确认框）。
Item {
    id: root
    property int conversationId: 0
    property string conversationTitle: qsTr("消息")

    readonly property var stackView: StackView.view

    ListModel { id: msgModel }
    function reload() {
        msgModel.clear()
        var list = ChatData.messages(root.conversationId)
        for (var i = 0; i < list.length; i++)
            msgModel.append(list[i])
        Qt.callLater(scrollBottom)
    }
    function scrollBottom() {
        if (msgModel.count > 0)
            messageList.positionViewAtIndex(msgModel.count - 1, ListView.End)
    }
    Connections {
        target: ChatData
        function onDataChanged() { root.reload() }
    }
    Component.onCompleted: root.reload()

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
                anchors.leftMargin: 8; anchors.rightMargin: 20
                spacing: 6
                MouseArea {
                    width: 36; height: 36
                    onClicked: root.stackView.pop()
                    Text {
                        anchors.centerIn: parent
                        text: "‹"
                        color: "#ffffff"; font.pixelSize: 30
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.conversationTitle
                    color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true
                    elide: Text.ElideRight
                }
            }
        }

        // 消息列表
        ListView {
            id: messageList
            width: parent.width
            height: ChatData.isInteractive(root.conversationId)
                     ? parent.height - 72 - inputBar.height
                     : parent.height - 72
            clip: true
            model: msgModel
            delegate: Item {
                id: del
                width: messageList.width
                height: bubbleCol.implicitHeight + 20
                property bool out: model.dir === "out"

                Row {
                    anchors.top: parent.top; anchors.topMargin: 8
                    anchors.left: parent.left; anchors.leftMargin: 12
                    anchors.right: parent.right; anchors.rightMargin: 12
                    layoutDirection: del.out ? Qt.RightToLeft : Qt.LeftToRight
                    spacing: 8

                    // 头像：对面=客服/会话，自己=用户
                    Rectangle {
                        width: 38; height: 38; radius: 19
                        color: del.out ? Theme.primary + "26" : Theme.textSecondary + "1F"
                        Text {
                            anchors.centerIn: parent
                            text: del.out ? "😊" : "🤖"
                            font.pixelSize: 18
                        }
                    }

                    // 气泡 + 时间
                    Column {
                        id: bubbleCol
                        spacing: 3
                        Rectangle {
                            width: btText.width + 20
                            height: btText.height + 14
                            radius: 10
                            color: del.out ? Theme.primary : "#ffffff"
                            border.width: del.out ? 0 : 1
                            border.color: Theme.border
                            Text {
                                id: btText
                                text: model.content
                                color: del.out ? "#ffffff" : Theme.textPrimary
                                font.pixelSize: Theme.fontSizeBase
                                wrapMode: Text.Wrap
                                anchors.centerIn: parent
                                width: Math.min(implicitWidth, messageList.width * 0.56)
                            }
                        }
                        Text {
                            text: model.time
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeTiny
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }

                // 长按删除
                MouseArea {
                    anchors.fill: parent
                    onPressAndHold: delDlg.bind(model.id)
                }
            }
        }

        // 底部输入栏（仅可回复会话）
        Rectangle {
            id: inputBar
            visible: ChatData.isInteractive(root.conversationId)
            width: parent.width
            height: visible ? 60 : 0
            color: Theme.card
            border.color: Theme.border
            border.width: 1
            Row {
                anchors.fill: parent
                anchors.leftMargin: 12; anchors.rightMargin: 12
                spacing: 10
                TextField {
                    id: inputField
                    width: parent.width - sendBtn.width - parent.spacing
                    anchors.verticalCenter: parent.verticalCenter
                    placeholderText: qsTr("请输入回复内容…")
                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: Theme.background
                        border.color: Theme.border; border.width: 1
                    }
                }
                Button {
                    id: sendBtn
                    width: 64
                    height: 40
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("发送")
                    onClicked: {
                        if (ChatData.sendMessage(root.conversationId, inputField.text)) {
                            inputField.text = ""
                        }
                    }
                    contentItem: Text {
                        text: sendBtn.text
                        color: "#ffffff"; font.pixelSize: Theme.fontSizeBase; font.bold: true
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle { radius: Theme.radiusSmall; color: Theme.primary }
                }
            }
        }
    }

    // 长按消息：删除该条
    ConfirmDialog {
        id: delDlg
        titleText: qsTr("删除消息")
        messageText: qsTr("确定要删除这条消息吗？")
        okText: qsTr("删除")
        property int msgId: 0
        function bind(id) {
            msgId = id
            open()
        }
        onConfirmed: ChatData.deleteMessage(root.conversationId, delDlg.msgId)
    }
}