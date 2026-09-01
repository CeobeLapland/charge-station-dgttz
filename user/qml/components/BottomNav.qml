import QtQuick
import QtQuick.Controls
import UserClient

// 底部三导航：充电 / 订单 / 我的（见 spec-用户端）
Item {
    id: root
    property int currentIndex: 0
    signal pageRequested(int index)

    height: 64

    Rectangle {
        anchors.fill: parent
        color: Theme.card
        border.color: Theme.border
        border.width: 1
    }

    Row {
        anchors.fill: parent
        anchors.topMargin: 6
        anchors.bottomMargin: 6

        NavButton { index: 0; label: qsTr("充电"); icon: "⚡"; active: root.currentIndex === 0 }
        NavButton { index: 1; label: qsTr("订单"); icon: "📋"; active: root.currentIndex === 1 }
        NavButton { index: 2; label: qsTr("我的"); icon: "👤"; active: root.currentIndex === 2 }
    }

    component NavButton: Rectangle {
        id: btn
        property int index: 0
        property string label: ""
        property string icon: ""
        property bool active: false

        color: "transparent"
        width: parent.width / 3
        height: parent.height

        Column {
            anchors.centerIn: parent
            spacing: 2
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: btn.icon
                font.pixelSize: 22
                color: btn.active ? Theme.primary : Theme.textSecondary
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: btn.label
                font.pixelSize: 12
                color: btn.active ? Theme.primary : Theme.textSecondary
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.pageRequested(btn.index)
        }
    }
}