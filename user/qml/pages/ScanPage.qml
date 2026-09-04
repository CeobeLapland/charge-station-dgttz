import QtQuick
import QtQuick.Controls
import UserClient

// 扫码页：本流程内校验「桩码一致性」后启动充电；独立扫码（未进入流程）仅演示识别。
// 目标桩码来自 ChargingFlow.flow（预约/排队匹配后回填）；确认(confirmScan)即 order.start → 充电页。
Item {
    id: root
    readonly property var stackView: StackView.view

    property int stationId: 0       // 预约电站（由 ReservePage / QueuePage 传入）
    property bool scanning: false
    property var scanResult: null   // 识别到的模拟内容

    // 流程内：显示应前往的桩
    readonly property var inFlow: ChargingFlow.phase === "scan_pending" && ChargingFlow.flow.station_id == stationId
    readonly property var expectCode: inFlow ? (ChargingFlow.flow.charger_code || "") : ""

    Rectangle { anchors.fill: parent; color: "#0b1220" }

    // —— 顶部：返回 + 标题 ——
    Rectangle {
        anchors.left: parent.left; anchors.top: parent.top
        width: parent.width; height: 72
        color: "transparent"
        Row {
            anchors.fill: parent
            anchors.leftMargin: 8; anchors.rightMargin: 20
            spacing: 6
            MouseArea {
                width: 36; height: 36
                anchors.verticalCenter: parent.verticalCenter
                onClicked: root.stackView.pop()
                Text { anchors.centerIn: parent; text: "‹"; color: "#ffffff"; font.pixelSize: 30 }
            }
            Text {
                text: qsTr("扫码启动")
                color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // —— 目标桩提示（流程内） ——
    Rectangle {
        visible: root.inFlow && !root.scanning
        anchors.top: parent.top; anchors.topMargin: 84
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 24; anchors.rightMargin: 24
        height: 56; radius: Theme.radiusSmall
        color: "#1A12c8ff"
        Row {
            anchors.centerIn: parent; spacing: 8
            Text { text: "\u{1F4CD}"; font.pixelSize: 16 }
            Text {
                text: qsTr("请到 ") + root.expectCode + qsTr(" 号桩，将二维码对准取景框")
                color: "#9BEEFF"; font.pixelSize: Theme.fontSizeSmall
            }
        }
    }

    // —— 扫描取景框 ——
    Rectangle {
        id: frame
        anchors.top: parent.top; anchors.topMargin: 160
        anchors.horizontalCenter: parent.horizontalCenter
        width: 260; height: 260
        color: "transparent"
        border.color: "#FFFFFF"
        border.width: 2
        radius: 12

        // 扫描线
        Rectangle {
            id: scanLine
            anchors.left: parent.left; anchors.right: parent.right
            y: 8
            height: 2
            color: "#12c8ff"
            opacity: root.scanning ? 1 : 0
            SequentialAnimation on y {
                running: root.scanning
                loops: Animation.Infinite
                PropertyAnimation { from: 8; to: 250; duration: 1600; easing.type: Easing.InOutQuad }
                PropertyAnimation { from: 250; to: 8; duration: 1600; easing.type: Easing.InOutQuad }
            }
        }
    }

    // —— 指示文案 ——
    Column {
        anchors.top: frame.bottom; anchors.topMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 8
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.scanning ? qsTr("正在扫描，请将二维码对准取景框…") :
                  (root.inFlow ? qsTr("扫描后自动校验桩号并启动充电") : qsTr("将二维码放入框内即可自动扫描"))
            color: "#9FB2CC"; font.pixelSize: Theme.fontSizeSmall
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("（示例环境无法调用摄像头，将自动模拟识别并校验）")
            color: "#5B6B84"; font.pixelSize: Theme.fontSizeTiny
        }
    }

    // —— 模拟识别结果 / 校验失败 ——
    Rectangle {
        visible: root.scanResult !== null && root.scanResult.bad !== true
        anchors.bottom: parent.bottom; anchors.bottomMargin: 60
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 24; anchors.rightMargin: 24
        color: Theme.card; radius: Theme.radius
        Column {
            anchors.fill: parent; anchors.margins: 16
            spacing: 12
            Text { text: qsTr("识别成功"); font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.textPrimary }
            Text {
                width: parent.width; wrapMode: Text.Wrap
                text: root.scanResult ? (qsTr("桩码：") + root.scanResult.code + "\n" + qsTr("所在：") + (root.scanResult.station || root.scanResult.code)) : ""
                color: Theme.textSecondary; font.pixelSize: Theme.fontSizeBase
            }
            Row {
                width: parent.width; spacing: 10
                Button {
                    width: (parent.width - parent.spacing) / 2; height: 42
                    text: root.inFlow ? qsTr("重新扫描") : qsTr("继续扫描")
                    onClicked: { autoLaunchTimer.stop(); root.startScan() }
                    contentItem: Text { text: parent.text; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeBase; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: Theme.background; radius: Theme.radiusSmall; border.color: Theme.border; border.width: 1 }
                }
                Button {
                    width: (parent.width - parent.spacing) / 2; height: 42
                    text: root.inFlow ? qsTr("启动充电") : qsTr("查看电站")
                    onClicked: root.inFlow ? root.launch() : root.stackView.push("qrc:/UserClient/qml/pages/StationDetailPage.qml", { stationId: 1 })
                    contentItem: Text { text: parent.text; color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeBase; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: Theme.primary; radius: Theme.radiusSmall }
                }
            }
        }
    }
    // 校验失败弹卡
    Rectangle {
        visible: root.scanResult !== null && root.scanResult.bad === true
        anchors.bottom: parent.bottom; anchors.bottomMargin: 60
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 24; anchors.rightMargin: 24
        color: Theme.card; radius: Theme.radius
        Column {
            anchors.fill: parent; anchors.margins: 16
            spacing: 12
            Text { text: "\u26A0\uFE0F 桩码不匹配"; font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.danger }
            Text {
                width: parent.width; wrapMode: Text.Wrap
                text: qsTr("请扫描分配桩 ") + root.expectCode + qsTr(" 的二维码，启动充电不受影响。")
                color: Theme.textSecondary; font.pixelSize: Theme.fontSizeBase
            }
            Button {
                width: parent.width; height: 42
                text: qsTr("重新扫描")
                onClicked: { autoLaunchTimer.stop(); root.startScan() }
                contentItem: Text { text: parent.text; color: Theme.primary; font.pixelSize: Theme.fontSizeBase; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: Theme.background; radius: Theme.radiusSmall; border.color: Theme.primary; border.width: 1 }
            }
        }
    }

    // —— 底部操作 ——
    Row {
        anchors.bottom: parent.bottom; anchors.bottomMargin: 28
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 40
        Column {
            width: 60; spacing: 4
            Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: "📂"; font.pixelSize: 24 }
            Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: qsTr("相册"); color: "#9FB2CC"; font.pixelSize: Theme.fontSizeTiny }
        }
        Column {
            width: 60; spacing: 4
            Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: "🔦"; font.pixelSize: 24 }
            Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: qsTr("手电筒"); color: "#9FB2CC"; font.pixelSize: Theme.fontSizeTiny }
        }
    }

    Component.onCompleted: root.startScan()
    function startScan() {
        if (root.scanning) return
        autoLaunchTimer.stop()
        root.scanResult = null
        root.scanning = true
        scanTimer.start()
    }
    function launch() {
        autoLaunchTimer.stop()
        if (!root.scanResult) return
        // 桩码一致性校验（order.start），放行后进充电页
        if (ChargingFlow.confirmScan(root.stationId, root.scanResult.code)) {
            stackView.replace("qrc:/UserClient/qml/pages/ChargingPage.qml")
        }
    }
    Timer {
        id: scanTimer
        interval: 2400
        onTriggered: {
            root.scanning = false
            if (root.inFlow) {
                // 流程内：自动识别「分配桩」二维码，供校验（真实情况由摄像头读取）
                root.scanResult = { code: root.expectCode, station: ChargingFlow.flow.station_name }
                // 展示几秒后自动启动充电（非真机扫码，无法手动操作）
                autoLaunchTimer.start()
            } else {
                root.scanResult = { code: "CS-A01", station: "星星充·国贸中心旗舰站" }
            }
        }
    }
    Timer {
        id: autoLaunchTimer
        interval: 2600
        onTriggered: root.launch()
    }
}