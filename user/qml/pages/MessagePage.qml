import QtQuick
import QtQuick.Controls
import UserClient

// 消息页（占位骨架，后续接入站内消息 notification）
Item {
    id: root
    property int navIndex: 2

    Column {
        anchors.centerIn: parent
        spacing: 12
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("消息")
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            color: Theme.textPrimary
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("内容建设中")
            color: Theme.textSecondary
        }
    }
}
