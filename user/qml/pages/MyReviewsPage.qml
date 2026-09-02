import QtQuick
import QtQuick.Controls
import UserClient

// 「我的评论」：用户发布过的评价列表（review）。社区/评价功能尚未接入，采用占位数据展示。
Item {
    id: root
    readonly property var stackView: StackView.view
    readonly property var reviews: UserData.myReviews()

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
                MouseArea {
                    anchors.fill: parent; anchors.margins: -8
                    onClicked: root.stackView.pop()
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("我的评论"); font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.textPrimary
            }
        }
    }

    // 列表
    ListView {
        anchors.top: parent.top; anchors.topMargin: 76
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        spacing: 12
        topMargin: 16
        bottomMargin: 24
        leftMargin: 16
        rightMargin: 16
        model: root.reviews
        delegate: Rectangle {
            width: ListView.view.width - 32
            height: rvCol.implicitHeight + 20
            radius: Theme.radiusSmall
            color: Theme.card
            border.color: Theme.border
            Column {
                id: rvCol
                width: parent.width - 24
                anchors.left: parent.left; anchors.leftMargin: 12
                anchors.right: parent.right; anchors.rightMargin: 12
                anchors.top: parent.top; anchors.topMargin: 10
                spacing: 6
                // 评价对象（左）+ 评分（右）
                Rectangle {
                    width: parent.width
                    height: Math.max(stNameTxt.implicitHeight, stScoreLbl.implicitHeight)
                    Text {
                        id: stNameTxt
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 80
                        text: qsTr("\u{1F3CD}\u{FE0F} ") + (modelData.station_name || qsTr("未知电站"))
                        font.pixelSize: Theme.fontSizeBase; font.bold: true; color: Theme.textPrimary
                        elide: Text.ElideRight; wrapMode: Text.NoWrap
                    }
                    Text {
                        id: stScoreLbl
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "\u{2B50} " + Number(modelData.overall_score || 0).toFixed(1)
                        color: "#F59E0B"; font.pixelSize: Theme.fontSizeBase; font.bold: true
                    }
                }
                // 标星可视化
                Text {
                    width: parent.width
                    text: repeatStar(Number(modelData.overall_score || 0))
                    color: "#F59E0B"; font.pixelSize: Theme.fontSizeSmall
                }
                // 评价文字
                Text {
                    width: parent.width
                    text: modelData.content || ""
                    color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.Wrap
                    lineHeight: 1.4
                }
                // 标签（左）+ 时间/有用（右）
                Rectangle {
                    width: parent.width
                    height: Math.max(tagRow.implicitHeight, usefullbl.implicitHeight)
                    Row {
                        id: tagRow
                        anchors.left: parent.left
                        spacing: 8
                        Repeater {
                            model: modelData.tags || []
                            delegate: Text {
                                text: "#" + modelData
                                color: Theme.accent; font.pixelSize: Theme.fontSizeTiny
                            }
                        }
                    }
                    Text {
                        id: usefullbl
                        anchors.right: parent.right
                        text: qsTr("有用 ") + (modelData.useful_count || 0) + " · " + (modelData.create_time || "")
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                    }
                }
            }
        }
    }

    // 底部说明：社区/评价待接入
    Rectangle {
        visible: root.reviews.length === 0
        anchors.centerIn: parent
        Column {
            anchors.centerIn: parent
            spacing: 8
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "\u{1F4AC}"; font.pixelSize: 48 }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("还没有发表过评价"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall }
        }
    }

    function repeatStar(score) {
        var full = Math.floor(score)
        var half = (score - full) >= 0.5 ? 1 : 0
        var s = ""
        for (var i = 0; i < full; i++) s += "\u{2B50}"
        if (half) s += "\u{1F31F}"
        return s
    }
}