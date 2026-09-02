import QtQuick
import QtQuick.Controls
import UserClient

// 「车辆」表单页：新建（vehicleId=0）与修改（vehicleId>0）复用。
// 字段对齐 DATA_STRUCTURE vehicle 表：name / type / battery_kwh / connector_type / max_power_kw / is_default。
Item {
    id: root
    property int vehicleId: 0
    readonly property var stackView: StackView.view
    readonly property bool editMode: root.vehicleId > 0

    function findVehicle(id) {
        var list = UserData.vehicles()
        for (var i = 0; i < list.length; i++)
            if (Number(list[i].id) === Number(id)) return list[i]
        return ({})
    }
    readonly property var v: editMode ? findVehicle(root.vehicleId) : ({})

    property var typeModel: [
        { key: "car",           label: qsTr("小汽车"),     icon: "\u{1F697}" },
        { key: "light_truck",   label: qsTr("微型货车"),   icon: "\u{1F69A}" },
        { key: "two_wheeler",   label: qsTr("两轮电动"),   icon: "\u{1F6B5}" },
        { key: "three_wheeler", label: qsTr("三轮电动"),   icon: "\u{1F6F4}" }
    ]
    property var connModel: [
        { key: "dc_gb", label: qsTr("直流快充（国标）") },
        { key: "ac_gb", label: qsTr("交流慢充（国标）") },
        { key: "other", label: qsTr("其他") }
    ]

    property string selType: v.type || "car"
    property string selConn: v.connector_type || "dc_gb"
    property bool isDefault: Number(v.is_default || 0) === 1

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
                text: editMode ? qsTr("修改车辆") : qsTr("新建车辆")
                font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.textPrimary
            }
        }
    }

    // 内容区
    ScrollView {
        anchors.top: parent.top; anchors.topMargin: 76
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: bottomBar.top
        clip: true
        Column {
            width: root.width - 32
            x: 16
            spacing: 18
            topPadding: 20
            bottomPadding: 20

            // 车辆名称
            FieldLabel { text: qsTr("车辆名称") }
            TextField {
                id: nameField
                width: parent.width
                height: 46
                text: v.name || ""
                maximumLength: 20
                placeholderText: qsTr("例如：我的小鹏 G6")
                verticalAlignment: Text.AlignVCenter
                leftPadding: 14
                color: Theme.textPrimary
                background: Rectangle { color: Theme.card; border.color: Theme.border; border.width: 1; radius: Theme.radiusSmall }
            }

            // 车辆类型（单选 chip）
            FieldLabel { text: qsTr("车辆类型") }
            Row {
                width: parent.width
                spacing: 8
                Repeater {
                    model: root.typeModel
                    delegate: Rectangle {
                        id: chip
                        property bool on: root.selType === modelData.key
                        width: (parent.width - 8 * (root.typeModel.length - 1)) / root.typeModel.length
                        height: 44
                        radius: Theme.radiusSmall
                        color: chip.on ? Theme.primary : Theme.background
                        border.color: chip.on ? Theme.primary : Theme.border
                        border.width: 1
                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.icon; font.pixelSize: 16
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: chip.on ? "#ffffff" : Theme.textPrimary
                                font.pixelSize: Theme.fontSizeTiny
                            }
                        }
                        MouseArea { anchors.fill: parent; onClicked: root.selType = modelData.key }
                    }
                }
            }

            // 电池容量
            FieldLabel { text: qsTr("电池容量（kWh）") }
            TextField {
                id: batteryField
                width: parent.width
                height: 46
                text: v.battery_kwh !== undefined ? String(v.battery_kwh) : ""
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                placeholderText: qsTr("例如：66")
                verticalAlignment: Text.AlignVCenter
                leftPadding: 14
                color: Theme.textPrimary
                background: Rectangle { color: Theme.card; border.color: Theme.border; border.width: 1; radius: Theme.radiusSmall }
            }

            // 充电接口（单选）
            FieldLabel { text: qsTr("充电接口") }
            Column {
                width: parent.width
                spacing: 8
                Repeater {
                    model: root.connModel
                    delegate: Rectangle {
                        id: cchip
                        property bool on: root.selConn === modelData.key
                        width: parent.width
                        height: 40
                        radius: Theme.radiusSmall
                        color: cchip.on ? Theme.primary + "14" : Theme.background
                        border.color: cchip.on ? Theme.primary : Theme.border
                        border.width: 1
                        Text {
                            anchors.left: parent.left; anchors.leftMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.label
                            color: cchip.on ? Theme.primary : Theme.textPrimary
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        Text {
                            anchors.right: parent.right; anchors.rightMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            text: cchip.on ? "\u{2713}" : ""
                            color: Theme.primary; font.pixelSize: Theme.fontSizeBase; font.bold: true
                        }
                        MouseArea { anchors.fill: parent; onClicked: root.selConn = modelData.key }
                    }
                }
            }

            // 最大功率
            FieldLabel { text: qsTr("最大功率（kW）") }
            TextField {
                id: powerField
                width: parent.width
                height: 46
                text: v.max_power_kw !== undefined ? String(v.max_power_kw) : ""
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                placeholderText: qsTr("例如：250")
                verticalAlignment: Text.AlignVCenter
                leftPadding: 14
                color: Theme.textPrimary
                background: Rectangle { color: Theme.card; border.color: Theme.border; border.width: 1; radius: Theme.radiusSmall }
            }

            // 设为默认
            Rectangle {
                width: parent.width
                height: 52
                radius: Theme.radiusSmall
                color: Theme.background
                border.color: Theme.border; border.width: 1
                Text {
                    anchors.left: parent.left; anchors.leftMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("设为默认车辆"); color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall
                }
                Rectangle {
                    anchors.right: parent.right; anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: 48; height: 28; radius: 14
                    color: root.isDefault ? Theme.success : Theme.border
                    Rectangle {
                        x: root.isDefault ? parent.width - width - 3 : 3
                        anchors.verticalCenter: parent.verticalCenter
                        width: 22; height: 22; radius: 11; color: "#ffffff"
                    }
                    MouseArea { anchors.fill: parent; onClicked: root.isDefault = !root.isDefault }
                }
            }
        }
    }

    // 底部提交栏
    Rectangle {
        id: bottomBar
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 84
        color: Theme.card; border.color: Theme.border
        Rectangle {
            anchors.left: parent.left; anchors.leftMargin: 16
            anchors.right: parent.right; anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            height: 48; radius: Theme.radiusSmall; color: Theme.primary
            Text {
                anchors.centerIn: parent
                text: editMode ? qsTr("保存修改") : qsTr("确认添加")
                color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeBase
            }
            MouseArea { anchors.fill: parent; onClicked: submit() }
        }
    }

    // 轻提示
    Rectangle {
        id: toast; visible: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 100
        width: Math.min(toastText.implicitWidth + 32, root.width - 48)
        height: 40; radius: 20; color: "#B3000000"
        Text { id: toastText; anchors.centerIn: parent; color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall }
    }
    Timer { id: toastTimer; interval: 2000; onTriggered: toast.visible = false }
    function showToast(msg) { toastText.text = msg; toast.visible = true; toastTimer.restart() }

    function submit() {
        var name = nameField.text.trim()
        if (!name) { showToast(qsTr("请输入车辆名称")); return }
        var battery = Number(batteryField.text || "0")
        var power = Number(powerField.text || "0")
        if (battery <= 0) { showToast(qsTr("请输入有效的电池容量")); return }
        if (power <= 0) { showToast(qsTr("请输入有效的最大功率")); return }

        var data = {
            name: name,
            type: root.selType,
            battery_kwh: battery,
            connector_type: root.selConn,
            max_power_kw: power,
            is_default: root.isDefault ? 1 : 0
        }
        if (editMode) {
            UserData.updateVehicle(root.vehicleId, data)
            showToast(qsTr("车辆信息已更新"))
        } else {
            UserData.addVehicle(data)
            showToast(qsTr("车辆已添加"))
        }
        root.stackView.pop()
    }

    component FieldLabel: Text {
        width: parent.width
        color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall; font.bold: true
    }
}