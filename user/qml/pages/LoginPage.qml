import QtQuick
import QtQuick.Controls
import UserClient

// 登录页：顶部欢迎语+软件名，右上角切换账号；中部账号/密码、记住密码、自动登录；底部找回密码/我要申诉
Item {
    id: root

    // 顶部右上角「切换账号」（预留：设置页的切换/退出入口尚未实现；点击清空当前输入，便于录入新账号）
    Text {
        id: switchBtn
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 24
        anchors.rightMargin: 20
        text: qsTr("切换账号")
        font.pixelSize: Theme.fontSizeSmall
        color: Theme.primary
        font.bold: true
        MouseArea {
            anchors.fill: parent
            onClicked: {
                errText.visible = false
                accountField.clear()
                passwordField.clear()
                accountField.forceActiveFocus()
            }
        }
    }

    // 顶部左侧软件名
    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 24
        anchors.leftMargin: 20
        text: qsTr("充电用户端")
        font.pixelSize: Theme.fontSizeTitle
        font.bold: true
        color: Theme.textPrimary
    }

    // 欢迎语
    Column {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 92
        spacing: 8
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("欢迎使用")
            font.pixelSize: Theme.fontSizeTitle
            font.bold: true
            color: Theme.textPrimary
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("登录您的账号，开启智能充电")
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.textSecondary
        }
    }

    // 账号密码表单
    Column {
        id: form
        anchors.top: parent.top
        anchors.topMargin: 200
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 32
        anchors.rightMargin: 32
        spacing: 16

        // 账号输入
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
                    onTextChanged: restoreAccountConfig()
                }
            }
        }

        // 密码输入
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
                    anchors.rightMargin: 14
                    verticalAlignment: Text.AlignVCenter
                    placeholderText: qsTr("请输入密码")
                    echoMode: TextInput.Password
                    inputMethodHints: Qt.ImhHiddenText
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
                onToggled: errText.visible = false
            }
            CheckBox {
                id: autoCb
                text: qsTr("自动登录")
                font.pixelSize: Theme.fontSizeSmall
                onToggled: errText.visible = false
            }
        }

        // 错误提示
        Text {
            id: errText
            width: parent.width
            visible: false
            text: qsTr("密码错误，请重试")
            color: Theme.danger
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.Wrap
        }

        // 登录按钮
        Button {
            id: loginBtn
            width: parent.width
            height: 48
            text: qsTr("登录")
            onClicked: doLogin()
            background: Rectangle {
                radius: Theme.radiusSmall
                color: Theme.primary
            }
            contentItem: Text {
                text: loginBtn.text
                color: "#ffffff"
                font.pixelSize: Theme.fontSizeBase
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    // 底部：找回密码 / 我要申诉
    Row {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 36
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 32

        Text {
            text: qsTr("找回密码")
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.textSecondary
            MouseArea { anchors.fill: parent }  // 预留，后续实现
        }
        Text {
            text: qsTr("我要申诉")
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.textSecondary
            MouseArea { anchors.fill: parent }  // 预留，后续实现
        }
    }

    // 账号不存在 -> 一键注册 确认弹窗
    ConfirmDialog {
        id: notFoundDlg
        titleText: qsTr("账号未注册")
        messageText: qsTr("未查询到账号，您要一键注册吗？")
        okText: qsTr("确定")
        cancelText: qsTr("取消")
        onConfirmed: {
            StackView.view.push(Qt.resolvedUrl("RegisterPage.qml"), {
                account: accountField.text.trim(),
                password: passwordField.text
            })
        }
        onCancelled: { }
    }

    // 切换账号对应账号配置回填（记住密码/自动登录）
    function restoreAccountConfig() {
        var acc = accountField.text.trim()
        if (authStore.hasAccount(acc)) {
            if (authStore.rememberPasswordFor(acc))
                passwordField.text = authStore.passwordFor(acc)
            rememberCb.checked = authStore.rememberPasswordFor(acc)
            autoCb.checked = authStore.autoLoginFor(acc)
        }
    }

    function doLogin() {
        var acc = accountField.text.trim()
        var pwd = passwordField.text
        if (!authStore.hasAccount(acc)) {
            notFoundDlg.open()
            return
        }
        if (!authStore.verifyLogin(acc, pwd)) {
            errText.text = qsTr("密码错误，请重试")
            errText.visible = true
            return
        }
        authStore.updateOptions(acc, rememberCb.checked, autoCb.checked)
        authStore.login(acc)
        // login 后 authStore.isLoggedIn 变化，Main.qml 会自动切换到主界面
    }
}