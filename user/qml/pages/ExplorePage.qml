import QtQuick
import QtQuick.Controls
import UserClient

// 探索页（占位骨架，后续接入站点搜索/推荐）
Item {
    id: root
    property int navIndex: 1

    Column {
        anchors.centerIn: parent
        spacing: 12
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("探索")
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
