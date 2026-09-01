import QtQuick
import QtQuick.Controls
import UserClient

// 通用确认弹窗：标题 + 内容 + 取消/确定 双按钮，供登录/注册等提示复用
Dialog {
    id: root
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(320, Overlay.overlay.width - 64)
    padding: 20
    closePolicy: Popup.NoAutoClose

    property string titleText: qsTr("提示")
    property string messageText: ""
    property string okText: qsTr("确定")
    property string cancelText: qsTr("取消")
    signal confirmed()
    signal cancelled()

    background: Rectangle {
        radius: Theme.radius
        color: Theme.card
    }

    Column {
        anchors.fill: parent
        spacing: 16

        Text {
            width: parent.width
            text: root.titleText
            font.pixelSize: Theme.fontSizeTitle
            font.bold: true
            color: Theme.textPrimary
        }

        Text {
            width: parent.width
            text: root.messageText
            font.pixelSize: Theme.fontSizeBase
            color: Theme.textSecondary
            wrapMode: Text.Wrap
        }

        Row {
            width: parent.width
            spacing: 12
            layoutDirection: Qt.RightToLeft

            Button {
                id: confirmBtn
                text: root.okText
                onClicked: {
                    root.close()
                    root.confirmed()
                }
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.primary
                }
                contentItem: Text {
                    text: confirmBtn.text
                    color: "#ffffff"
                    font.pixelSize: Theme.fontSizeBase
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                id: cancelBtn
                text: root.cancelText
                onClicked: {
                    root.close()
                    root.cancelled()
                }
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.background
                    border.color: Theme.border
                    border.width: 1
                }
                contentItem: Text {
                    text: cancelBtn.text
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeBase
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}