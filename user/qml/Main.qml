import QtQuick
import QtQuick.Controls
import UserClient

ApplicationWindow {
    id: root
    width: 414
    height: 896
    visible: true
    title: qsTr("充电用户端")
    color: appBackground
    // 竖屏手机端交互
    readonly property int pageWidth: 414
    readonly property int pageHeight: root.height - bottomNav.height

    // 全局主题色
    readonly property color appPrimary: "#1e88e5"
    readonly property color appBackground: "#f4f6f8"
    readonly property color cardColor: "#ffffff"
    readonly property color textPrimary: "#212121"
    readonly property color textSecondary: "#757575"

    Connections {
        target: backend
        function onConnected() {
            console.log("[UserClient] connected to server")
        }
        function onMessageReceived(type, code, message, payload) {
            console.log("[UserClient] recv", type, code)
        }
    }

    // 页面容器，过渡动画由 StackView 提供
    StackView {
        id: pageStack
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: bottomNav.top
        initialItem: Qt.resolvedUrl("pages/HomePage.qml")
        pushEnter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0.6
                to: 1.0
                duration: 180
            }
        }
        pushExit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1.0
                to: 0.6
                duration: 180
            }
        }
    }

    BottomNav {
        id: bottomNav
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        currentIndex: pageStack.currentItem ? pageStack.currentItem.navIndex : 0

        onPageRequested: function (index) {
            var pages = [
                Qt.resolvedUrl("pages/HomePage.qml"),
                Qt.resolvedUrl("pages/OrderPage.qml"),
                Qt.resolvedUrl("pages/ProfilePage.qml")
            ]
            bottomNav.currentIndex = index
            pageStack.pop(null)
            pageStack.replace(pages[index])
        }
    }
}