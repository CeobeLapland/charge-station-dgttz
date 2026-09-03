import QtQuick
import QtQuick.Controls
import UserClient

// 积分详情页：当前积分+累计获得+明细列表（point_record 表）
Item {
    id: root
    readonly property var stackView: StackView.view
    property var profile: ({})
    readonly property var records: UserData.pointRecords()

    function refresh() { root.profile = UserData.profile() }
    Component.onCompleted: refresh()
    Connections { target: UserData; function onProfileChanged() { refresh() } }

    // 汇总：获得和消耗
    readonly property int earnedSum: {
        var s = 0
        for (var i = 0; i < records.length; i++) {
            var r = records[i]; if (r.change > 0) s += r.change
        }
        s
    }
    readonly property int spentSum: {
        var s = 0
        for (var i = 0; i < records.length; i++) {
            var r = records[i]; if (r.change < 0) s += Math.abs(r.change)
        }
        s
    }

    Rectangle { anchors.fill: parent; color: Theme.background }

    // 顶部渐变头
    Rectangle {
        width: parent.width; height: 180
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.primary }
            GradientStop { position: 1; color: Theme.accent }
        }
        Row {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: parent.top; anchors.topMargin: 24
            anchors.leftMargin: 8; anchors.rightMargin: 20
            spacing: 8
            Text { text: "‹"; font.pixelSize: 28; color: "#ffffff"
                MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: root.stackView.pop() } }
            Text { text: qsTr("积分详情"); color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true }
        }
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("当前可用积分"); color: "#FFFFFFDD"; font.pixelSize: Theme.fontSizeTiny
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: String(profile.points || 0)
                color: "#ffffff"; font.pixelSize: 40; font.bold: true
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 24
                Column {
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "+" + earnedSum; color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall; font.bold: true }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("累计获得"); color: "#FFFFFFCC"; font.pixelSize: 10 }
                }
                Rectangle { width: 1; height: 24; color: "#FFFFFF55"; anchors.verticalCenter: parent.verticalCenter }
                Column {
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "-" + spentSum; color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall; font.bold: true }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("累计使用"); color: "#FFFFFFCC"; font.pixelSize: 10 }
                }
            }
        }
    }

    // 明细列表
    Column {
        anchors.top: parent.top; anchors.topMargin: 180
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 0
        Rectangle {
            width: parent.width; height: 44; color: Theme.card
            Text {
                anchors.left: parent.left; anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("积分明细"); font.pixelSize: Theme.fontSizeSmall; font.bold: true; color: Theme.textPrimary
            }
            Text {
                anchors.right: parent.right; anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("共 ") + records.length + qsTr(" 条")
                color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
            }
            // 底部分隔线
            Rectangle {
                anchors.left: parent.left; anchors.right: parent.right
                anchors.bottom: parent.bottom; height: 1
                color: Theme.border
            }
        }
        ListView {
            width: parent.width
            height: parent.height - 44
            clip: true
            spacing: 0
            model: root.records

            delegate: Rectangle {
                width: ListView.view.width
                height: 60
                color: Theme.card
                // 左：图标（不用 Row 管理器，直接 anchors，避免变动值的 anchors.right 违规）
                Rectangle {
                    id: iconR
                    anchors.left: parent.left; anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    width: 36; height: 36; radius: 18
                    color: (modelData.change >= 0) ? Theme.success + "1A" : Theme.danger + "1A"
                    Text {
                        anchors.centerIn: parent
                        text: "\u{2B50}"; font.pixelSize: 18
                        color: (modelData.change >= 0) ? Theme.success : Theme.danger
                    }
                }
                // 右：变动值
                Text {
                    id: changeTxt
                    anchors.right: parent.right; anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: (modelData.change >= 0 ? "+" : "") + (modelData.change || 0)
                    color: (modelData.change >= 0) ? Theme.success : Theme.danger
                    font.pixelSize: Theme.fontSizeBase; font.bold: true
                }
                // 中间：原因 + 时间
                Column {
                    anchors.left: iconR.right; anchors.leftMargin: 12
                    anchors.right: changeTxt.left; anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3
                    Text {
                        text: reasonText(modelData.reason)
                        color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                        elide: Text.ElideRight; width: parent.width
                    }
                    Text {
                        text: modelData.create_time || ""
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                    }
                }
                // 行分隔线
                Rectangle {
                    anchors.left: parent.left; anchors.leftMargin: 16
                    anchors.right: parent.right; anchors.rightMargin: 16
                    anchors.bottom: parent.bottom; height: 1
                    color: Theme.border
                }
            }
        }
    }

    function reasonText(r) {
        if (r === "charge") return qsTr("充电获得")
        if (r === "redeem") return qsTr("积分抵扣")
        if (r === "sign_in") return qsTr("签到奖励")
        return r || "-"
    }
}
