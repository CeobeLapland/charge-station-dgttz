import QtQuick
import QtQuick.Controls
import UserClient

// 登录页：顶部欢迎语+软件名，右上角切换账号；中部账号/密码、记住密码、自动登录；底部找回密码/我要申诉
Item {
    id: root

    // 标记为登录流程页：Main.qml 的 applyAuthView 用它判断「已回到登录页」，
    // 避免把设置页等非主页面误判成登录页。
    property bool isLoginPage: true

    // 在页面根节点缓存 StackView 引用：Dialog/Popup 打开时会被重挂到 Overlay，
    // 此时在弹窗回调里直接读 StackView.view 会得到 null，因此统一走这个缓存。
    readonly property var stackView: StackView.view

    // 连续输错密码计数：连续三次自动跳转找回密码
    property int failCount: 0

    // 顶部右上角「切换账号」：打开本地账号列表，选中后回填账号/密码/勾选状态
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
            onClicked: openSwitchAccount()
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
            MouseArea {
                anchors.fill: parent
                onClicked: openFindPassword()
            }
        }
        Text {
            text: qsTr("我要申诉")
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.textSecondary
            MouseArea {
                anchors.fill: parent
                onClicked: openAppeal()
            }
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
            root.stackView.push("qrc:/UserClient/qml/pages/RegisterPage.qml", {
                account: accountField.text.trim(),
                password: passwordField.text
            })
        }
        onCancelled: { }
    }

    // 账号被冻结 -> 拒绝登录，引导去申诉
    ConfirmDialog {
        id: frozenDlg
        titleText: qsTr("账号已被冻结")
        messageText: qsTr("该账号因异常已被冻结，暂无法登录。如有疑问可提交申诉，等待平台审核。")
        okText: qsTr("我要申诉")
        cancelText: qsTr("取消")
        onConfirmed: openAppeal()
        onCancelled: { }
    }

    // 切换账号对应账号配置回填（记住密码/自动登录）
    function restoreAccountConfig() {
        var acc = accountField.text.trim()
        failCount = 0
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
        if (acc.length === 0) {
            errText.text = qsTr("请输入账号")
            errText.visible = true
            return
        }
        if (pwd.length === 0) {
            errText.text = qsTr("请输入密码")
            errText.visible = true
            return
        }
        if (!authStore.hasAccount(acc)) {
            failCount = 0
            errText.visible = false
            notFoundDlg.open()
            return
        }
        // 冻结账号审查：拒绝登录并弹窗提示（对应 user.status=frozen）
        if (authStore.isFrozen(acc)) {
            failCount = 0
            errText.visible = false
            frozenDlg.open()
            return
        }
        if (!authStore.verifyLogin(acc, pwd)) {
            failCount++
            if (failCount >= 3) {
                failCount = 0
                errText.visible = false
                openFindPassword()
                return
            }
            errText.text = qsTr("密码错误，请重试（还可尝试 %1 次）").arg(3 - failCount)
            errText.visible = true
            return
        }
        failCount = 0
        authStore.updateOptions(acc, rememberCb.checked, autoCb.checked)
        authStore.login(acc)
        // login 后 authStore.isLoggedIn 变化，Main.qml 会自动切换到主界面
    }

    // 顶部「切换账号」：打开本地账号列表页，选中后回填（仍需用户点击登录）
    function openSwitchAccount() {
        root.stackView.push("qrc:/UserClient/qml/pages/SwitchAccountPage.qml", {
            "onAccountSelected": function(acc) { applyAccount(acc) }
        })
    }

    // 底部「找回密码」/ 连续输错自动跳转：带入当前账号
    function openFindPassword() {
        root.stackView.push("qrc:/UserClient/qml/pages/FindPasswordPage.qml", {
            account: accountField.text.trim()
        })
    }

    // 底部「我要申诉」/ 冻结提示去申诉：带入当前账号
    function openAppeal() {
        root.stackView.push("qrc:/UserClient/qml/pages/AppealPage.qml", {
            account: accountField.text.trim()
        })
    }

    // 切换账号回填：账号 + 记住密码/自动登录勾选状态
    function applyAccount(acc) {
        errText.visible = false
        failCount = 0
        accountField.text = acc
        passwordField.text = authStore.passwordFor(acc)
        rememberCb.checked = authStore.rememberPasswordFor(acc)
        autoCb.checked = authStore.autoLoginFor(acc)
    }
}