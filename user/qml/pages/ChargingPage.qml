import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UserClient

// 充电过程页：实时面板。进度由 ChargingFlow(mock 服务端) 驱动 push.order_progress。
// 充电曲线两段式：0–80% 满功率、80% 后降速（mockProgressTick 内实现）。
Item {
    id: root
    readonly property var stackView: StackView.view
    readonly property var f: ChargingFlow.flow

    Rectangle { anchors.fill: parent; color: Theme.background }

    // 顶部装饰
    Rectangle {
        width: parent.width; height: 120
        gradient: Gradient { GradientStop { position: 0; color: Theme.primary } GradientStop { position: 1; color: "#0a5bbf" } }
    }

    // 返回 + 标题
    Row {
        anchors.top: parent.top; anchors.topMargin: 8
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 8; anchors.rightMargin: 20; spacing: 8
        Text {
            text: "‹"; font.pixelSize: 28; color: "#ffffff"
            MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: root.stackView.pop() }
        }
        Column { spacing: 2
            Text { text: qsTr("充电中"); color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true }
            Text { text: f.station_name + " · " + f.charger_code; color: "#D6E7FF"; font.pixelSize: Theme.fontSizeTiny }
            Text { text: f.station_address || ""; color: "#B9CFF2"; font.pixelSize: Theme.fontSizeTiny; elide: Text.ElideRight }
        }
    }

    // —— 仪表盘 ——
    Item {
        anchors.top: parent.top; anchors.topMargin: 96
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(root.width - 48, 300); height: Math.min(root.width - 48, 300)

        Canvas {
            id: gaugeBg
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.lineWidth = 18
                ctx.lineCap = "round"
                var w = width, h = height
                var c = { x: w/2, y: h/2 }
                var r = Math.min(w, h)/2 - 20
                // 底环
                ctx.strokeStyle = "#E6ECF5"
                ctx.beginPath(); ctx.arc(c.x, c.y, r, 0, Math.PI*2); ctx.stroke()
                // 进度环
                var frac = (Number(f.soc) - Number(f.start_soc)) / Math.max(1, Number(f.target_soc) - Number(f.start_soc))
                frac = Math.max(0, Math.min(1, frac))
                var a0 = -Math.PI/2
                var a1 = a0 + frac * Math.PI * 2
                var grad = ctx.createLinearGradient(0,0,w,h)
                grad.addColorStop(0, "#3d9bff"); grad.addColorStop(1, "#12c8ff")
                ctx.strokeStyle = grad
                ctx.beginPath(); ctx.arc(c.x, c.y, r, a0, a1); ctx.stroke()
            }
        }
        Column {
            anchors.centerIn: parent
            spacing: 2
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Number(f.soc || 0).toFixed(0) + "%"
                font.pixelSize: 52; font.bold: true; color: Theme.textPrimary
            }
            Text { anchors.horizontalCenter: parent.horizontalCenter
                   text: qsTr("当前电量 → 目标 ") + Number(f.target_soc || 0) + "%"
                   color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
        }
        // 完成态小徽标
        Rectangle {
            anchors.bottom: parent.bottom; anchors.right: parent.right
            visible: Number(f.soc) >= Number(f.target_soc)
            width: 66; height: 24; radius: 12; color: Theme.success + "1A"
            border.color: Theme.success; border.width: 1
            Text { anchors.centerIn: parent; text: "目标达成"; color: Theme.success; font.pixelSize: Theme.fontSizeTiny; font.bold: true }
        }
    }

    // —— 数据行 ——
    Row {
        anchors.top: parent.top; anchors.topMargin: 96 + Math.min(root.width - 48, 300) + 8
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 20; anchors.rightMargin: 20
        Item { width: 1; height: 1 }
        Stat { k: qsTr("功率"); v: Number(f.power_kw || 0).toFixed(1) + "kW" }
        Stat { k: qsTr("电量"); v: Number(f.energy_kwh || 0).toFixed(1) + "kWh" }
        Stat { k: qsTr("费用"); v: "\u{FFE5}" + Number(f.cost || 0).toFixed(2) }
        Stat { k: qsTr("时长"); v: (Number(f.duration_min || 0).toFixed(1)) + "分" }
    }

    // —— 曲线提示 ——
    Rectangle {
        anchors.top: parent.top; anchors.topMargin: 96 + Math.min(root.width - 48, 300) + 72
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 20; anchors.rightMargin: 20
        height: 60; radius: Theme.radiusSmall; color: Theme.card; border.color: Theme.border
        Column { anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; spacing: 2
            Text { text: qsTr("充电曲线"); font.pixelSize: Theme.fontSizeSmall; font.bold: true; color: Theme.textPrimary }
            Text { text: Number(f.soc) <= 80 ? qsTr("0–80% 快充功率：提速中…") : qsTr("已过 80%，功率下降以保护电池")
                   ; font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary }
        }
    }

    // —— 订单信息 ——
    Rectangle {
        anchors.top: parent.top; anchors.topMargin: 96 + Math.min(root.width - 48, 300) + 72 + 68
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 20; anchors.rightMargin: 20
        height: infoCol.implicitHeight + 20; radius: Theme.radiusSmall; color: Theme.card; border.color: Theme.border
        Column {
            id: infoCol
            anchors.left: parent.left; anchors.leftMargin: 14; anchors.top: parent.top; anchors.topMargin: 10
            width: parent.width - 28; spacing: 6
            InfoRow { k: qsTr("车辆"); v: f.vehicle_name + " · " + (f.speed === "fast" ? qsTr("快充") : qsTr("慢充")) }
            InfoRow { k: qsTr("订单号"); v: "#" + f.order_id }
            InfoRow { k: qsTr("开始时间"); v: f.reserved_time }
            InfoRow { k: qsTr("单价"); v: "\u{FFE5}" + Number(f.unit_price || 0).toFixed(2) + "/kWh（含服务费）" }
        }
    }

    // —— 底部操作 ——
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 96; color: "#00000000"
        Row { anchors.centerIn: parent; spacing: 14
            Rectangle {
                width: 140; height: 50; radius: 25
                color: Theme.background; border.color: Theme.warn; border.width: 1
                Text { anchors.centerIn: parent; text: qsTr("结束充电"); color: Theme.warn; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: { ChargingFlow.finishCharging() } }
            }
            Rectangle {
                width: 140; height: 50; radius: 25; color: "#F4F5F7"
                border.color: Theme.border; border.width: 1
                Text { anchors.centerIn: parent; text: qsTr("演示：中途拔枪"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
                MouseArea { anchors.fill: parent; onClicked: ChargingFlow.simulatePeel() }
            }
        }
    }

    // —— 充电完成/中断 → 结算（用 replace，返回键可直接退出）——
    function settleNav() {
        if (ChargingFlow.phase !== "settle") return
        stackView.replace("qrc:/UserClient/qml/pages/SettlePage.qml")
    }
    Connections {
        target: ChargingFlow
        function onStateChanged() { root.settleNav() }
        function onAbnormal(title, sub) {
            toastText.text = title + "：" + sub; toast.visible = true; toastTimer.restart()
        }
    }

    // 轻提示
    Rectangle {
        id: toast; visible: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 110
        width: Math.min(toastText.implicitWidth + 32, root.width - 48); height: 40; radius: 20
        color: "#B3000000"; z: 30
        Text { id: toastText; anchors.centerIn: parent; text: ""; color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall }
    }
    Timer { id: toastTimer; interval: 2400; onTriggered: toast.visible = false }

    component Stat: Column {
        property string k
        property string v
        width: parent.width / 4 - 4
        spacing: 4
        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: parent.k; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: parent.v; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall; font.bold: true }
    }
    component InfoRow: Row {
        property string k
        property string v
        width: parent.width; spacing: 6
        Text { text: parent.k + "："; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
        Text { text: parent.v; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTiny; elide: Text.ElideRight }
    }
}