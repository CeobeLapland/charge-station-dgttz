import QtQuick
import QtQuick.Controls
import QtWebEngine
import UserClient

// 导航页：调用外部地图（高德 web 路线规划）将“我的位置 → 目标点”拉起导航。
// 受 Qt WebEngine 沙箱限制，无法直接在应用内启动高德/百度 App，这里加载高德
// 官方 web 深链（uri.amap.com/navigation），页面上可点击「打开高德地图」调起外部。
Item {
    id: root
    readonly property var stackView: StackView.view
    property real fromLng: 116.397128
    property real fromLat: 39.916527
    property real toLng: 116.40
    property real toLat: 39.92
    property string toName: qsTr("目的地")

    // 高德 web 导航深链：from / to 支持经纬度+名称，mode=car 驾车
    readonly property string navUrl:
        ("https://uri.amap.com/navigation?from=" + root.fromLng + "," + root.fromLat + ",我的位置"
         + "&to=" + root.toLng + "," + root.toLat + "," + encodeURIComponent(root.toName)
         + "&mode=car&src=chargeUser")

    Rectangle { anchors.fill: parent; color: Theme.background }

    // —— 顶部 ——
    Rectangle {
        anchors.left: parent.left; anchors.top: parent.top
        width: parent.width; height: 72
        color: Theme.primary
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
                text: qsTr("导航到 ") + root.toName
                color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
            }
        }
    }

    // 提示条
    Rectangle {
        anchors.top: parent.top; anchors.topMargin: 72
        width: parent.width; height: 40
        color: Theme.accent + "22"
        Text {
            anchors.centerIn: parent
            text: qsTr("调高德地图路线规划（示例），可点击页面「打开高德地图」。")
            color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTiny
        }
    }

    // 路线 web 视图
    WebEngineView {
        anchors.top: parent.top; anchors.topMargin: 112
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        url: navUrl
        backgroundColor: "#ffffff"
    }
}