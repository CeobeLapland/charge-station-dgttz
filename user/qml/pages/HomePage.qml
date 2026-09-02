import QtQuick
import QtQuick.Controls
import UserClient

// 首页：附近充电站列表（station.nearby 的本地示例实现）。
// 由 ExploreData 提供电站种子，按距模拟定位点的距离升序排列；卡片点击进入电站详情。
Item {
    id: root
    property int navIndex: 0
    readonly property var stackView: StackView.view

    // —— 模拟当前定位（北京·朝阳）——
    readonly property var myLoc: { "lng": 116.46, "lat": 39.91 }

    // —— 全量电站 ——
    readonly property var stationsAll: ExploreData.stations()

    // —— 距离（km，Haversine）——
    function haversine(lng1, lat1, lng2, lat2) {
        var R = 6371.0
        var toRad = function (d) { return d * Math.PI / 180.0 }
        var dLat = toRad(lat2 - lat1)
        var dLng = toRad(lng2 - lng1)
        var a = Math.sin(dLat / 2) * Math.sin(dLat / 2)
              + Math.cos(toRad(lat1)) * Math.cos(toRad(lat2))
              * Math.sin(dLng / 2) * Math.sin(dLng / 2)
        return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a))
    }

    // —— 起价：min(谷/平/峰电价) + 服务费 ——
    function startPrice(st) {
        var rules = ExploreData.priceRulesForStation(st.id)
        var minp = Infinity
        for (var i = 0; i < rules.length; i++) {
            var p = Number(rules[i].price || 0)
            if (p < minp) minp = p
        }
        if (minp === Infinity) minp = 0
        return minp + Number(st.service_fee || 0)
    }

    // —— 带距离/起价的排序列表 ——
    readonly property var list: {
        var out = []
        for (var i = 0; i < stationsAll.length; i++) {
            var st = stationsAll[i]
            out.push({
                data: st,
                distance_km: haversine(myLoc.lng, myLoc.lat, Number(st.longitude), Number(st.latitude)),
                start_price: startPrice(st)
            })
        }
        out.sort(function (a, b) { return a.distance_km - b.distance_km })
        return out
    }

    // —— 一键找空闲桩：最近的「有快充空闲」的站；没有则最近的任意空闲站 ——
    function nearestIdleStation() {
        for (var i = 0; i < list.length; i++) {
            var st = list[i].data
            if ((Number(st.fast_idle) + Number(st.slow_idle)) > 0)
                return list[i]
        }
        return null
    }
    readonly property var idleRec: nearestIdleStation()

    // 不透明背景
    Rectangle { anchors.fill: parent; color: Theme.background }

    Column {
        anchors.fill: parent
        spacing: 0

        // 顶部：定位 + 标题
        Rectangle {
            width: parent.width
            height: 92
            color: Theme.primary
            Column {
                anchors.left: parent.left; anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 20; anchors.rightMargin: 20
                spacing: 6
                Row {
                    width: parent.width
                    spacing: 8
                    Text { text: "\u{1F4CD}"; font.pixelSize: 20 }
                    Text {
                        text: qsTr("北京市 · 朝阳区")
                        color: "#ffffff"; font.pixelSize: Theme.fontSizeBase; font.bold: true
                    }
                    Text { text: "▾"; color: "#E6F0FF"; font.pixelSize: 14 }
                }
                Text {
                    text: qsTr("附近充电站")
                    color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true
                }
            }
        }

        // 一键找空闲桩
        Rectangle {
            width: parent.width
            height: 58
            color: Theme.card
            border.color: Theme.border
            Column {
                anchors.fill: parent
                anchors.leftMargin: 16; anchors.rightMargin: 16; anchors.topMargin: 8; anchors.bottomMargin: 8
                spacing: 2
                Row {
                    width: parent.width; spacing: 8
                    Text { text: "\u{26A1}"; font.pixelSize: 18 }
                    Text {
                        text: qsTr("一键找空闲桩")
                        font.pixelSize: Theme.fontSizeBase; font.bold: true; color: Theme.textPrimary
                    }
                }
                Text {
                    width: parent.width
                    text: idleRec
                          ? (qsTr("推荐：") + idleRec.data.name + qsTr(" · 有空闲桩 · 距离 ")
                             + idleRec.distance_km.toFixed(1) + " km")
                          : qsTr("当前暂无空闲桩")
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall; elide: Text.ElideRight
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: if (idleRec) root.stackView.push(
                               "qrc:/UserClient/qml/pages/StationDetailPage.qml",
                               { stationId: idleRec.data.id })
            }
        }

        // 列表标题
        Row {
            width: parent.width - 32
            anchors.left: parent.left; anchors.leftMargin: 16
            height: 40
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("按距离排序 · 共 ") + list.length + qsTr(" 座")
                font.pixelSize: Theme.fontSizeSmall; color: Theme.textSecondary
            }
        }

        // 电站列表
        ListView {
            id: listView
            width: parent.width
            height: parent.height - 92 - 58 - 40
            clip: true
            model: root.list
            spacing: 12
            topMargin: 4
            bottomMargin: 16

            delegate: Rectangle {
                width: listView.width - 32
                x: 16
                height: cardCol.implicitHeight + 20
                color: Theme.card
                radius: Theme.radiusSmall
                border.color: Theme.border

                Column {
                    id: cardCol
                    width: parent.width - 24
                    anchors.left: parent.left; anchors.leftMargin: 12
                    anchors.top: parent.top; anchors.topMargin: 10
                    spacing: 6

                    // 第一行：站名 + 评分
                    Row {
                        width: parent.width
                        spacing: 8
                        Text {
                            width: parent.width - 96
                            text: modelData.data.name || ""
                            font.pixelSize: Theme.fontSizeBase + 1
                            font.bold: true
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                        }
                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text { text: "⭐"; font.pixelSize: 14; color: "#F59E0B" }
                            Text {
                                text: Number(modelData.data.rating || 0).toFixed(1)
                                font.pixelSize: Theme.fontSizeSmall; font.bold: true; color: Theme.textPrimary
                            }
                        }
                    }

                    // 地址
                    Text {
                        width: parent.width
                        text: modelData.data.address || ""
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                        elide: Text.ElideRight
                    }

                    // 标签行：归属 + 设施
                    Row {
                        width: parent.width; spacing: 6
                        Repeater {
                            model: [modelData.data.owner_label || "",
                                    ((Number(modelData.data.fast_idle) + Number(modelData.data.slow_idle)) > 0 ? qsTr("有空闲") : qsTr("已满")),
                                    (modelData.data.has_swap ? qsTr("换电") : "")]
                            delegate: Rectangle {
                                visible: modelData !== ""
                                height: 20
                                width: txt.implicitWidth + 12
                                radius: 10
                                color: Theme.primary + "14"
                                Text {
                                    id: txt
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: Theme.primary
                                    font.pixelSize: 10; font.bold: true
                                }
                            }
                        }
                    }
                }

                // 右下：价格 / 空闲 / 距离
                Row {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: 12; anchors.bottomMargin: 10
                    spacing: 14
                    Text {
                        text: "\u{FFE5}" + modelData.start_price.toFixed(2) + qsTr(" 起/度")
                        color: Theme.danger; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                    }
                    Text {
                        text: qsTr("空闲 ") + (Number(modelData.data.fast_idle) + Number(modelData.data.slow_idle))
                              + "/" + (modelData.data.total_chargers || 0)
                        color: ((Number(modelData.data.fast_idle) + Number(modelData.data.slow_idle)) > 0)
                               ? Theme.success : Theme.warn
                        font.pixelSize: Theme.fontSizeSmall; font.bold: true
                    }
                    Text {
                        text: modelData.distance_km < 1
                              ? Math.round(modelData.distance_km * 1000) + " m"
                              : modelData.distance_km.toFixed(1) + " km"
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.stackView.push(
                                   "qrc:/UserClient/qml/pages/StationDetailPage.qml",
                                   { stationId: modelData.data.id })
                }
            }
        }
    }
}