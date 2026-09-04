import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UserClient

// 预约页：电站详情「立即预约」的统一入口。填写 时间/车辆/目标电量/快慢 后提交。
// 提交后由 ChargingFlow(mock 服务端) 决策：有空位→直接匹配等待扫码；无空位→进入排队。
Item {
    id: root
    property int stationId: 0
    readonly property var stackView: StackView.view

    readonly property var st: ExploreData.stationById(stationId) || ({})
    readonly property var vehicles: UserData.vehicles()
    readonly property var priceRules: ExploreData.priceRulesForStation(stationId)

    // —— 表单状态（客户端本地，仅驱动本页 UI）——
    property string reserveType: "immediate"   // immediate / timed
    property int vehicleIndex: 0
    property int targetSoc: 80
    property string speed: "fast"              // fast / slow

    function pad2(n) { return (n < 10 ? "0" : "") + n }
    function hhmm(d) { return pad2(d.getHours()) + ":" + pad2(d.getMinutes()) }

    // 定时预约的可选时段（客户端预置，含预计开始时间）
    property var slotItems: [
        { label: qsTr("30 分钟后"), deltaMin: 30 },
        { label: qsTr("1 小时后"),  deltaMin: 60 },
        { label: qsTr("18:00 出发"), deltaMin: 0, fixedHour: 18 }
    ]
    property int slotIndex: 0

    // 所选时间展示（立即 → 现在；定时 → 时段对应时刻）
    property string expectedTimeLabel: {
        if (reserveType === "immediate")
            return qsTr("立即（现在）")
        var sl = slotItems[slotIndex] || {}
        var d = new Date()
        if (sl.fixedHour !== undefined)
            d.setHours(sl.fixedHour, 0, 0, 0)
        else
            d.setTime(d.getTime() + (sl.deltaMin || 0) * 60000)
        return hhmm(d) + "（" + (sl.label || "") + "）"
    }

    // 提交 → ChargingFlow.startCharge（真正决策在 mock 服务端）
    function submit() {
        var v = vehicles[vehicleIndex] || {}
        var expect = ""
        if (reserveType === "timed") {
            var sl = slotItems[slotIndex]
            var d = new Date()
            if (sl.fixedHour !== undefined) { d.setHours(sl.fixedHour, 0, 0, 0) }
            else { d.setTime(d.getTime() + sl.deltaMin * 60000) }
            expect = hhmm(d)
        }
        ChargingFlow.startCharge(stationId, reserveType, expect,
                                 Number(v.id || 0), targetSoc, speed)
        // 触发后根据 mock 服务端决策切换页面
        root.navAfterSubmit()
    }
    function navAfterSubmit() {
        if (ChargingFlow.phase === "scan_pending")
            stackView.push("qrc:/UserClient/qml/pages/ScanPage.qml",
                           { stationId: root.stationId })
        else if (ChargingFlow.phase === "queued")
            stackView.push("qrc:/UserClient/qml/pages/QueuePage.qml")
        else
            showToast("预留失败，请重试")
    }

    Rectangle { anchors.fill: parent; color: Theme.background }

    // 返回 + 标题
    Rectangle {
        width: parent.width; height: 76; color: Theme.primary
        Row {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 8; anchors.rightMargin: 20; spacing: 8
            Text {
                text: "‹"; font.pixelSize: 28; color: "#ffffff"
                MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: root.stackView.pop() }
            }
            Column { spacing: 2
                Text { text: qsTr("立即预约"); color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true }
                Text { text: st.name || ""; color: "#E0ECFF"; font.pixelSize: Theme.fontSizeTiny }
            }
        }
    }

    ScrollView {
        id: sv
        anchors.top: parent.top; anchors.topMargin: 76
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        clip: true
        Column {
            width: sv.width; spacing: 16; leftPadding: 16; rightPadding: 16; topPadding: 16; bottomPadding: 24

            // 电站概览小卡
            Rectangle {
                width: parent.width
                height: cardRow.implicitHeight + 24
                radius: Theme.radiusSmall; color: Theme.card; border.color: Theme.border
                Column {
                    id: cardRow
                    anchors.left: parent.left; anchors.leftMargin: 14; anchors.top: parent.top; anchors.topMargin: 12
                    spacing: 6
                    Text { text: st.name || ""; font.bold: true; font.pixelSize: Theme.fontSizeBase; color: Theme.textPrimary }
                    Text { text: "\u{1F4CD} " + (st.address || ""); font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary; wrapMode: Text.Wrap; width: parent.width - 24 }
                }
            }

            // 预约时间
            Group { title: qsTr("预约时间") }
            Row {
                width: parent.width; spacing: 8
                IconBtn { text: qsTr("立即预约"); selected: root.reserveType === "immediate";
                          onClicked: root.reserveType = "immediate" }
                IconBtn { text: qsTr("定时预约"); selected: root.reserveType === "timed";
                          onClicked: root.reserveType = "timed" }
            }
            Row {   // 所选时间的明确展示
                width: parent.width; spacing: 6
                Text { text: qsTr("预计开始："); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                       anchors.verticalCenter: parent.verticalCenter }
                Text { text: root.expectedTimeLabel; color: Theme.primary; font.bold: true; font.pixelSize: Theme.fontSizeTiny
                       anchors.verticalCenter: parent.verticalCenter }
            }
            Row {   // 时段（定时才有意义）
                visible: root.reserveType === "timed"
                width: parent.width; spacing: 8
                Repeater {
                    model: root.slotItems
                    delegate: Rectangle {
                        width: parent.width / root.slotItems.length - 6
                        height: 34; radius: Theme.radiusSmall
                        color: root.slotIndex === index ? Theme.primary + "1A" : Theme.card
                        border.color: root.slotIndex === index ? Theme.primary : Theme.border
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: modelData.label; font.pixelSize: Theme.fontSizeTiny
                            color: root.slotIndex === index ? Theme.primary : Theme.textSecondary
                            font.bold: root.slotIndex === index
                        }
                        MouseArea { anchors.fill: parent; onClicked: root.slotIndex = index }
                    }
                }
            }

            // 车辆
            Group { title: qsTr("我的车辆") }
            Rectangle {
                width: parent.width; height: 40; radius: Theme.radiusSmall
                color: Theme.card; border.color: Theme.border
                ComboBox {
                    id: vehBox
                    anchors.fill: parent
                    visible: root.vehicles.length > 0
                    model: root.vehicles
                    textRole: "name"
                    currentIndex: root.vehicleIndex
                    onActivated: root.vehicleIndex = currentIndex
                }
                Text {
                    visible: root.vehicles.length === 0
                    anchors.fill: parent
                    text: qsTr("暂无车辆，到「我的」添加后再预约"); color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeTiny; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
            }

            // 目标电量
            Group { title: qsTr("目标电量") }
            Slider {
                width: parent.width
                from: 30; to: 100; stepSize: 5
                value: root.targetSoc
                onValueChanged: root.targetSoc = value
                handle: Rectangle {
                    x: parent.visualPosition * (parent.width - implicitWidth)
                    y: parent.topPadding + parent.availableHeight / 2 - implicitHeight / 2
                    implicitWidth: 22; implicitHeight: 22
                    radius: 11
                    color: Theme.primary; border.color: Theme.card; border.width: 3
                }
                background: Rectangle {
                    width: parent.availableWidth; height: 6; radius: 3
                    color: Theme.border
                }
                Text {
                    anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("充到 ") + root.targetSoc + "%"
                    font.bold: true; color: Theme.primary; font.pixelSize: Theme.fontSizeSmall
                }
            }

            // 充电方式
            Group { title: qsTr("充电方式") }
            Row { width: parent.width; spacing: 8
                IChip { text: qsTr("快充"); sub: qsTr("速度快"); on: root.speed === "fast"; onClicked: root.speed = "fast" }
                IChip { text: qsTr("慢充"); sub: qsTr("更护电池"); on: root.speed === "slow"; onClicked: root.speed = "slow" }
            }

            // 电价提示
            Row {
                width: parent.width; spacing: 4
                Text { text: "💡"; font.pixelSize: 12 }
                Text {
                    text: qsTr("当前分时电价：" ) + priceHint + qsTr(" 元/kWh（已含服务费）")
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny; wrapMode: Text.Wrap
                    width: parent.width - 20
                }
            }

            Text {
                width: parent.width; wrapMode: Text.Wrap
                text: qsTr("提示：定时/排队时系统将在「到点/轮到」时才为您匹配空闲桩，不会提前占桩。")
                color: Theme.warn; font.pixelSize: Theme.fontSizeTiny
            }
        }
    }

    readonly property var priceHint: {
        // 取谷电档展示（错峰心理暗示），首个 value 兜底
        for (var i = 0; i < priceRules.length; i++) {
            if (priceRules[i].level === "valley")
                return (Number(priceRules[i].price || 0) + Number(st.service_fee || 0)).toFixed(2)
        }
        if (priceRules.length > 0)
            return (Number(priceRules[0].price || 0) + Number(st.service_fee || 0)).toFixed(2)
        return "0.80"
    }

    // 底部提交
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 84; color: "#00000000"
        Rectangle {
            width: parent.width - 32; x: 16; height: 52; radius: 26
            gradient: Gradient { GradientStop { position: 0; color: Theme.primary } GradientStop { position: 1; color: Theme.accent } }
            Text { anchors.centerIn: parent; text: qsTr("立即预约"); color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeBase }
            MouseArea { anchors.fill: parent; onClicked: root.submit() }
        }
    }

    // 轻提示
    Rectangle {
        id: toast; visible: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 100
        width: Math.min(toastText.implicitWidth + 32, root.width - 48); height: 40; radius: 20
        color: "#B3000000"; z: 30
        Text { id: toastText; anchors.centerIn: parent; text: ""; color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall }
    }
    Timer { id: toastTimer; interval: 2000; onTriggered: toast.visible = false }
    function showToast(m) { toastText.text = m; toast.visible = true; toastTimer.restart() }

    // —— 小组件 ——
    component Group: Text {
        property string title
        text: title
        font.pixelSize: Theme.fontSizeSmall + 1
        font.bold: true
        color: Theme.textPrimary
    }
    component IconBtn: Rectangle {
        property string text
        property bool selected: false
        height: 34; radius: Theme.radiusSmall
        color: selected ? Theme.primary + "1A" : Theme.card
        border.color: selected ? Theme.primary : Theme.border; border.width: 1
        Text { anchors.centerIn: parent; text: parent.text; font.pixelSize: Theme.fontSizeSmall
               color: parent.selected ? Theme.primary : Theme.textSecondary; font.bold: parent.selected }
        MouseArea { anchors.fill: parent; onClicked: parent.clicked() }
        signal clicked
    }
    component IChip: Rectangle {
        id: chip
        property string text
        property string sub
        property bool on: false
        height: 52; width: (parent.width - 8) / 2; radius: Theme.radiusSmall
        color: on ? Theme.primary + "1A" : Theme.card
        border.color: on ? Theme.primary : Theme.border; border.width: 1
        Column {
            anchors.centerIn: parent; spacing: 2
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: chip.text
                   color: chip.on ? Theme.primary : Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall; font.bold: true }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: chip.sub
                   color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
        }
        MouseArea { anchors.fill: parent; onClicked: parent.clicked() }
        signal clicked
    }
}