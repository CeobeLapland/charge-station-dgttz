import QtQuick
import QtQuick.Controls
import UserClient

// 订单列表（占位骨架）
Item {
    id: root
    property int navIndex: 1

    Column {
        anchors.centerIn: parent
        spacing: 12
        Text { text: qsTr("我的订单"); font.pixelSize: Theme.fontSizeLarge; font.bold: true; color: Theme.textPrimary }
        Text { text: qsTr("订单列表将在此展示"); color: Theme.textSecondary }
    }
}