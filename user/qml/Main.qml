import QtQuick
import QtQuick.Controls
import UserClient

ApplicationWindow {
    id: root
    width: 414
    height: 896
    visible: true
    title: qsTr("充电用户端")
    // 竖屏手机端交互
    readonly property int pageWidth: 414

    Connections {
        target: backend
        function onConnected() {
            console.log("[UserClient] connected to server")
        }
        function onMessageReceived(type, code, message, payload) {
            console.log("[UserClient] recv", type, code)
        }
    }

    // 页面容器（底层 StackView）与底部三导航。登录态下显示底部导航，未登录/登录流程时隐藏。
    Column {
        anchors.fill: parent
        spacing: 0

        StackView {
            id: pageStack
            width: parent.width
            height: parent.height - (bottomNav.visible ? bottomNav.height : 0)
            initialItem: "qrc:/UserClient/qml/pages/LoginPage.qml"
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
            visible: false
            width: parent.width
            currentIndex: (pageStack.currentItem && pageStack.currentItem.navIndex !== undefined)
                         ? pageStack.currentItem.navIndex : 0

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

    // 登录/主界面切换。
    // 注意：这里不把 authStore/Theme 放在顶层属性绑定里（Qt 6.2 的 qmlcache AOT
    // 对「内联 component + Loader」场景下的 context property 解析存在缺陷，会导致读成 null），
    // 改为在 onCompleted / 信号回调里用命令式 JS 切换（onCompleted 能正常读到 context property）。
    Component.onCompleted: {
        root.color = Theme.background
        authStore.currentAccountChanged.connect(applyAuthView)
        // 启动时若已有勾选「自动登录」的账号，等 StackView 初始页加载就绪后再切到主界面
        if (authStore.isLoggedIn)
            bootstrapTimer.start()
    }

    // 让 bootstrapTimer 在初始页加载完成后的一帧再触发，避免 replace 撞上异步加载
    Timer {
        id: bootstrapTimer
        interval: 0
        repeat: false
        onTriggered: applyAuthView()
    }

    function applyAuthView() {
        var cur = pageStack.currentItem
        var wantLogin = !authStore.isLoggedIn
        // 主界面三页都有 navIndex；登录/注册页没有
        var onMain = cur ? (cur.navIndex !== undefined) : false
        bottomNav.visible = !wantLogin
        if (wantLogin === !onMain)
            return // 已在目标视图，无需切换
        pageStack.pop(null)
        pageStack.replace(wantLogin
                          ? Qt.resolvedUrl("pages/LoginPage.qml")
                          : Qt.resolvedUrl("pages/HomePage.qml"))
    }
}
