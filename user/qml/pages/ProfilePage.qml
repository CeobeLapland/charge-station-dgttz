import QtQuick
import QtQuick.Controls
import UserClient

// 「我的」页：头像/昵称/手机号/余额 + 充值/改昵称 + 画像 + 车辆/优惠券/积分 + 功能入口。
// 数据来自 UserData 单例（对齐 user/vehicle/coupon/point_record 等表）。
Item {
    id: root
    property int navIndex: 3
    readonly property var stackView: StackView.view

    // —— 当前用户信息（随 profileChanged 刷新）——
    property var profile: ({})
    readonly property var portrait: UserData.portrait()
    readonly property var vehiclesData: UserData.vehicles()
    readonly property var couponsData: UserData.coupons()
    readonly property var pointsData: UserData.pointRecords()

    property string activeSheet: ""   // "" | vehicles | coupons | points

    function refresh() { root.profile = UserData.profile() }
    Component.onCompleted: refresh()
    Connections {
        target: UserData
        function onProfileChanged() { refresh() }
    }

    function levelText(lv) {
        if (lv === "vip") return qsTr("VIP 会员")
        if (lv === "enterprise") return qsTr("企业会员")
        return qsTr("普通会员")
    }

    // 不透明背景
    Rectangle { anchors.fill: parent; color: Theme.background }

    ScrollView {
        anchors.fill: parent
        clip: true
        Column {
            width: root.width
            spacing: 16

            // 顶部渐变头部
            Rectangle {
                width: parent.width
                height: 176
                color: Theme.primary
                // 头像 + 昵称 + 手机号 + 等级
                Row {
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 20; anchors.rightMargin: 20
                    spacing: 14

                    // 头像（灰色占位圆）
                    Rectangle {
                        width: 60; height: 60; radius: 30; color: "#FFFFFF"
                        Text {
                            anchors.centerIn: parent
                            text: profile.avatar_path ? "" : "\u{1F464}"
                            font.pixelSize: 34
                        }
                    }

                    Column {
                        width: parent.width - 74
                        spacing: 4
                        Row {
                            spacing: 8
                            Text {
                                text: profile.nickname || qsTr("用户")
                                color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true
                            }
                            Rectangle {
                                visible: !!profile.level && profile.level !== "normal"
                                height: 20; width: levelTxt.implicitWidth + 12; radius: 10
                                color: "#FFFFFF" + "33"
                                Text {
                                    id: levelTxt
                                    anchors.centerIn: parent
                                    text: levelText(profile.level)
                                    color: "#ffffff"; font.pixelSize: 10; font.bold: true
                                }
                            }
                        }
                        Text {
                            text: profile.phone || ""
                            color: "#E6F0FF"; font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }

                // 改昵称入口（头部右上）
                Text {
                    anchors.right: parent.right; anchors.top: parent.top
                    anchors.rightMargin: 20; anchors.topMargin: 16
                    text: qsTr("编辑")
                    color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall
                    MouseArea { anchors.fill: parent; onClicked: nicknameDlg.open() }
                }
            }

            // 余额卡片
            Rectangle {
                width: parent.width - 32
                x: 16
                height: 72
                color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
                y: -40
                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 16; anchors.rightMargin: 16
                    spacing: 16
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        Text { text: qsTr("账户余额"); font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary }
                        Row {
                            spacing: 2
                            Text { text: "\u{FFE5}"; font.pixelSize: Theme.fontSizeBase; color: Theme.textPrimary }
                            Text {
                                text: Number(profile.balance || 0).toFixed(2)
                                font.pixelSize: Theme.fontSizeLarge; font.bold: true; color: Theme.textPrimary
                            }
                        }
                    }
                    Item { width: 1; height: 1 }
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter; anchors.right: parent.right
                        width: 96; height: 36; radius: 18; color: Theme.primary
                        Text { anchors.centerIn: parent; text: qsTr("充值"); color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeSmall }
                        MouseArea { anchors.fill: parent; onClicked: rechargeDlg.open() }
                    }
                }
            }

            // 快捷统计：积分 / 优惠券 / 车辆
            Row {
                width: parent.width - 32
                x: 16
                height: 64
                spacing: 12
                StatCard { label: qsTr("积分"); value: String(profile.points || 0); onClicked: root.activeSheet = "points" }
                StatCard { label: qsTr("优惠券"); value: String(couponsData.length); onClicked: root.activeSheet = "coupons" }
                StatCard { label: qsTr("车辆"); value: String(vehiclesData.length); onClicked: root.activeSheet = "vehicles" }
            }

            // 充电画像
            GroupCard {
                title: qsTr("我的充电画像")
                Column {
                    width: parent.width; spacing: 8
                    PortraitRow { k: qsTr("月均充电次数"); v: Number(portrait.month_avg_count || 0).toFixed(1) + " 次" }
                    PortraitRow { k: qsTr("平均单次充电量"); v: Number(portrait.avg_energy_kwh || 0).toFixed(1) + " kWh" }
                    PortraitRow { k: qsTr("最常到访站"); v: portrait.favorite_station || "-" }
                    PortraitRow { k: qsTr("常用时段"); v: portrait.usual_hours || "-" }
                    PortraitRow { k: qsTr("偏好类型"); v: portrait.prefer_type || "-" }
                }
            }

            // 功能入口
            GroupCard {
                title: qsTr("功能")
                Column {
                    width: parent.width
                    Repeater {
                        model: [
                            { label: qsTr("我的订单"), icon: "\u{1F5D2}", act: "orders" },
                            { label: qsTr("我的优惠券"), icon: "\u{1F3AB}", act: "coupons" },
                            { label: qsTr("我的车辆"), icon: "\u{1F697}", act: "vehicles" },
                            { label: qsTr("积分明细"), icon: "\u{2B50}", act: "points" },
                            { label: qsTr("客服与工单"), icon: "\u{1F4DE}", act: "service" },
                            { label: qsTr("设置"), icon: "\u{2699}\u{FE0F}", act: "settings" }
                        ]
                        delegate: Rectangle {
                            width: parent.width; height: 48
                            color: "transparent"
                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 4; anchors.rightMargin: 4
                                spacing: 12
                                Text { anchors.verticalCenter: parent.verticalCenter; text: modelData.icon; font.pixelSize: 20 }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.label
                                    font.pixelSize: Theme.fontSizeBase; color: Theme.textPrimary
                                }
                                Item { width: 1; height: 1 }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter; anchors.right: parent.right
                                    text: "›"; font.pixelSize: 22; color: Theme.textSecondary
                                }
                            }
                            MouseArea { anchors.fill: parent; onClicked: root.onMenu(modelData.act) }
                            Rectangle {
                                anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right
                                height: 1; color: Theme.border
                            }
                        }
                    }
                }
            }

            Text { width: parent.width - 32; x: 16; text: qsTr("余额/积分/订单均为示例数据，后续接入服务端"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny; horizontalAlignment: Text.AlignHCenter }
        }
    }

    // 右上角设置图标（保留原有入口，兼容旧习惯）
    Text {
        anchors.top: parent.top; anchors.right: parent.right
        anchors.topMargin: 16; anchors.rightMargin: 16
        text: "⚙️"; font.pixelSize: 24; color: "#ffffff"
        MouseArea { anchors.fill: parent; onClicked: root.stackView.push("qrc:/UserClient/qml/pages/SettingsPage.qml") }
    }

    function onMenu(act) {
        if (act === "orders")    root.stackView.push("qrc:/UserClient/qml/pages/OrderPage.qml")
        else if (act === "vehicles") root.activeSheet = "vehicles"
        else if (act === "coupons")  root.activeSheet = "coupons"
        else if (act === "points")   root.activeSheet = "points"
        else if (act === "settings") root.stackView.push("qrc:/UserClient/qml/pages/SettingsPage.qml")
        else if (act === "service")  showToast(qsTr("客服与工单（增强）待接入"))
    }

    // —— 底部列表 sheet（车辆 / 优惠券 / 积分）——
    Rectangle {
        visible: activeSheet !== ""
        anchors.fill: parent
        z: 10
        color: "#66000000"
        MouseArea { anchors.fill: parent; onClicked: root.activeSheet = "" }
    }
    Rectangle {
        visible: activeSheet !== ""
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: root.height * 0.6
        z: 11
        color: Theme.background; radius: Theme.radius

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Row {
                width: parent.width
                Text {
                    text: (activeSheet === "vehicles") ? qsTr("我的车辆")
                        : (activeSheet === "coupons") ? qsTr("我的优惠券") : qsTr("积分明细")
                    font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.textPrimary
                }
                Item { width: 1; height: 1 }
                Text {
                    anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    text: "✕"; font.pixelSize: 20; color: Theme.textSecondary
                    MouseArea { anchors.fill: parent; onClicked: root.activeSheet = "" }
                }
            }

            ListView {
                width: parent.width; height: parent.height - 64
                clip: true
                model: (activeSheet === "vehicles") ? vehiclesData
                     : (activeSheet === "coupons") ? couponsData : pointsData

                delegate: Rectangle {
                    width: ListView.view.width
                    height: (activeSheet === "vehicles") ? 64 : (activeSheet === "coupons") ? 72 : 40
                    color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border

                    // 车辆
                    Loader {
                        active: root.activeSheet === "vehicles"
                        anchors.fill: parent; anchors.margins: 12
                        sourceComponent: Component {
                            Row {
                                spacing: 12
                                Text { anchors.verticalCenter: parent.verticalCenter; text: "\u{1F697}"; font.pixelSize: 22 }
                                Column {
                                    width: parent.width - 120
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2
                                    Text { text: modelData.name || ""; font.bold: true; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall }
                                    Text { text: typeText(modelData.type) + " · " + (modelData.battery_kwh || 0) + " kWh · " + connText(modelData.connector_type); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
                                }
                                Text {
                                    visible: !!modelData.is_default
                                    anchors.verticalCenter: parent.verticalCenter; anchors.right: parent.right
                                    text: qsTr("默认"); color: Theme.primary; font.pixelSize: Theme.fontSizeTiny; font.bold: true
                                }
                            }
                        }
                    }
                    // 优惠券
                    Loader {
                        active: root.activeSheet === "coupons"
                        anchors.fill: parent; anchors.margins: 12
                        sourceComponent: Component {
                            Row {
                                spacing: 12
                                Rectangle {
                                    width: 44; height: 44; radius: Theme.radiusSmall
                                    color: (modelData.status === "unused") ? Theme.primary : Theme.border
                                    Text {
                                        anchors.centerIn: parent
                                        text: "\u{FFE5}" + (modelData.discount_amount || 0)
                                        color: (modelData.status === "unused") ? "#ffffff" : Theme.textSecondary
                                        font.bold: true; font.pixelSize: Theme.fontSizeBase
                                    }
                                }
                                Column {
                                    width: parent.width - 100
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2
                                    Text { text: modelData.title || ""; font.bold: true; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall }
                                    Text { text: (modelData.scope || "") + " · " + (modelData.valid_until || ""); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter; anchors.right: parent.right
                                    text: couponStatusText(modelData.status)
                                    color: (modelData.status === "unused") ? Theme.success
                                         : (modelData.status === "used") ? Theme.textSecondary : Theme.warn
                                    font.pixelSize: Theme.fontSizeTiny; font.bold: true
                                }
                            }
                        }
                    }
                    // 积分
                    Loader {
                        active: root.activeSheet === "points"
                        anchors.fill: parent; anchors.margins: 8
                        sourceComponent: Component {
                            Row {
                                spacing: 12
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: (modelData.change >= 0 ? "+" : "") + (modelData.change || 0)
                                    color: (modelData.change >= 0) ? Theme.success : Theme.danger
                                    font.bold: true; font.pixelSize: Theme.fontSizeSmall
                                }
                                Text {
                                    width: parent.width - 160
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: (modelData.reason === "charge") ? qsTr("充电获得") : qsTr("积分兑换")
                                    color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter; anchors.right: parent.right
                                    text: modelData.create_time || ""; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // —— 充值弹窗 ——
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

    // —— 改昵称弹窗 ——
    Dialog {
        id: nicknameDlg
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(320, Overlay.overlay ? Overlay.overlay.width - 64 : 320)
        padding: 20
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { radius: Theme.radius; color: Theme.card }

        Column {
            spacing: 16
            Text { text: qsTr("修改昵称"); font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.textPrimary }
            Rectangle {
                width: parent.width; height: 44
                color: Theme.background; border.color: Theme.border; radius: Theme.radiusSmall
                TextField {
                    id: nicknameInput
                    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
                    text: profile.nickname || ""
                    maximumLength: 20
                    verticalAlignment: Text.AlignVCenter
                    color: Theme.textPrimary
                    background: Rectangle { color: "transparent" }
                }
            }
            Row {
                width: parent.width
                spacing: 12
                layoutDirection: Qt.RightToLeft
                Button {
                    text: qsTr("保存")
                    onClicked: {
                        if (UserData.updateNickname(nicknameInput.text)) {
                            nicknameDlg.close()
                            showToast(qsTr("昵称已更新"))
                        } else {
                            showToast(qsTr("昵称需 2–20 个字符"))
                        }
                    }
                    background: Rectangle { radius: Theme.radiusSmall; color: Theme.primary }
                    contentItem: Text { text: qsTr("保存"); color: "#ffffff"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
                Button {
                    text: qsTr("取消")
                    onClicked: nicknameDlg.close()
                    background: Rectangle { radius: Theme.radiusSmall; color: Theme.background; border.color: Theme.border }
                    contentItem: Text { text: qsTr("取消"); color: Theme.textPrimary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
            }
        }
    }

    // 轻提示
    Rectangle {
        id: toast
        visible: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 24
        width: Math.min(toastText.implicitWidth + 32, root.width - 48)
        height: 40; radius: 20
        color: "#B3000000"
        z: 20
        Text { id: toastText; anchors.centerIn: parent; color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall }
    }
    Timer { id: toastTimer; interval: 2000; onTriggered: toast.visible = false }
    function showToast(msg) { toastText.text = msg; toast.visible = true; toastTimer.restart() }

    function typeText(t) {
        if (t === "car") return qsTr("乘用车")
        if (t === "light_truck") return qsTr("轻商")
        if (t === "two_wheeler") return qsTr("两轮")
        if (t === "three_wheeler") return qsTr("三轮")
        return t
    }
    function connText(c) {
        if (c === "ac_gb") return qsTr("国标交流")
        if (c === "dc_gb") return qsTr("国标直流")
        return qsTr("其他")
    }
    function couponStatusText(s) {
        if (s === "unused") return qsTr("未使用")
        if (s === "used") return qsTr("已使用")
        if (s === "expired") return qsTr("已过期")
        return s
    }

    // —— 复用小组件 ——
    component StatCard: Rectangle {
        property string label
        property string value
        signal clicked()
        Layout.fillWidth: true
        width: (parent ? (parent.width - 24) / 3 : 100)
        height: 64
        color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
        Column {
            anchors.centerIn: parent; spacing: 2
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: parent.parent.value; font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.primary }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: parent.parent.label; font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary }
        }
        MouseArea { anchors.fill: parent; onClicked: parent.clicked() }
    }
    component GroupCard: Rectangle {
        property string title
        width: parent.width - 32
        x: 16
        height: cardCol.implicitHeight + 24
        color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
        Column {
            id: cardCol
            width: parent.width - 24
            anchors.left: parent.left; anchors.leftMargin: 12
            anchors.top: parent.top; anchors.topMargin: 12
            spacing: 8
            Text { text: parent.parent.title; font.pixelSize: Theme.fontSizeSmall + 1; font.bold: true; color: Theme.textPrimary }
        }
    }
    component PortraitRow: Row {
        property string k
        property string v
        width: parent.width
        spacing: 6
        Text { text: parent.k + "："; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
        Text { text: parent.v; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTiny; font.bold: true }
    }
}