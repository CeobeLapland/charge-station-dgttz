import QtQuick
import QtQuick.Controls
import UserClient

// 设置页：当前仅「退出账号」，点击后登出并回到登录页
Item {
    id: root

    readonly property var stackView: StackView.view

    // 不透明背景：遮住下层个人页（页面根 Item 默认透明）
    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    // 顶部返回
    Rectangle {
        width: 40
        height: 40
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 20
        anchors.leftMargin: 16
        radius: 20
        color: Theme.background
        border.color: Theme.border
        border.width: 1
        Text {
            anchors.centerIn: parent
            text: "‹"
            font.pixelSize: 24
            color: Theme.primary
        }
        MouseArea {
            anchors.fill: parent
            onClicked: root.stackView.pop()
        }
    }

    // 标题
    Text {
        anchors.top: parent.top
        anchors.topMargin: 32
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("设置")
        font.pixelSize: Theme.fontSizeTitle
        font.bold: true
        color: Theme.textPrimary
    }

    // 退出账号
    Column {
        anchors.top: parent.top
        anchors.topMargin: 120
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 32
        anchors.rightMargin: 32
        spacing: 12

        Button {
            id: logoutBtn
            width: parent.width
            height: 48
            text: qsTr("退出账号")
            onClicked: authStore.logout()
            // 退出后 authStore.isLoggedIn 变化，Main.qml 自动 replace 到登录页
            background: Rectangle {
                radius: Theme.radiusSmall
                color: Theme.danger
            }
            contentItem: Text {
                text: logoutBtn.text
                color: "#ffffff"
                font.pixelSize: Theme.fontSizeBase
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
