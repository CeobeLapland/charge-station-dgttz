import QtQuick
import QtQuick.Controls
import UserClient

// 「我的」页（Profile 个人页）。按 DATA_STRUCTURE 的 user/vehicle/coupon/point_record 等结构渲染。
// 结构（自上而下，全部 ScrollView 内嵌 Column，间距均由 Column.spacing 控制，不使用 y 负值避免重叠）：
//   ① 用户信息栏（渐变背景）：左侧头像+昵称+手机号+会员标+总用电；右侧：编辑按钮 + 设置按钮
//   ② 会员中心入口卡片（点击进入 MemberCenterPage.qml）
//   ③ 账户一行卡：余额 / 积分 / 优惠券 + 充值按钮 （积分→PointsDetail，优惠券→CouponDetail，充值→弹 Dialog）
//   ④ 订单入口卡片：全部订单 / 已预约 / 待支付 / 开发票   （前三个→OrderPage 传筛选，发票→InvoicePage）
//   ⑤ 占位：充电画像 + 我的车辆（后续扩展）
Item {
    id: root
    property int navIndex: 3
    readonly property var stackView: StackView.view

    // —— 当前用户信息 ——
    property var profile: ({})
    readonly property var portrait: UserData.portrait()
    readonly property var vehiclesData: UserData.vehicles()
    readonly property var couponsData: UserData.coupons()
    readonly property var pointsData: UserData.pointRecords()
    readonly property double totalKwh: UserData.totalEnergyKwh()
    readonly property var curPlan: UserData.currentPlan()

    function refresh() { root.profile = UserData.profile() }
    Component.onCompleted: refresh()
    Connections { target: UserData; function onProfileChanged() { refresh() } }

    function levelText(lv) {
        if (lv === "vip") return qsTr("VIP 会员")
        if (lv === "enterprise") return qsTr("企业会员")
        return qsTr("普通会员")
    }
    function levelColor(lv) {
        if (lv === "vip") return "#FFD700"
        if (lv === "enterprise") return "#9C27B0"
        return Theme.textSecondary
    }

    // 背景
    Rectangle { anchors.fill: parent; color: Theme.background }

    // ========================= 内容滚动区 =========================
    ScrollView {
        anchors.fill: parent
        clip: true

        Column {
            width: root.width
            spacing: 14
            topPadding: 0
            bottomPadding: 24

            // ========== ① 用户信息栏（渐变背景） ==========
            Rectangle {
                width: parent.width
                height: 180
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.primary }
                    GradientStop { position: 1.0; color: Theme.accent }
                }

                // 右上角两个按钮：编辑 / 设置
                Row {
                    anchors.top: parent.top; anchors.topMargin: 18
                    anchors.right: parent.right; anchors.rightMargin: 16
                    spacing: 8
                    // 编辑按钮
                    Rectangle {
                        width: 64; height: 30; radius: 15
                        color: "#FFFFFF22"
                        border.color: "#FFFFFF55"; border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("编辑")
                            color: "#ffffff"; font.pixelSize: Theme.fontSizeTiny; font.bold: true
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.stackView.push("qrc:/UserClient/qml/pages/EditProfilePage.qml")
                        }
                    }
                    // 设置按钮
                    Rectangle {
                        width: 30; height: 30; radius: 15
                        color: "#FFFFFF22"
                        border.color: "#FFFFFF55"; border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: "\u{2699}\u{FE0F}"; font.pixelSize: 16
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.stackView.push("qrc:/UserClient/qml/pages/SettingsPage.qml")
                        }
                    }
                }

                // 左侧：头像 + 名称/账号/会员/总用电量
                Row {
                    anchors.left: parent.left; anchors.leftMargin: 20
                    anchors.right: parent.right; anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 14

                    // 头像圆
                    Rectangle {
                        width: 68; height: 68; radius: 34
                        color: "#FFFFFF"
                        border.color: "#FFFFFF99"; border.width: 2
                        Text {
                            anchors.centerIn: parent
                            text: profile.avatar_path ? "" : "\u{1F464}"
                            font.pixelSize: 36
                        }
                    }

                    // 名称、账号、会员、总用电
                    Column {
                        width: parent.width - 68 - 14
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 5
                        // 昵称 + 会员标
                        Row {
                            width: parent.width
                            spacing: 8
                            Text {
                                text: profile.nickname || qsTr("用户")
                                color: "#ffffff"
                                font.pixelSize: Theme.fontSizeTitle; font.bold: true
                                elide: Text.ElideRight
                                width: parent.width - 92
                            }
                            Rectangle {
                                visible: !!profile.level
                                height: 20; width: levelLabel.implicitWidth + 12; radius: 10
                                color: "#FFFFFF33"
                                Text {
                                    id: levelLabel
                                    anchors.centerIn: parent
                                    text: root.levelText(profile.level)
                                    color: root.levelColor(profile.level)
                                    font.pixelSize: 10; font.bold: true
                                }
                            }
                        }
                        // 账号（手机号）
                        Row {
                            spacing: 6
                            Text {
                                text: qsTr("账号：")
                                color: "#E6F0FF"; font.pixelSize: Theme.fontSizeTiny
                            }
                            Text {
                                text: profile.phone || ""
                                color: "#ffffff"; font.pixelSize: Theme.fontSizeTiny; font.bold: true
                            }
                        }
                        // 总用电量
                        Row {
                            spacing: 6
                            Text {
                                text: qsTr("累计用电：")
                                color: "#E6F0FF"; font.pixelSize: Theme.fontSizeTiny
                            }
                            Text {
                                text: Number(root.totalKwh || 0).toFixed(1) + " kWh"
                                color: "#ffffff"; font.pixelSize: Theme.fontSizeTiny; font.bold: true
                            }
                        }
                    }
                }
            }

            // ========== ② 会员中心入口 ==========
            Rectangle {
                width: parent.width - 32; x: 16
                height: memberRow.implicitHeight + 60
                radius: Theme.radiusSmall
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#FFF8E1" }
                    GradientStop { position: 1.0; color: "#FFECB3" }
                }
                border.color: "#FFD54F55"

                Rectangle {
                    id: memberRow
                    anchors.fill: parent
                    anchors.leftMargin: 16; anchors.rightMargin: 16
                    // 左侧：皇冠
                    Text {
                        id: crownTxt
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: "\u{1F451}"; font.pixelSize: 32
                    }
                    // 右侧：箭头
                    Text {
                        id: arrowTxt
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "›"; color: "#8D6E63"; font.pixelSize: 28
                    }
                    // 中间：文字区（左贴皇冠右贴箭头，自动适配）
                    Column {
                        anchors.left: crownTxt.right; anchors.leftMargin: 12
                        anchors.right: arrowTxt.left; anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 3
                        Row {
                            width: parent.width
                            spacing: 6
                            Text {
                                text: qsTr("会员中心")
                                color: "#5D4037"; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                            }
                            Rectangle {
                                visible: !!(curPlan && curPlan.status) && curPlan.status === "active"
                                height: 18; width: daysLbl.implicitWidth + 10; radius: 9
                                color: "#FF9800"
                                Text {
                                    id: daysLbl; anchors.centerIn: parent
                                    text: qsTr("剩 ") + ((curPlan && curPlan.days_left) || 0) + qsTr(" 天")
                                    color: "#ffffff"; font.pixelSize: 10; font.bold: true
                                }
                            }
                        }
                        Text {
                            text: ((curPlan && curPlan.name) || qsTr("点击开通会员"))
                                  + " · " + ((curPlan && curPlan.description) || qsTr("享充电折扣、积分倍增"))
                            color: "#8D6E63"; font.pixelSize: Theme.fontSizeTiny
                            elide: Text.ElideRight; width: parent.width
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.stackView.push("qrc:/UserClient/qml/pages/MemberCenterPage.qml")
                }
            }

            // ========== ③ 账户栏：余额 / 积分 / 优惠券 + 充值按钮 ==========
            Rectangle {
                width: parent.width - 32; x: 16
                height: walletCol.implicitHeight + 20
                color: Theme.card
                radius: Theme.radiusSmall
                border.color: Theme.border
                Column {
                    id: walletCol
                    width: parent.width
                    anchors.left: parent.left; anchors.leftMargin: 14
                    anchors.right: parent.right; anchors.rightMargin: 14
                    anchors.top: parent.top; anchors.topMargin: 14
                    spacing: 14
                    // 标题
                    Text { text: qsTr("我的账户"); font.pixelSize: Theme.fontSizeSmall + 1; font.bold: true; color: Theme.textPrimary }
                    // 三张卡片 + 一个按钮
                    Row {
                        id: walletRow
                        width: parent.width
                        spacing: 8
                        readonly property int cardW: (width - spacing * 3 - 88) / 3
                        // 余额卡
                        AccountCard {
                            cardWidth: walletRow.cardW
                            icon: "\u{1F4B3}"
                            topLabel: qsTr("余额")
                            amountText: "\u{FFE5}" + Number(profile.balance || 0).toFixed(2)
                            subLabel: qsTr("点击充值")
                            c: Theme.danger
                            onClicked: rechargeDlg.open()
                        }
                        // 积分卡
                        AccountCard {
                            cardWidth: walletRow.cardW
                            icon: "\u{2B50}"
                            topLabel: qsTr("积分")
                            amountText: String(profile.points || 0)
                            subLabel: qsTr("查看明细")
                            c: Theme.warn
                            onClicked: root.stackView.push("qrc:/UserClient/qml/pages/PointsDetailPage.qml")
                        }
                        // 优惠券卡
                        AccountCard {
                            cardWidth: walletRow.cardW
                            icon: "\u{1F3AB}"
                            topLabel: qsTr("优惠券")
                            amountText: String(couponsData.length)
                            subLabel: qsTr("查看卡包")
                            c: Theme.success
                            onClicked: root.stackView.push("qrc:/UserClient/qml/pages/CouponDetailPage.qml")
                        }
                        // 充值按钮
                        Rectangle {
                            width: 88; height: 80; radius: Theme.radiusSmall
                            color: Theme.primary
                            Column {
                                anchors.centerIn: parent
                                spacing: 4
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "+"; color: "#ffffff"
                                    font.pixelSize: 24; font.bold: true
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: qsTr("充值")
                                    color: "#ffffff"
                                    font.pixelSize: Theme.fontSizeSmall; font.bold: true
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: rechargeDlg.open()
                            }
                        }
                    }
                }
            }

            // ========== ④ 订单栏：全部订单 / 已预约 / 待支付 / 开发票 ==========
            Rectangle {
                width: parent.width - 32; x: 16
                height: orderCol.implicitHeight + 20
                color: Theme.card
                radius: Theme.radiusSmall
                border.color: Theme.border
                Column {
                    id: orderCol
                    width: parent.width
                    anchors.left: parent.left; anchors.leftMargin: 14
                    anchors.right: parent.right; anchors.rightMargin: 14
                    anchors.top: parent.top; anchors.topMargin: 14
                    spacing: 14
                    // 标题 + 查看全部
                    Rectangle {
                        width: parent.width
                        height: orderTitleLbl.implicitHeight
                        Text {
                            id: orderTitleLbl
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("我的订单"); font.pixelSize: Theme.fontSizeSmall + 1; font.bold: true; color: Theme.textPrimary
                        }
                        Text {
                            id: orderAllLbl
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("查看全部 ›")
                            color: Theme.primary; font.pixelSize: Theme.fontSizeTiny; font.bold: true
                            MouseArea {
                                anchors.fill: parent; anchors.margins: -8
                                onClicked: root.pushOrders("all")
                            }
                        }
                    }
                    // 四个入口
                    Row {
                        id: orderEntryRow
                        width: parent.width
                        spacing: 8
                        readonly property int entryW: (width - spacing * 3) / 4
                        OrderEntry {
                            entryWidth: orderEntryRow.entryW
                            icon: "\u{1F5D2}"
                            label: qsTr("全部订单")
                            onClicked: root.pushOrders("all")
                        }
                        OrderEntry {
                            entryWidth: orderEntryRow.entryW
                            icon: "\u{23F0}"
                            label: qsTr("已预约")
                            onClicked: root.pushOrders("reserved")
                        }
                        OrderEntry {
                            entryWidth: orderEntryRow.entryW
                            icon: "\u{1F4B0}"
                            label: qsTr("待支付")
                            badge: root.countStatus("pending_settle")
                            onClicked: root.pushOrders("pending_settle")
                        }
                        OrderEntry {
                            entryWidth: orderEntryRow.entryW
                            icon: "\u{1F4C4}"
                            label: qsTr("开发票")
                            onClicked: root.stackView.push("qrc:/UserClient/qml/pages/InvoicePage.qml")
                        }
                    }
                }
            }

            // ========== ⑤ 占位：充电画像 + 我的车辆（后续扩展） ==========
            Rectangle {
                width: parent.width - 32; x: 16
                height: portraitCol.implicitHeight + 20
                color: Theme.card
                radius: Theme.radiusSmall
                border.color: Theme.border
                Column {
                    id: portraitCol
                    width: parent.width - 24
                    anchors.left: parent.left; anchors.leftMargin: 12
                    anchors.right: parent.right; anchors.rightMargin: 12
                    anchors.top: parent.top; anchors.topMargin: 12
                    spacing: 10
                    Text { text: qsTr("充电画像 · 我的车辆"); font.pixelSize: Theme.fontSizeSmall + 1; font.bold: true; color: Theme.textPrimary }
                    Text {
                        text: qsTr("（待后续补充内容）")
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                    }
                }
            }

            // 底部说明（占位）
            Text {
                width: parent.width - 32; x: 16
                text: qsTr("数据为示例，后续接入服务端实时同步")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeTiny
                horizontalAlignment: Text.AlignHCenter
                topPadding: 4
            }
        }
    }

    // ========== 辅助：跳转订单页 ==========
    function pushOrders(status) {
        root.stackView.push("qrc:/UserClient/qml/pages/OrderPage.qml",
                            { initialStatus: status })
    }
    function countStatus(s) {
        var n = 0
        var list = UserData.orders()
        for (var i = 0; i < list.length; i++)
            if (list[i].status === s) n++
        return n
    }

    // ========== 充值弹窗（复用原有） ==========
    Dialog {
        id: rechargeDlg
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(320, Overlay.overlay ? Overlay.overlay.width - 64 : 320)
        padding: 20
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { radius: Theme.radius; color: Theme.card }

        Column {
            spacing: 16
            Text { text: qsTr("余额充值"); font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.textPrimary }
            Text { text: qsTr("选择或输入金额，模拟支付成功后余额实时增加"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall; wrapMode: Text.Wrap }

            Row { spacing: 10
                Repeater {
                    model: [50, 100, 200]
                    delegate: Rectangle {
                        width: 72; height: 40; radius: Theme.radiusSmall
                        color: amountInput.text === String(modelData) ? Theme.primary : Theme.background
                        border.color: amountInput.text === String(modelData) ? Theme.primary : Theme.border
                        Text {
                            anchors.centerIn: parent
                            text: "\u{FFE5}" + modelData
                            color: amountInput.text === String(modelData) ? "#ffffff" : Theme.textPrimary
                            font.bold: true
                        }
                        MouseArea { anchors.fill: parent; onClicked: amountInput.text = String(modelData) }
                    }
                }
            }

            Rectangle {
                width: parent.width; height: 44
                color: Theme.background; border.color: Theme.border; radius: Theme.radiusSmall
                TextField {
                    id: amountInput
                    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
                    text: "100"
                    verticalAlignment: Text.AlignVCenter
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    placeholderText: qsTr("请输入金额")
                    color: Theme.textPrimary
                    background: Rectangle { color: "transparent" }
                }
            }

            Row {
                width: parent.width
                spacing: 12
                layoutDirection: Qt.RightToLeft
                Button {
                    text: qsTr("确认充值")
                    onClicked: {
                        var a = Number(amountInput.text)
                        if (UserData.recharge(a)) {
                            rechargeDlg.close()
                            showToast(qsTr("充值成功 +") + a + qsTr(" 元"))
                        } else {
                            showToast(qsTr("请输入有效金额"))
                        }
                    }
                    background: Rectangle { radius: Theme.radiusSmall; color: Theme.primary }
                    contentItem: Text { text: qsTr("确认充值"); color: "#ffffff"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
                Button {
                    text: qsTr("取消")
                    onClicked: rechargeDlg.close()
                    background: Rectangle { radius: Theme.radiusSmall; color: Theme.background; border.color: Theme.border }
                    contentItem: Text { text: qsTr("取消"); color: Theme.textPrimary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
            }
        }
    }

    // 轻提示
    Rectangle {
        id: toast; visible: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 24
        width: Math.min(toastText.implicitWidth + 32, root.width - 48)
        height: 40; radius: 20; color: "#B3000000"; z: 20
        Text { id: toastText; anchors.centerIn: parent; color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall }
    }
    Timer { id: toastTimer; interval: 2000; onTriggered: toast.visible = false }
    function showToast(msg) { toastText.text = msg; toast.visible = true; toastTimer.restart() }

    // ========================= 复用组件 =========================

    // 账户三卡：余额/积分/优惠券
    component AccountCard: Rectangle {
        id: ac
        property int cardWidth: 100
        property string icon
        property string topLabel
        property string amountText
        property string subLabel
        property color c: Theme.primary
        signal clicked()

        width: cardWidth; height: 80
        radius: Theme.radiusSmall
        color: c + "0D"
        border.color: c + "33"
        Column {
            anchors.centerIn: ac
            spacing: 3
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 4
                Text { text: ac.icon || ""; font.pixelSize: 16 }
                Text {
                    text: ac.topLabel || ""
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: ac.amountText || ""
                color: ac.c
                font.pixelSize: Theme.fontSizeSmall + 1; font.bold: true
                elide: Text.ElideMiddle; width: ac.width - 12
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: ac.subLabel || ""
                color: Theme.textSecondary; font.pixelSize: 10
            }
        }
        MouseArea { anchors.fill: ac; onClicked: ac.clicked() }
    }

    // 订单入口 icon+label
    component OrderEntry: Item {
        id: oe
        property int entryWidth: 100
        property string icon
        property string label
        property int badge: 0
        signal clicked()
        width: entryWidth
        height: col.implicitHeight

        Column {
            id: col
            width: parent.width; spacing: 4
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 44; height: 44; radius: 22
                color: Theme.primary + "10"
                Text { anchors.centerIn: parent; text: oe.icon; font.pixelSize: 22 }
                // 未支付红点徽标
                Rectangle {
                    visible: oe.badge > 0
                    anchors.right: parent.right; anchors.top: parent.top
                    anchors.rightMargin: -2; anchors.topMargin: -2
                    height: 18
                    width: Math.max(18, badgeTxt.implicitWidth + 8); radius: 9
                    color: Theme.danger
                    Text {
                        id: badgeTxt; anchors.centerIn: parent
                        text: (oe.badge > 99 ? "99+" : String(oe.badge))
                        color: "#ffffff"; font.pixelSize: 10; font.bold: true
                    }
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: oe.label
                color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTiny
            }
        }

        // 放大点击区域（不参与布局，避免 polish 递归）
        MouseArea {
            anchors.fill: parent
            anchors.topMargin: -4; anchors.bottomMargin: -4
            onClicked: oe.clicked()
        }
    }
}
