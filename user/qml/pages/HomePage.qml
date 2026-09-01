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
            font.pixelSize: 24
            font.bold: true
            color: "#212121"
        }
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("连接服务端")
            onClicked: backend.connectServer("")
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("站点列表将在此展示")
            color: "#757575"
        }
    }
}