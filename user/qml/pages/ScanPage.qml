import QtQuick
import QtQuick.Controls
import UserClient

// 扫码页：当前无法直接调用摄像头，用动画取景框 + 延时模拟识别，兜底展示模拟结果。
Item {
    id: root
    readonly property var stackView: StackView.view

    property bool scanning: false
    property var scanResult: null    // 识别到的模拟内容

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
                text: qsTr("扫码")
                color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // —— 扫描取景框 ——
    Rectangle {
        id: frame
        anchors.top: parent.top; anchors.topMargin: 140
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
            text: root.scanning ? qsTr("正在扫描，请将二维码对准取景框…") : qsTr("将二维码放入框内即可自动扫描")
            color: "#9FB2CC"; font.pixelSize: Theme.fontSizeSmall
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("（示例环境无法调用摄像头，将自动模拟识别）")
            color: "#5B6B84"; font.pixelSize: Theme.fontSizeTiny
        }
    }

    // —— 模拟识别结果 ——
    Rectangle {
        visible: root.scanResult !== null
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
                text: root.scanResult ? (qsTr("桩码：") + root.scanResult + "\n" + qsTr("所在：星星充·国贸中心旗舰站")) : ""
                color: Theme.textSecondary; font.pixelSize: Theme.fontSizeBase
            }
            Row {
                width: parent.width; spacing: 10
                Button {
                    width: (parent.width - parent.spacing) / 2; height: 42
                    text: qsTr("继续扫描")
                    onClicked: root.scanResult = null
                    contentItem: Text { text: parent.text; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeBase; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: Theme.background; radius: Theme.radiusSmall; border.color: Theme.border; border.width: 1 }
                }
                Button {
                    width: (parent.width - parent.spacing) / 2; height: 42
                    text: qsTr("查看电站")
                    onClicked: root.stackView.push("qrc:/UserClient/qml/pages/StationDetailPage.qml",
                                    { stationId: 1 })
                    contentItem: Text { text: parent.text; color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeBase; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: Theme.primary; radius: Theme.radiusSmall }
                }
            }
        }
    }

    // —— 底部操作 ——
    Row {
        anchors.bottom: parent.bottom; anchors.bottomMargin: 28
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 40
        Column {
            width: 60
            spacing: 4
            Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: "📂"; font.pixelSize: 24 }
            Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: qsTr("相册"); color: "#9FB2CC"; font.pixelSize: Theme.fontSizeTiny }
        }
        Column {
            width: 60
            spacing: 4
            Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: "🔦"; font.pixelSize: 24 }
            Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: qsTr("手电筒"); color: "#9FB2CC"; font.pixelSize: Theme.fontSizeTiny }
        }
    }

    Component.onCompleted: root.startScan()
    function startScan() {
        if (root.scanning) return
        root.scanResult = null
        root.scanning = true
        scanTimer.start()
    }
    Timer {
        id: scanTimer
        interval: 2600
        onTriggered: {
            root.scanning = false
            root.scanResult = "CS-A01"   // 模拟识别桩码
        }
    }
}