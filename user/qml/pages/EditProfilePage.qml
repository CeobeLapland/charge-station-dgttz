import QtQuick
import QtQuick.Controls
import UserClient

// 编辑资料页：修改头像、昵称、手机号（只读）。确定后提交到 UserData。
Item {
    id: root
    readonly property var stackView: StackView.view
    property var profile: ({})

    function refresh() { root.profile = UserData.profile() }
    Component.onCompleted: refresh()
    Connections { target: UserData; function onProfileChanged() { refresh() } }

    // 背景
    Rectangle { anchors.fill: parent; color: Theme.background }

    // 顶部栏
    Rectangle {
        width: parent.width; height: 72
        color: Theme.primary
        Row {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 8; anchors.rightMargin: 20
            spacing: 8
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "‹"; font.pixelSize: 28; color: "#ffffff"
                MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: root.stackView.pop() }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("编辑资料"); color: "#ffffff"
                font.pixelSize: Theme.fontSizeTitle; font.bold: true
            }
        }
    }

    ScrollView {
        anchors.top: parent.top; anchors.topMargin: 72
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true

        Column {
            width: root.width
            spacing: 16
            topPadding: 16
            bottomPadding: 120

            // 头像选择区
            Rectangle {
                width: parent.width - 32; x: 16
                height: avatarCol.implicitHeight + 28
                color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
                Column {
                    id: avatarCol
                    width: parent.width
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top; anchors.topMargin: 16
                    spacing: 10
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("头像"); color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeTiny
                    }
                    // 头像圆
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 88; height: 88; radius: 44; color: "#EEEEEE"
                        border.color: Theme.border; border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: profile.avatar_path ? "" : "\u{1F464}"
                            font.pixelSize: 48
                        }
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("点击更换头像")
                        color: Theme.primary; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                        MouseArea {
                            anchors.fill: parent; anchors.margins: -8
                            onClicked: {
                                // 模拟头像路径（实际用 FileDialog 选图）
                                var avatars = ["avatar_red", "avatar_blue", "avatar_green", "avatar_yellow", "avatar_purple", ""]
                                var idx = Math.floor(Math.random() * avatars.length)
                                if (UserData.updateAvatar(avatars[idx]))
                                    showToast(qsTr("头像已更新"))
                            }
                        }
                    }
                }
            }

            // 昵称
            ProfileFieldEdit {
                title: qsTr("昵称")
                value: profile.nickname || ""
                placeholder: qsTr("请输入 2–20 个字符")
                maxLen: 20
                validator: function(newValue) {
                    if (UserData.updateNickname(newValue)) {
                        showToast(qsTr("昵称已更新")); return true
                    } else {
                        showToast(qsTr("昵称需 2–20 个字符")); return false
                    }
                }
            }

            // 手机号（只读）
            ProfileFieldReadonly {
                title: qsTr("手机号")
                value: profile.phone || ""
                hint: qsTr("绑定后不可修改")
            }

            // 账号等级（只读）
            ProfileFieldReadonly {
                title: qsTr("会员等级")
                value: levelText(profile.level)
            }

            // 注册时间（只读）
            ProfileFieldReadonly {
                title: qsTr("注册时间")
                value: profile.register_time || ""
            }
        }
    }

    // 底部"确定提交"按钮
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 96; color: Theme.background
        Rectangle {
            width: parent.width - 32; x: 16
            height: 48; radius: 24
            color: Theme.primary
            anchors.verticalCenter: parent.verticalCenter
            Text {
                anchors.centerIn: parent
                text: qsTr("确定提交")
                color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeBase
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    // 所有字段都已经实时保存，这里只提示并返回
                    showToast(qsTr("资料已保存"))
                    root.stackView.pop()
                }
            }
        }
    }

    // 轻提示
    Rectangle {
        id: toast; visible: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 120
        width: Math.min(toastText.implicitWidth + 32, root.width - 48)
        height: 40; radius: 20; color: "#B3000000"; z: 20
        Text { id: toastText; anchors.centerIn: parent; color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall }
    }
    Timer { id: toastTimer; interval: 2000; onTriggered: toast.visible = false }
    function showToast(msg) { toastText.text = msg; toast.visible = true; toastTimer.restart() }

    function levelText(lv) {
        if (lv === "vip") return qsTr("VIP 会员")
        if (lv === "enterprise") return qsTr("企业会员")
        return qsTr("普通会员")
    }

    // —— 可编辑字段组件 ——
    component ProfileFieldEdit: Rectangle {
        property string title
        property string value
        property string placeholder
        property int maxLen: 20
        property var validator: function(v) { return true }   // 返回 true 成功，false 失败回滚
        width: root.width - 32; x: 16
        height: 72; color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
        Row {
            anchors.fill: parent
            anchors.leftMargin: 16; anchors.rightMargin: 16
            spacing: 12
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: parent.parent.title
                width: 80
                color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
            }
            TextField {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 80 - 56
                text: parent.parent.value
                maximumLength: parent.parent.maxLen
                placeholderText: parent.parent.placeholder
                color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall
                background: Rectangle { color: "transparent" }
                onEditingFinished: {
                    var ok = parent.parent.validator(text)
                    if (!ok) text = parent.parent.value  // 失败回滚
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "✎"; font.pixelSize: 18; color: Theme.textSecondary
            }
        }
    }

    // —— 只读字段组件 ——
    component ProfileFieldReadonly: Rectangle {
        id: pf
        property string title
        property string value
        property string hint: ""
        width: root.width - 32; x: 16
        height: hint ? 64 : 56; color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
        Column {
            anchors.fill: pf
            anchors.leftMargin: 16; anchors.rightMargin: 16
            anchors.topMargin: 10
            spacing: 2
            Row {
                width: parent.width
                Text {
                    text: pf.title || ""
                    width: 80; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                }
                Text {
                    text: pf.value || ""
                    color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                }
            }
            Text {
                visible: pf.hint && pf.hint !== ""
                text: "   " + (pf.hint || "")
                color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
            }
        }
    }
}
