import QtQuick
import QtQuick.Controls
import UserClient

// 首页（附近电站）——占位骨架，后续接入 station.nearby
Item {
    id: root
    property int navIndex: 0

    Column {
        anchors.centerIn: parent
        spacing: 12

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("附近电站")
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            color: Theme.textPrimary
        }
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("连接服务端")
            onClicked: backend.connectServer("")
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("站点列表将在此展示")
            color: Theme.textSecondary
        }
    }
}