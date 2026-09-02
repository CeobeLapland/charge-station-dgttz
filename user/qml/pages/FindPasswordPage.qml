import QtQuick
import QtQuick.Controls
import UserClient

// 找回密码页：账号 + 验证码（发送后 60s 倒计时，演示期固定 123456）。
// 验证码正确后显示「新密码 / 确认密码」两栏，两次一致即更新本地密码并返回登录页。
Item {
    id: root

    // 从登录页带入的账号（可为空，允许手动输入）
    property string account: ""

    readonly property var stackView: StackView.view

    // 短信验证码接口占位：真实接入时在此处调用后端发送短信并返回 code，演示期固定 123456
    readonly property string mockCode: "123456"
    property int countdown: 0           // 剩余秒数，>0 时禁止重发
    property bool codeVerified: false   // 验证码通过后显示新密码栏

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
    Column {
        anchors.top: parent.top
        anchors.topMargin: 40
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("找回密码")
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            color: Theme.textPrimary
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: codeVerified ? qsTr("验证通过，请设置新密码") : qsTr("通过验证码验证身份")
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.textSecondary
        }
    }

    // 表单
    Column {
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
                    text: root.account
                }
            }
        }

        // 验证码 + 发送按钮
        Column {
            width: parent.width
            spacing: 6
            Text {
                text: qsTr("验证码")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.textSecondary
            }
            Rectangle {
                width: parent.width
                height: 48
                radius: Theme.radiusSmall
                color: Theme.card
                border.color: codeField.activeFocus ? Theme.primary : Theme.border
                border.width: 1
                TextField {
                    id: codeField
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 108
                    verticalAlignment: Text.AlignVCenter
                    placeholderText: qsTr("请输入验证码")
                    maximumLength: 6
                    enabled: !codeVerified
                }
                // 发送 / 倒计时按钮
                Rectangle {
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    width: 100
                    height: 40
                    radius: Theme.radiusSmall
                    color: sendBtn.enabled ? Theme.primary : Theme.border
                    Text {
                        anchors.centerIn: parent
                        text: root.countdown > 0 ? qsTr("%1 秒后重发").arg(root.countdown)
                                                 : qsTr("发送验证码")
                        font.pixelSize: Theme.fontSizeSmall
                        color: "#ffffff"
                    }
                    MouseArea {
                        id: sendBtn
                        anchors.fill: parent
                        enabled: root.countdown === 0 && !root.codeVerified
                        onClicked: sendCode()
                    }
                }
            }
        }

        // 验证码通过后显示：新密码 + 确认密码
        Column {
            width: parent.width
            spacing: 14
            visible: root.codeVerified

            Column {
                width: parent.width
                spacing: 6
                Text {
                    text: qsTr("新密码")
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textSecondary
                }
                Rectangle {
                    width: parent.width
                    height: 48
                    radius: Theme.radiusSmall
                    color: Theme.card
                    border.color: newPwField.activeFocus ? Theme.primary : Theme.border
                    border.width: 1
                    TextField {
                        id: newPwField
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 44
                        verticalAlignment: Text.AlignVCenter
                        placeholderText: qsTr("请输入新密码")
                        echoMode: TextInput.Password
                        inputMethodHints: Qt.ImhHiddenText
                    }
                    EyeToggle { target: newPwField }
                }
            }

            Column {
                width: parent.width
                spacing: 6
                Text {
                    text: qsTr("确认新密码")
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textSecondary
                }
                Rectangle {
                    width: parent.width
                    height: 48
                    radius: Theme.radiusSmall
                    color: Theme.card
                    border.color: confirmPwField.activeFocus ? Theme.primary : Theme.border
                    border.width: 1
                    TextField {
                        id: confirmPwField
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 44
                        verticalAlignment: Text.AlignVCenter
                        placeholderText: qsTr("请再次输入新密码")
                        echoMode: TextInput.Password
                        inputMethodHints: Qt.ImhHiddenText
                    }
                    EyeToggle { target: confirmPwField }
                }
            }
        }

        // 提示/错误
        Text {
            id: errText
            width: parent.width
            visible: false
            text: qsTr("")
            color: Theme.danger
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.Wrap
        }

        // 确认按钮
        Button {
            id: confirmBtn
            width: parent.width
            height: 48
            text: root.codeVerified ? qsTr("确认") : qsTr("下一步")
            onClicked: root.codeVerified ? submit() : verifyCode()
            background: Rectangle {
                radius: Theme.radiusSmall
                color: Theme.primary
            }
            contentItem: Text {
                text: confirmBtn.text
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

    // 60s 倒计时
    Timer {
        id: countdownTimer
        interval: 1000
        repeat: true
        onTriggered: {
            root.countdown--
            if (root.countdown <= 0) {
                root.countdown = 0
                countdownTimer.stop()
            }
        }
    }

    // 修改成功弹窗：确认后返回登录页
    ConfirmDialog {
        id: successDlg
        titleText: qsTr("修改成功")
        messageText: qsTr("密码已更新，请使用新密码重新登录。")
        okText: qsTr("确定")
        cancelText: qsTr("取消")
        onConfirmed: root.stackView.pop()
        onCancelled: { }
    }

    // 发送验证码（短信接口占位）
    function sendCode() {
        var acc = accountField.text.trim()
        if (acc.length === 0) { showErr(qsTr("请输入账号")); return }
        if (!authStore.hasAccount(acc)) { showErr(qsTr("该账号未注册，无法找回密码")); return }
        // TODO(短信): 真实实现调用后端发送短信，并把验证码返回给用户；演示期固定 123456
        root.countdown = 60
        countdownTimer.start()
        showErr(qsTr("验证码已发送（演示期固定 123456），1 分钟内有效"), false)
    }

    // 校验验证码，通过后显示新密码栏
    function verifyCode() {
        if (codeField.text.trim() !== root.mockCode) {
            showErr(qsTr("验证码错误"))
            return
        }
        root.codeVerified = true
        errText.visible = false
    }

    // 校验新密码并提交
    function submit() {
        var acc = accountField.text.trim()
        var np = newPwField.text
        if (np.length === 0) { showErr(qsTr("请输入新密码")); return }
        if (np !== confirmPwField.text) { showErr(qsTr("两次输入的密码不一致")); return }
        authStore.resetPassword(acc, np)
        successDlg.open()
    }

    function showErr(msg, isError) {
        errText.text = msg
        errText.color = (isError === false) ? Theme.success : Theme.danger
        errText.visible = true
    }
}
