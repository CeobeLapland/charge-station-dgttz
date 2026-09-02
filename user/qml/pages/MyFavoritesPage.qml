import QtQuick
import QtQuick.Controls
import UserClient

// 「我的收藏」：收藏的电站卡片列表。点击卡片进入电站详情页（复用 StationDetailPage）。
// 数据来源：UserData.favorites()（favorite 表）聚合 ExploreData.stationById()。
Item {
    id: root
    readonly property var stackView: StackView.view

    // 收藏数据（映射成含站点的卡片），favoritesChanged 时重建
    property var favList: []
    function rebuild() {
        var raws = UserData.favorites()
        var out = []
        for (var i = 0; i < raws.length; i++) {
            var sid = Number(raws[i].station_id)
            var st = ExploreData.stationById(sid) || ({})
            out.push({
                station_id: sid,
                create_time: raws[i].create_time || "",
                name: st.name || qsTr("未知电站"),
                area: st.area || "",
                address: st.address || "",
                rating: Number(st.rating || 0),
                rating_count: Number(st.rating_count || 0),
                fast_idle: Number(st.fast_idle || 0),
                slow_idle: Number(st.slow_idle || 0)
            })
        }
        root.favList = out
    }
    Component.onCompleted: rebuild()
    Connections { target: UserData; function onFavoritesChanged() { rebuild() } }

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
                text: qsTr("我的收藏"); font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.textPrimary
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
        model: root.favList
        delegate: Rectangle {
            width: ListView.view.width - 32
            height: favCol.implicitHeight + 20
            radius: Theme.radiusSmall
            color: Theme.card
            border.color: Theme.border
            MouseArea {
                anchors.fill: parent
                onClicked: root.stackView.push("qrc:/UserClient/qml/pages/StationDetailPage.qml",
                                               { stationId: modelData.station_id })
            }
            Column {
                id: favCol
                width: parent.width - 24
                anchors.left: parent.left; anchors.leftMargin: 12
                anchors.right: parent.right; anchors.rightMargin: 12
                anchors.top: parent.top; anchors.topMargin: 10
                spacing: 6
                // 站名（左）+ 评分（右）
                Rectangle {
                    width: parent.width
                    height: Math.max(nameTxt.implicitHeight, favRatingLbl.implicitHeight)
                    Text {
                        id: nameTxt
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 64
                        text: (modelData.station_id > 0 ? "\u{2B50} " : "") + modelData.name
                        font.pixelSize: Theme.fontSizeBase; font.bold: true; color: Theme.textPrimary
                        elide: Text.ElideRight; wrapMode: Text.NoWrap
                    }
                    Text {
                        id: favRatingLbl
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: Number(modelData.rating).toFixed(1)
                        font.pixelSize: Theme.fontSizeBase; font.bold: true; color: "#F59E0B"
                    }
                }
                Text {
                    width: parent.width
                    text: "\u{1F4CD} " + modelData.address
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                    elide: Text.ElideRight; wrapMode: Text.NoWrap
                }
                Row {
                    width: parent.width
                    spacing: 10
                    Text {
                        text: qsTr("空闲 ") + modelData.fast_idle + "快/" + modelData.slow_idle + "慢"
                        color: Theme.success; font.pixelSize: Theme.fontSizeTiny; font.bold: true
                    }
                    Text {
                        text: qsTr("收藏于 ") + (modelData.create_time || "")
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                    }
                }
            }
        }
    }

    // 空态
    Rectangle {
        visible: root.favList.length === 0
        anchors.centerIn: parent
        Column {
            anchors.centerIn: parent
            spacing: 8
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "\u{1F493}"; font.pixelSize: 48
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("还没有收藏的电站")
                color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
            }
        }
    }
}