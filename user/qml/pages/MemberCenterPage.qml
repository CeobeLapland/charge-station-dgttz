import QtQuick
import QtQuick.Controls
import UserClient

// 会员中心页：当前订阅卡片 + 套餐列表（周卡/月卡/季卡/年卡）+ 优惠政策说明
Item {
    id: root
    readonly property var stackView: StackView.view

    readonly property var plans: UserData.memberPlans()
    readonly property var curPlan: UserData.currentPlan()

    // 背景
    Rectangle { anchors.fill: parent; color: Theme.background }

    // 顶部渐变栏
    Rectangle {
        width: parent.width; height: 200
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.primary }
            GradientStop { position: 1; color: Theme.accent }
        }
        // 返回+标题
        Row {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: parent.top; anchors.topMargin: 24
            anchors.leftMargin: 8; anchors.rightMargin: 20
            spacing: 8
            Text {
                text: "‹"; font.pixelSize: 28; color: "#ffffff"
                MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: root.stackView.pop() }
            }
            Text {
                text: qsTr("会员中心")
                color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true
            }
        }
        // 当前套餐卡
        Column {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.bottom: parent.bottom; anchors.bottomMargin: 20
            anchors.leftMargin: 20; anchors.rightMargin: 20
            spacing: 4
            Text {
                text: qsTr("当前套餐：") + ((curPlan && curPlan.name) || qsTr("未开通"))
                color: "#FFFFFFEE"; font.pixelSize: Theme.fontSizeSmall
            }
            Text {
                visible: !!(curPlan && curPlan.status) && curPlan.status === "active"
                text: qsTr("有效期至 ") + ((curPlan && curPlan.end_time) || "")
                      + qsTr("（剩余 ") + ((curPlan && curPlan.days_left) || 0) + qsTr(" 天）")
                color: "#FFFFFFCC"; font.pixelSize: Theme.fontSizeTiny
            }
        }
    }

    ScrollView {
        anchors.top: parent.top; anchors.topMargin: 200
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true

        Column {
            width: root.width
            spacing: 16
            topPadding: 16
            bottomPadding: 32

            // 特权说明
            Rectangle {
                width: parent.width - 32; x: 16
                height: perksCol.implicitHeight + 20
                color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
                Column {
                    id: perksCol
                    width: parent.width - 24
                    anchors.left: parent.left; anchors.leftMargin: 12
                    anchors.top: parent.top; anchors.topMargin: 12
                    spacing: 8
                    Text { text: qsTr("会员特权"); font.pixelSize: Theme.fontSizeSmall + 1; font.bold: true; color: Theme.textPrimary }
                    Row { width: parent.width; spacing: 8
                        Repeater {
                            model: [
                                { icon: "\u{1F4B0}", label: qsTr("服务费折扣"), desc: qsTr("最低 6.5 折") },
                                { icon: "\u{1F319}", label: qsTr("夜间特惠"), desc: qsTr("最低 6 折") },
                                { icon: "\u{2B50}",   label: qsTr("积分翻倍"), desc: qsTr("最高 2 倍") }
                            ]
                            delegate: Column {
                                width: (parent.width - 16) / 3
                                spacing: 2
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.icon; font.pixelSize: 26 }
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; font.pixelSize: Theme.fontSizeTiny; color: Theme.textPrimary; font.bold: true }
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.desc; font.pixelSize: 10; color: Theme.textSecondary }
                            }
                        }
                    }
                }
            }

            // 套餐选择
            Item { width: parent.width; height: 8 } // 占位间距
            Text {
                width: parent.width - 32; x: 16
                text: qsTr("选择套餐"); font.pixelSize: Theme.fontSizeSmall + 1; font.bold: true; color: Theme.textPrimary
            }

            // 套餐卡片
            Repeater {
                model: root.plans
                delegate: Rectangle {
                    property var plan: modelData
                    readonly property bool isCurrent: (curPlan && curPlan.plan_id) === modelData.id
                    width: root.width - 32; x: 16
                    height: planCardCol.implicitHeight + 22
                    color: isCurrent ? (Theme.primary + "0A") : Theme.card
                    radius: Theme.radiusSmall
                    border.color: isCurrent ? Theme.primary : Theme.border
                    border.width: isCurrent ? 2 : 1

                    Column {
                        id: planCardCol
                        width: parent.width - 24
                        anchors.left: parent.left; anchors.leftMargin: 12
                        anchors.top: parent.top; anchors.topMargin: 12
                        spacing: 8

                        Row {
                            width: parent.width
                            spacing: 10
                            // 价格
                            Column {
                                spacing: 2
                                Text {
                                    text: "\u{FFE5}" + Number(plan.price || 0).toFixed(2)
                                    color: Theme.danger; font.pixelSize: Theme.fontSizeTitle + 4; font.bold: true
                                }
                                Text {
                                    text: plan.name + " · " + plan.valid_days + qsTr(" 天")
                                    color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                                }
                            }
                            Item { width: 1; height: 1 }
                            Rectangle {
                                visible: isCurrent
                                anchors.verticalCenter: parent.verticalCenter
                                height: 24; width: currentTxt.implicitWidth + 14; radius: 12
                                color: Theme.primary
                                Text {
                                    id: currentTxt; anchors.centerIn: parent
                                    text: qsTr("使用中"); color: "#ffffff"; font.pixelSize: 11; font.bold: true
                                }
                            }
                        }

                        Text {
                            text: plan.description || ""
                            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny; wrapMode: Text.Wrap
                            width: parent.width
                        }

                        // 权益标签列表（不用 wrapMode，避免部分 Qt 环境 Flow.Wrap 枚举失效；标签短小一行足够）
                        Row {
                            width: parent.width
                            spacing: 10
                            Repeater {
                                model: (plan.perks && plan.perks.length) ? plan.perks : []
                                delegate: Rectangle {
                                    height: 22; width: perkTxt.implicitWidth + 12; radius: 11
                                    color: Theme.primary + "14"
                                    Text {
                                        id: perkTxt; anchors.centerIn: parent
                                        text: modelData; color: Theme.primary
                                        font.pixelSize: 10; font.bold: true
                                    }
                                }
                            }
                        }

                        // 开通按钮
                        Rectangle {
                            visible: !isCurrent
                            width: parent.width; height: 40; radius: 20
                            color: Theme.primary
                            Text {
                                anchors.centerIn: parent
                                text: qsTr("立即开通")
                                color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeSmall
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (UserData.recharge(plan.price)) {
                                        UserData.subscribePlan(plan.id)
                                        showToast(qsTr("开通成功，已扣款 ¥") + Number(plan.price).toFixed(2))
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 优惠政策说明
            Rectangle {
                width: parent.width - 32; x: 16
                height: policyCol.implicitHeight + 20
                color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
                Column {
                    id: policyCol
                    width: parent.width - 24
                    anchors.left: parent.left; anchors.leftMargin: 12
                    anchors.top: parent.top; anchors.topMargin: 12
                    spacing: 8
                    Text { text: qsTr("优惠政策说明"); font.pixelSize: Theme.fontSizeSmall + 1; font.bold: true; color: Theme.textPrimary }
                    Text {
                        width: parent.width; wrapMode: Text.Wrap
                        text: qsTr("1. 会员有效期内自动享受对应折扣，无需手动领取。\n2. 夜间折扣时段：每日 22:00 – 次日 06:00。\n3. 积分倍率与其他积分活动叠加，低峰时段再 ×2。\n4. 新开通会员当日生效，到期前 3 天提醒续费。\n5. 季卡及以上等级赠送生日礼包，当月有效。")
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                        lineHeight: 1.6
                    }
                }
            }
        }
    }

    // 轻提示
    Rectangle {
        id: toast; visible: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 32
        width: Math.min(toastText.implicitWidth + 32, root.width - 48)
        height: 40; radius: 20; color: "#B3000000"; z: 20
        Text { id: toastText; anchors.centerIn: parent; color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall }
    }
    Timer { id: toastTimer; interval: 2000; onTriggered: toast.visible = false }
    function showToast(msg) { toastText.text = msg; toast.visible = true; toastTimer.restart() }
}
