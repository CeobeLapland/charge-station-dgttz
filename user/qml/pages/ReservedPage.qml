import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UserClient

// 已预约·待扫码 中间页：预约/排队匹配成功后落在此，展示分配桩号 + 扫码截止倒计时。
// 用户（可能还在去往电站路上）到桩后再手动「去扫码」，避免预约成功就立刻被拉进扫码界面。
Item {
    id: root
    readonly property var stackView: StackView.view

    // 流程上下文：仅当还处于 scan_pending 且有分配桩号时展示有效内容
    readonly property bool active: ChargingFlow.phase === "scan_pending"
                                      && Number(ChargingFlow.flow.station_id) > 0
    readonly property var st: ExploreData.stationById(Number(ChargingFlow.flow.station_id || 0)) || ({})

    function pad2(n) { return (n < 10 ? "0" : "") + n }
    // 扫码剩余时间 → mm:ss
    function mmss(sec) {
        var s = Math.max(0, Number(sec || 0))
        return pad2(Math.floor(s / 60)) + ":" + pad2(s % 60)
    }

    Rectangle { anchors.fill: parent; color: Theme.background }

    // 返回 + 标题
    Rectangle {
        width: parent.width; height: 76; color: Theme.primary
        Row {
            anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 8; anchors.rightMargin: 20; spacing: 8
            Text { text: "\u{2039}"; font.pixelSize: 28; color: "#ffffff"
                   MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: root.stackView.pop() } }
            Text { text: qsTr("已预约"); color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true }
        }
    }

    ScrollView {
        id: reviewScroll
        anchors.top: parent.top; anchors.topMargin: 76
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: bottomBtn.top
        clip: true
        Column {
            width: reviewScroll.width
            leftPadding: 16; rightPadding: 16; topPadding: 16; bottomPadding: 24
            spacing: 16

        // 状态大卡：桩号 + 倒计时
        Rectangle {
            width: parent.width; height: 128; radius: Theme.radiusSmall
            color: Theme.card; border.color: Theme.border; border.width: 1
            Column {
                anchors.centerIn: parent; spacing: 10
                Text {
                    text: active ? qsTr("请在 ") + (ChargingFlow.flow.charger_code || "") + qsTr(" 号桩扫码启动")
                                 : qsTr("暂无有效预约")
                    color: Theme.textPrimary; font.bold: true; font.pixelSize: Theme.fontSizeBase
                }
                Text {
                    text: active ? qsTr("扫码截止 ") + root.mmss(ChargingFlow.flow.scan_remaining_sec)
                                 : qsTr("当前没有待扫码的预约，请返回首页")
                    color: Theme.danger; font.bold: true; font.pixelSize: Theme.fontSizeTitle
                }
                Text {
                    text: active ? qsTr("超时未扫码将自动释放桩并产生违约金") : ""
                    visible: active
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                }
            }
        }

        // 订单摘要
        Rectangle {
            width: parent.width; height: 150; radius: Theme.radiusSmall
            color: Theme.card; border.color: Theme.border; border.width: 1
            Column {
                anchors.top: parent.top; anchors.topMargin: 14
                anchors.left: parent.left; anchors.right: parent.right
                anchors.leftMargin: 14; anchors.rightMargin: 14; spacing: 8
                ColumnRow { label: qsTr("电站"); value: st.name || ChargingFlow.flow.station_name || ""; bold: true }
                ColumnRow { label: qsTr("车辆"); value: active ? (ChargingFlow.flow.vehicle_name || "") : "" }
                ColumnRow { label: qsTr("目标电量"); value: active ? qsTr("充至 ") + Number(ChargingFlow.flow.target_soc || 0) + "%" : "" }
                ColumnRow { label: qsTr("方式"); value: active ? (ChargingFlow.flow.charger_type === "fast" ? qsTr("快充") : qsTr("慢充")) : "" }
            }
        }

        Rectangle { color: "transparent"; height: 16; width: parent.width }
        }
    }

    // 底部按钮：去扫码（主）+ 主动取消（次）
    Rectangle {
        id: bottomBtn
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 122; color: Theme.background
        Rectangle {
            width: parent.width - 32; x: 16; y: 8; height: 52; radius: 26
            gradient: Gradient { GradientStop { position: 0; color: Theme.primary } GradientStop { position: 1; color: Theme.accent } }
            opacity: root.active ? 1 : 0.4
            Text {
                anchors.centerIn: parent
                text: qsTr("到桩啦，去扫码 →"); color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeBase
            }
            MouseArea {
                enabled: root.active
                anchors.fill: parent
                onClicked: root.stackView.push("qrc:/UserClient/qml/pages/ScanPage.qml",
                                               { stationId: Number(ChargingFlow.flow.station_id) })
            }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top; anchors.topMargin: 70
            text: qsTr("取消预约"); color: Theme.danger; font.pixelSize: Theme.fontSizeSmall; font.bold: true
            MouseArea { anchors.fill: parent; anchors.margins: -12
                onClicked: { ChargingFlow.cancel(); root.stackView.pop() } }
        }
    }

    // 扫码超时 / 取消 → 流程结束，退出本页
    Connections {
        target: ChargingFlow
        function onAbnormal(title, sub) {
            if (ChargingFlow.phase === "cancelled" || ChargingFlow.phase === "done")
                root.stackView.pop()
        }
    }

    // 摘要行组件（声明在根级，避免嵌套声明导致的绑定卡死）
    component ColumnRow: Row {
        property string label
        property string value
        property bool bold: false
        width: parent.width; spacing: 6
        Text { text: parent.label; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
        Text { text: parent.value; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall; font.bold: parent.bold }
    }
}