import QtQuick
import QtQuick.Controls
import UserClient

// 切换账号页：展示本地已注册账号列表。
// 点击账号 → 发 accountSelected 信号并返回登录页（由 LoginPage 回填账号/密码/勾选状态）。
// 行右侧「冻结/解冻」为本地演示入口：生产环境冻结状态由服务端 user.status 管控，此处仅用于联调。
Item {
    id: root

    readonly property var stackView: StackView.view
    signal accountSelected(string account)

    // 不透明背景：遮住下层登录页（页面根 Item 默认透明）
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
        text: qsTr("切换账号")
        font.pixelSize: Theme.fontSizeTitle
        font.bold: true
        color: Theme.textPrimary
    }

    // 账号列表
    ListView {
        id: accountList
        anchors.top: parent.top
        anchors.topMargin: 96
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 32
        anchors.rightMargin: 32
        clip: true
        spacing: 12
        model: authStore.accounts()

        // 空态
        Text {
            anchors.centerIn: parent
            visible: accountList.count === 0
            text: qsTr("暂无本地账号，请先注册")
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.textSecondary
        }

        delegate: Rectangle {
            id: row
            required property string modelData
            width: accountList.width
            height: 64
            radius: Theme.radiusSmall
            color: Theme.card
            border.color: Theme.border
            border.width: 1

            // 点击整行 = 选择该账号，回登录页回填
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    root.accountSelected(modelData)
                    root.stackView.pop()
                }
            }

            // 账号 + 配置角标
            Row {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.right: freezeBtn.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Text {
                    text: modelData
                    font.pixelSize: Theme.fontSizeBase
                    font.bold: true
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                    width: implicitWidth
                }
                Text {
                    visible: authStore.rememberPasswordFor(modelData)
                    text: qsTr("记住")
                    font.pixelSize: Theme.fontSizeTiny
                    color: Theme.primary
                    verticalAlignment: Text.AlignVCenter
                }
                Text {
                    visible: authStore.autoLoginFor(modelData)
                    text: qsTr("自动登录")
                    font.pixelSize: Theme.fontSizeTiny
                    color: Theme.success
                    verticalAlignment: Text.AlignVCenter
                }
                Text {
                    visible: authStore.isFrozen(modelData)
                    text: qsTr("已冻结")
                    font.pixelSize: Theme.fontSizeTiny
                    color: Theme.danger
                    verticalAlignment: Text.AlignVCenter
                }
            }

            // 冻结/解冻（演示入口，生产由服务端管控）
            Text {
                id: freezeBtn
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: authStore.isFrozen(modelData) ? qsTr("解冻") : qsTr("冻结")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.primary
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        authStore.setAccountStatus(
                            modelData,
                            authStore.isFrozen(modelData) ? "normal" : "frozen")
                        // 强制刷新列表行（accounts() 无 change 通知，需重建模型）
                        accountList.model = authStore.accounts()
                    }
                }
            }
        }
    }
}
