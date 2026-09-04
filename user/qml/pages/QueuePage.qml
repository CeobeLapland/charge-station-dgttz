import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UserClient

// 排队页：电站暂时无空闲桩时进入。展示排队序号/预计等待 + 站内「充电中」与「预约队列」列表。
// 队列安排由服务端(ChargingFlow mock)负责；轮到你( reservationReady )时跳转到扫码页。
Item {
    id: root
    readonly property var stackView: StackView.view
    readonly property var st: ExploreData.stationById(ChargingFlow.flow.station_id) || ({})

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
            Text { text: qsTr("排队中"); color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true }
        }
    }

    // 轮到你 → 跳扫码
    Connections {
        target: ChargingFlow
        function onReservationReady(message) {
            stackView.pop()
            stackView.push("qrc:/UserClient/qml/pages/ScanPage.qml",
                           { stationId: Number(ChargingFlow.flow.station_id) })
            toast.text = message
        }
    }
    Connections {
        target: ChargingFlow
        function onAbnormal(title, sub) {
            if (ChargingFlow.phase === "cancelled") { root.stackView.pop(); return }
            toast.text = title + "：" + sub
        }
    }

    ListView {
        anchors.top: parent.top; anchors.topMargin: 92
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true; spacing: 14; topMargin: 4; bottomMargin: 24

        model: root.rows()
        delegate: Rectangle {
            width: parent.width - 32; x: 16
            height: delCol.implicitHeight + 20
            radius: Theme.radiusSmall; color: Theme.card; border.color: Theme.border
            Column {
                id: delCol
                width: parent.width - 24; x: 12; y: 10; spacing: 8

                // 我 —— 排队状态大卡
                Item {
                    visible: modelData.kind === "me"
                    width: parent.width
                    height: meCol.implicitHeight
                    Column {
                        id: meCol
                        width: parent.width; spacing: 6
                        Row {
                            width: parent.width
                            Text { text: qsTr("我的排队"); font.bold: true; font.pixelSize: Theme.fontSizeTitle; color: Theme.primary }
                            Item { Layout.fillWidth: true; width: 1; height: 1 }
                            Row { spacing: 2
                                Text { text: qsTr("序号 "); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall }
                                Text { text: String(ChargingFlow.flow.queue_no || "—")
                                       font.bold: true; font.pixelSize: Theme.fontSizeTitle; color: Theme.primary }
                            }
                        }
                        Text { text: qsTr("预计等待 ") + (ChargingFlow.flow.estimate_wait_min || 0) + qsTr(" 分钟")
                             ; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
                        Text { text: qsTr("站内 ") + (st.total_chargers || 0) + qsTr(" 台桩，轮到你时将即时通知并跳转扫码。")
                             ; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny; wrapMode: Text.Wrap
                             ; width: parent.width }
                    }
                }

                // 站内列表项
                Item {
                    visible: modelData.kind === "citem"
                    width: parent.width
                    height: citemRow.implicitHeight
                    Row {
                        id: citemRow
                        width: parent.width
                        Text { text: modelData.left; font.bold: true; font.pixelSize: Theme.fontSizeSmall; color: Theme.textPrimary }
                        Item { Layout.fillWidth: true; width: 1; height: 1 }
                        Chip { text: modelData.right }
                    }
                }
            }
        }
    }

    function rows() {
        var n = Number(ChargingFlow.flow.queue_no || 1)
        var out = [{ kind: "me" }]
        out.push({ kind: "citem", left: qsTr("充电中"), right: qsTr("A-03 · 45% / A-05 · 88%") })
        var ahead = ""
        for (var i = n + 1; i <= n + 2; i++) ahead += (ahead ? " / " : "") + qsTr("序号 ") + i
        out.push({ kind: "citem", left: qsTr("预约队列"), right: ahead })
        return out
    }

    // 底部按钮
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 84; color: "#00000000"
        Rectangle {
            width: parent.width - 32; x: 16; height: 52; radius: 26
            color: Theme.background; border.color: Theme.danger; border.width: 1
            Text { anchors.centerIn: parent; text: qsTr("取消排队"); color: Theme.danger; font.bold: true; font.pixelSize: Theme.fontSizeBase }
            MouseArea { anchors.fill: parent; onClicked: { ChargingFlow.cancel(); root.stackView.pop() } }
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

    component Chip: Rectangle {
        property string text
        height: 22; width: chipTxt.implicitWidth + 14; radius: 11
        color: Theme.background; border.color: Theme.border; border.width: 1
        Text { id: chipTxt; anchors.centerIn: parent; text: parent.text; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
    }
}