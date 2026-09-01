import QtQuick
import QtQuick.Controls
import UserClient

// 首页（占位骨架，后续接入附近电站 station.nearby）
Item {
    id: root
    property int navIndex: 0

    Column {
        anchors.centerIn: parent
        spacing: 12
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("首页")
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
