import QtQuick
import QtQuick.Controls
import UserClient

// 开发票页面：占位页面（后续接入开票接口）
Item {
    id: root
    readonly property var stackView: StackView.view

    Rectangle { anchors.fill: parent; color: Theme.background }

    // 顶部栏
    Rectangle {
        width: parent.width; height: 72; color: Theme.primary
        Row {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 8; anchors.rightMargin: 20
            spacing: 8
            Text { text: "‹"; font.pixelSize: 28; color: "#ffffff"
                MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: root.stackView.pop() } }
            Text { text: qsTr("开发票"); color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true }
        }
    }

    ScrollView {
        anchors.top: parent.top; anchors.topMargin: 72
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true

        Column {
            width: root.width
            spacing: 16
            topPadding: 16
            bottomPadding: 32

            // 占位提示大卡片
            Rectangle {
                width: parent.width - 32; x: 16
                height: 160
                color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
                Column {
                    anchors.centerIn: parent
                    spacing: 12
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "\u{1F4C4}"; font.pixelSize: 52
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("电子发票功能即将上线")
                        color: Theme.textPrimary; font.pixelSize: Theme.fontSizeBase; font.bold: true
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("可在此处开具充电订单的增值税电子普通发票")
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                    }
                }
            }

            // 占位：发票信息表单
            GroupCard {
                title: qsTr("发票抬头（预览占位）")
                spacing: 10
                content: Item { width: parent.width; height: boxCol.implicitHeight
                    Column { id: boxCol; width: parent.width; spacing: 10
                        PlaceRow { label: qsTr("抬头类型"); value: qsTr("个人 / 企业（可选择）") }
                        PlaceRow { label: qsTr("发票抬头"); value: qsTr("请输入名称") }
                        PlaceRow { label: qsTr("税号");     value: qsTr("企业必填，个人选填") }
                        PlaceRow { label: qsTr("收票邮箱"); value: qsTr("用于接收电子发票 PDF") }
                        PlaceRow { label: qsTr("收票手机"); value: qsTr("用于短信提醒开票进度") }
                    }
                }
            }

            // 占位：可开票订单列表
            GroupCard {
                title: qsTr("可开票订单（预览占位）")
                spacing: 8
                content: Item { width: parent.width; height: ordCol.implicitHeight
                    Column { id: ordCol; width: parent.width; spacing: 6
                        PlaceRow { label: qsTr("订单 9008"); value: "\u{FFE5}31.40  " + qsTr("（待开）") }
                        PlaceRow { label: qsTr("订单 9009"); value: "\u{FFE5}20.10  " + qsTr("（待开）") }
                        PlaceRow { label: qsTr("订单 9012"); value: "\u{FFE5}9.70  " + qsTr("（待开）") }
                    }
                }
            }

            // 按钮区
            Rectangle {
                width: parent.width - 32; x: 16
                height: 48; radius: 24
                color: Theme.border
                Text {
                    anchors.centerIn: parent
                    text: qsTr("提交开票申请（待接入）")
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                }
            }
        }
    }

    // —— 复用组件 ——
    component GroupCard: Rectangle {
        property string title
        property int spacing: 8
        property Item content: Item { }
        width: root.width - 32; x: 16
        height: col.implicitHeight + 20
        color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
        Column {
            id: col
            width: parent.width - 24
            anchors.left: parent.left; anchors.leftMargin: 12
            anchors.top: parent.top; anchors.topMargin: 12
            spacing: parent.parent.spacing
            Text { text: parent.parent.title; font.pixelSize: Theme.fontSizeSmall + 1; font.bold: true; color: Theme.textPrimary }
            Loader {
                width: parent.width
                sourceComponent: parent.parent.content
                active: true
            }
        }
    }
    component PlaceRow: Row {
        property string label
        property string value
        width: parent.width
        spacing: 8
        Text { text: parent.label; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny; width: 96 }
        Text { text: parent.value; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTiny }
    }
}
