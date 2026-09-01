import QtQuick
import QtQuick.Controls
import UserClient

// 注册页：账号/密码/确认密码（密码栏右侧小眼睛可切换明文），记住密码/自动登录，注册后直接进入主界面
Item {
    id: root

    // 从登录页带入的初始账号/密码（自动填充，见 LoginPage 的 push）
    property string account: ""
    property string password: ""

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
            onClicked: StackView.view.pop()
        }
    }

    // 标题
    Column {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 40
        spacing: 6
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("注册账号")
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            color: Theme.textPrimary
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("注册后即可开始充电")
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.textSecondary
        }
    }

    // 注册表单
    Column {
        id: form
        anchors.top: parent.top
        anchors.topMargin: 130
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 32
        anchors.rightMargin: 32
        spacing: 14

        // 账号
        Column {
            width: parent.width
            spacing: 6
            Text {
                text: qsTr("账号")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.textSecondary
            }
            Rectangle {
                width: parent.width
                height: 48
                radius: Theme.radiusSmall
                color: Theme.card
                border.color: accountField.activeFocus ? Theme.primary : Theme.border
                border.width: 1
                TextField {
                    id: accountField
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    verticalAlignment: Text.AlignVCenter
                    placeholderText: qsTr("请输入账号")
                    maximumLength: 20
                }
            }
        }

        // 密码（带小眼睛）
        Column {
            width: parent.width
            spacing: 6
            Text {
                text: qsTr("密码")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.textSecondary
            }
            Rectangle {
                width: parent.width
                height: 48
                radius: Theme.radiusSmall
                color: Theme.card
                border.color: passwordField.activeFocus ? Theme.primary : Theme.border
                border.width: 1
                TextField {
                    id: passwordField
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 44
                    verticalAlignment: Text.AlignVCenter
                    placeholderText: qsTr("请输入密码")
                    echoMode: TextInput.Password
                    inputMethodHints: Qt.ImhHiddenText
                }
                EyeToggle {
                    target: passwordField
                }
            }
        }

        // 确认密码（带小眼睛）
        Column {
            width: parent.width
            spacing: 6
            Text {
                text: qsTr("确认密码")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.textSecondary
            }
            Rectangle {
                width: parent.width
                height: 48
                radius: Theme.radiusSmall
                color: Theme.card
                border.color: confirmField.activeFocus ? Theme.primary : Theme.border
                border.width: 1
                TextField {
                    id: confirmField
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 44
                    verticalAlignment: Text.AlignVCenter
                    placeholderText: qsTr("请再次输入密码")
                    echoMode: TextInput.Password
                    inputMethodHints: Qt.ImhHiddenText
                }
                EyeToggle {
                    target: confirmField
                }
            }
        }

        // 记住密码 + 自动登录
        Row {
            width: parent.width
            spacing: 4
            CheckBox {
                id: rememberCb
                text: qsTr("记住密码")
                font.pixelSize: Theme.fontSizeSmall
                checked: true
            }
            CheckBox {
                id: autoCb
                text: qsTr("自动登录")
                font.pixelSize: Theme.fontSizeSmall
            }
        }

        // 错误提示
        Text {
            id: errText
            width: parent.width
            visible: false
            text: qsTr("")
            color: Theme.danger
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.Wrap
        }

        // 注册按钮
        Button {
            id: registerBtn
            width: parent.width
            height: 48
            text: qsTr("注册")
            onClicked: doRegister()
            background: Rectangle {
                radius: Theme.radiusSmall
                color: Theme.primary
            }
            contentItem: Text {
                text: registerBtn.text
                color: "#ffffff"
                font.pixelSize: Theme.fontSizeBase
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    // 密码栏右侧小眼睛组件：点击在圆点/明文间切换
    component EyeToggle: Item {
        property TextField target
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        width: 24
        height: 24
        Text {
            anchors.centerIn: parent
            text: target.echoMode === TextInput.Password ? qsTr("👁") : qsTr("🙈")
            font.pixelSize: 18
            color: Theme.textSecondary
        }
        MouseArea {
            anchors.fill: parent
            onClicked: target.echoMode = (target.echoMode === TextInput.Password)
                        ? TextInput.Normal : TextInput.Password
        }
    }

    // 未勾选自动登录时的确认弹窗
    ConfirmDialog {
        id: confirmRegDlg
        titleText: qsTr("提示")
        messageText: qsTr("您未勾选自动登录，下次打开应用需重新登录，确认注册吗？")
        okText: qsTr("确定")
        cancelText: qsTr("取消")
        onConfirmed: finishRegister()
        onCancelled: { }
    }

    Component.onCompleted: {
        if (account.length > 0)
            accountField.text = account
        if (password.length > 0)
            passwordField.text = password
    }

    function doRegister() {
        var acc = accountField.text.trim()
        var pw = passwordField.text
        var cp = confirmField.text.trim()
        if (acc.length === 0) { showErr(qsTr("请输入账号")); return }
        if (pw.length === 0) { showErr(qsTr("请输入密码")); return }
        if (cp.length === 0) { showErr(qsTr("请输入确认密码")); return }
        if (pw !== confirmField.text) { showErr(qsTr("两次输入的密码不一致")); return }
        if (authStore.hasAccount(acc)) { showErr(qsTr("该账号已注册，请直接登录")); return }

        if (!autoCb.checked)
            confirmRegDlg.open()
        else
            finishRegister()
    }

    function finishRegister() {
        var acc = accountField.text.trim()
        authStore.registerAccount(acc, passwordField.text, rememberCb.checked, autoCb.checked)
        authStore.login(acc)
        // login 后 authStore.isLoggedIn 变化，Main.qml 自动切换到主界面
    }

    function showErr(msg) {
        errText.text = msg
        errText.visible = true
    }
}