import QtQuick
import QtQuick.Controls
import UserClient

// 优惠券详情页：顶部统计（可用/已用/过期）+ Tab 切换 + 卡片列表
Item {
    id: root
    readonly property var stackView: StackView.view
    readonly property var allCoupons: UserData.coupons()

    property string filter: "unused"   // unused / used / expired

    readonly property var unusedCount: countByStatus("unused")
    readonly property var usedCount: countByStatus("used")
    readonly property var expiredCount: countByStatus("expired")

    function countByStatus(s) {
        var n = 0
        for (var i = 0; i < allCoupons.length; i++)
            if (allCoupons[i].status === s) n++
        return n
    }
    readonly property var filtered: {
        var out = []
        for (var i = 0; i < allCoupons.length; i++)
            if (allCoupons[i].status === root.filter) out.push(allCoupons[i])
        return out
    }

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
            Text { text: qsTr("我的优惠券"); color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true }
        }
    }

    // 数量汇总条
    Rectangle {
        anchors.top: parent.top; anchors.topMargin: 72
        width: parent.width; height: 72
        color: Theme.card
        Row {
            anchors.fill: parent
            Repeater {
                model: [
                    { key: "unused",  label: qsTr("可使用"), num: root.unusedCount,  c: Theme.primary },
                    { key: "used",    label: qsTr("已使用"), num: root.usedCount,    c: Theme.textSecondary },
                    { key: "expired", label: qsTr("已过期"), num: root.expiredCount, c: Theme.warn }
                ]
                delegate: Item {
                    width: root.width / 3; height: parent.height
                    Column {
                        anchors.centerIn: parent
                        spacing: 3
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: String(modelData.num)
                            color: modelData.c; font.pixelSize: Theme.fontSizeTitle + 2; font.bold: true
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.label
                            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                        }
                    }
                    Rectangle {
                        visible: root.filter === modelData.key
                        width: 32; height: 3; radius: 1.5; color: modelData.c
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom; anchors.bottomMargin: 2
                    }
                    MouseArea { anchors.fill: parent; onClicked: root.filter = modelData.key }
                }
            }
        }
        // 汇总条底部分隔线
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.bottom: parent.bottom; height: 1
            color: Theme.border
        }
    }

    // 优惠券列表
    ListView {
        anchors.top: parent.top; anchors.topMargin: 72 + 72
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        spacing: 12
        topMargin: 12
        bottomMargin: 16

        model: root.filtered

        delegate: Rectangle {
            width: ListView.view.width - 32; x: 16
            height: 92
            radius: Theme.radiusSmall
            color: (modelData.status === "unused") ? Theme.card : "#FAFAFA"
            border.color: Theme.border
            opacity: (modelData.status === "unused") ? 1.0 : 0.7

            Row {
                anchors.fill: parent
                spacing: 0
                // 左侧票根：面额
                Rectangle {
                    width: 104; height: parent.height
                    color: (modelData.status === "unused") ? Theme.primary : Theme.textSecondary
                    radius: Theme.radiusSmall
                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 0
                            Text {
                                text: "\u{FFE5}"; color: "#ffffff"
                                font.pixelSize: Theme.fontSizeSmall; font.bold: true
                                anchors.bottom: parent.bottom
                            }
                            Text {
                                text: String(Math.floor(modelData.discount_amount || 0))
                                color: "#ffffff"; font.pixelSize: 32; font.bold: true
                            }
                            Text {
                                text: "." + String((Number(modelData.discount_amount) * 10 % 10).toFixed(0))
                                color: "#ffffff"; font.pixelSize: Theme.fontSizeBase; font.bold: true
                                anchors.bottom: parent.bottom
                            }
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: (modelData.min_amount > 0
                                   ? qsTr("满") + Math.floor(modelData.min_amount) + qsTr("可用")
                                   : qsTr("无门槛"))
                            color: "#FFFFFFEE"; font.pixelSize: 10
                        }
                    }
                    // 票根圆孔
                    Rectangle { width: 10; height: 10; radius: 5; color: Theme.background
                        x: parent.width - 5; y: -5 }
                    Rectangle { width: 10; height: 10; radius: 5; color: Theme.background
                        x: parent.width - 5; y: parent.height - 5 }
                }

                // 右侧：券信息
                Column {
                    width: parent.width - 104
                    height: parent.height
                    anchors.left: parent.left; anchors.leftMargin: 104 + 14
                    anchors.top: parent.top; anchors.topMargin: 12
                    anchors.right: parent.right; anchors.rightMargin: 12
                    spacing: 4
                    Text {
                        text: modelData.title || ""
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeSmall; font.bold: true
                        elide: Text.ElideRight; width: parent.width
                    }
                    Text {
                        text: (modelData.scope || "")
                            + (modelData.time_range ? (" · " + modelData.time_range) : "")
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                        elide: Text.ElideRight; width: parent.width
                    }
                    Row {
                        width: parent.width
                        Text {
                            text: qsTr("有效期至：") + (modelData.valid_until || "-")
                            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                        }
                        Item { width: 1; height: 1 }
                        Rectangle {
                            anchors.right: parent.right
                            height: 22; width: statusTxt.implicitWidth + 12; radius: 11
                            color: statusColor(modelData.status) + "1A"
                            border.color: statusColor(modelData.status); border.width: 1
                            Text {
                                id: statusTxt; anchors.centerIn: parent
                                text: statusText(modelData.status)
                                color: statusColor(parent.parent.parent.modelData.status)
                                font.pixelSize: 10; font.bold: true
                            }
                        }
                    }
                }
            }
        }

        // 空状态
        Text {
            visible: root.filtered.length === 0
            anchors.centerIn: parent
            text: qsTr("暂无优惠券")
            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
        }
    }

    function statusText(s) {
        if (s === "unused") return qsTr("可使用")
        if (s === "used") return qsTr("已使用")
        if (s === "expired") return qsTr("已过期")
        return s
    }
    function statusColor(s) {
        if (s === "unused") return Theme.success
        if (s === "used") return Theme.textSecondary
        if (s === "expired") return Theme.warn
        return Theme.textSecondary
    }
}
