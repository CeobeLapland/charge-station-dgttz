import QtQuick
import QtQuick.Controls
import UserClient

// 「个人」页：右上角设置图标入口，点击进入设置页
Item {
    id: root
    property int navIndex: 3

    readonly property var stackView: StackView.view

    // 右上角设置图标（大号）
    Text {
        id: settingsIcon
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 20
        anchors.rightMargin: 20
        text: "⚙️"
        font.pixelSize: 32
        color: Theme.textPrimary
        MouseArea {
            anchors.fill: parent
            onClicked: root.stackView.push("qrc:/UserClient/qml/pages/SettingsPage.qml")
        }
    }
}
