import QtQuick
import QtQuick.Controls
import UserClient

// 「车辆详情」：展示车辆信息，底部有「修改车辆」与「删除车辆」按钮。
// 删除带二次确认弹窗（复用 ConfirmDialog），删除成功返回上一页。
Item {
    id: root
    property int vehicleId: 0
    readonly property var stackView: StackView.view

    function findVehicle(id) {
        var list = UserData.vehicles()
        for (var i = 0; i < list.length; i++)
            if (Number(list[i].id) === Number(id)) return list[i]
        return ({})
    }
    readonly property var v: findVehicle(root.vehicleId)

    function typeLabel(t) {
        if (t === "car") return qsTr("小汽车")
        if (t === "light_truck") return qsTr("微型货车")
        if (t === "two_wheeler") return qsTr("两轮电动")
        if (t === "three_wheeler") return qsTr("三轮电动")
        return t
    }
    function typeIcon(t) {
        if (t === "car") return "\u{1F697}"
        if (t === "light_truck") return "\u{1F69A}"
        if (t === "two_wheeler") return "\u{1F6B5}"
        if (t === "three_wheeler") return "\u{1F6F4}"
        return "\u{1F697}"
    }
    function connLabel(c) {
        if (c === "dc_gb") return qsTr("直流快充（国标）")
        if (c === "ac_gb") return qsTr("交流慢充（国标）")
        if (c === "other") return qsTr("其他")
        return c
    }

    // 背景
    Rectangle { anchors.fill: parent; color: Theme.background }

    // 顶部导航
    Rectangle {
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 76
        color: Theme.card; border.color: Theme.border
        Row {
            anchors.left: parent.left; anchors.leftMargin: 8
            anchors.right: parent.right; anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            Text {
                text: "‹"; font.pixelSize: 28; color: Theme.primary
                MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: root.stackView.pop() }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("车辆详情"); font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.textPrimary
            }
        }
    }

    // 内容区
    ScrollView {
        anchors.top: parent.top; anchors.topMargin: 76
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: actionBar.top
        anchors.bottomMargin: 8
        clip: true
        Column {
            width: root.width - 32
            x: 16
            spacing: 16
            topPadding: 16
            bottomPadding: 16

            // 车辆头部：图标 + 名称 + 默认标
            Rectangle {
                width: parent.width
                height: headCol.implicitHeight + 24
                radius: Theme.radiusSmall
                gradient: Gradient {
                    GradientStop { position: 0; color: Theme.primary }
                    GradientStop { position: 1; color: Theme.accent }
                }
                Column {
                    id: headCol
                    width: parent.width - 32
                    anchors.left: parent.left; anchors.leftMargin: 16
                    anchors.right: parent.right; anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6
                    Text {
                        text: root.typeIcon(v.type || "car")
                        font.pixelSize: 44
                    }
                    Row {
                        width: parent.width
                        spacing: 8
                        Text {
                            width: parent.width - 72
                            text: v.name || qsTr("未知车辆")
                            color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true
                            elide: Text.ElideRight; wrapMode: Text.NoWrap
                        }
                        Rectangle {
                            visible: Number(v.is_default) === 1
                            height: 20; width: defTxt.implicitWidth + 12; radius: 10
                            color: "#FFFFFF33"
                            Text {
                                id: defTxt; anchors.centerIn: parent
                                text: qsTr("默认车辆"); color: "#ffffff"; font.pixelSize: 10; font.bold: true
                            }
                        }
                    }
                }
            }

            // 车辆信息明细
            Rectangle {
                width: parent.width
                height: infoCol.implicitHeight + 24
                radius: Theme.radiusSmall
                color: Theme.card; border.color: Theme.border
                Column {
                    id: infoCol
                    width: parent.width - 32
                    anchors.left: parent.left; anchors.leftMargin: 16
                    anchors.right: parent.right; anchors.rightMargin: 16
                    anchors.top: parent.top; anchors.topMargin: 12
                    spacing: 12
                    Text { text: qsTr("车辆信息"); font.pixelSize: Theme.fontSizeSmall + 1; font.bold: true; color: Theme.textPrimary }
                    InfoRow { k: qsTr("车辆名称");   v: v.name || "-" }
                    InfoRow { k: qsTr("车辆类型");   v: root.typeLabel(v.type || "") }
                    InfoRow { k: qsTr("电池容量");   v: (v.battery_kwh || 0) + " kWh" }
                    InfoRow { k: qsTr("充电接口");   v: root.connLabel(v.connector_type || "") }
                    InfoRow { k: qsTr("最大功率");   v: (v.max_power_kw || 0) + " kW" }
                    InfoRow { k: qsTr("是否为默认"); v: (Number(v.is_default) === 1) ? qsTr("是") : qsTr("否") }
                    InfoRow { k: qsTr("添加时间");   v: v.created_time || "-" }
                }
            }

            component InfoRow: Row {
                property string k: ""
                property string v: ""
                width: parent.width
                spacing: 12
                Text {
                    width: 84; text: k; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                }
                Text {
                    width: parent.width - 84
                    text: v; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    // 底部操作条：修改 + 删除
    Rectangle {
        id: actionBar
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 84
        color: Theme.card; border.color: Theme.border
        Row {
            anchors.centerIn: parent
            spacing: 20
            // 删除（可确认置前，避免误点）
            Rectangle {
                width: 150; height: 48; radius: Theme.radiusSmall
                color: Theme.background; border.color: Theme.danger; border.width: 1
                Text { anchors.centerIn: parent; text: qsTr("删除车辆"); color: Theme.danger; font.bold: true; font.pixelSize: Theme.fontSizeBase }
                MouseArea { anchors.fill: parent; onClicked: delDlg.open() }
            }
            // 修改
            Rectangle {
                width: 150; height: 48; radius: Theme.radiusSmall; color: Theme.primary
                Text { anchors.centerIn: parent; text: qsTr("修改车辆"); color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeBase }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.stackView.push("qrc:/UserClient/qml/pages/VehicleFormPage.qml",
                                                   { vehicleId: root.vehicleId })
                }
            }
        }
    }

    // 删除二次确认
    ConfirmDialog {
        id: delDlg
        titleText: qsTr("删除车辆")
        messageText: qsTr("确定要删除「") + (v.name || "") + qsTr("」吗？删除后不可恢复。")
        okText: qsTr("删除")
        onConfirmed: {
            UserData.removeVehicle(root.vehicleId)
            root.stackView.pop()
        }
    }
}