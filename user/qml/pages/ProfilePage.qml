import QtQuick
import QtQuick.Controls
import UserClient

// 「我的」页（占位骨架）
Item {
    id: root
    property int navIndex: 2

    Column {
        anchors.centerIn: parent
        spacing: 12
        Text { text: qsTr("我的"); font.pixelSize: 24; font.bold: true; color: "#212121" }
        Text { text: qsTr("头像 / 昵称 / 余额将在此展示"); color: "#757575" }
    }
}