import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine
import UserClient

// =========================================================
// 历史足迹：按时间段筛选「已完成」充电订单，列表展示 + 复用
// MapLibre 地图把足迹点高亮，并用黑色折线按时间顺序连接。
//
// 接口预留（未来接真实数据）：
//  · cityCenter 城市中心坐标表：之后由服务端/地理编码下发
//  · 订单坐标解析：先用「订单城市」映射；若订单的 station_id
//    在电站库中存在，则用真实电站坐标（高亮更精确）
// =========================================================
Item {
    id: root
    signal requestClose()

    // —— 数据源 ——
    property var allOrders: []                     // 已完成订单（含解析后的 lng/lat）
    property var cityCenter: {
        "北京": [116.407526, 39.904030],
        "上海": [121.473701, 31.230416],
        "广州": [113.264385, 23.129112],
        "深圳": [114.057868, 22.543099],
        "杭州": [120.155070, 30.274084],
        "成都": [104.066513, 30.572269],
        "武汉": [114.305393, 30.593099],
        "西安": [108.939949, 34.341568],
        "重庆": [106.551643, 29.562849],
        "苏州": [120.619585, 31.317987],
        "天津": [117.200983, 39.084158],
        "青岛": [120.382639, 36.067082],
        "南京": [118.796877, 32.060255]
    }
    property var stationsAll: ExploreData.stations()
    // 仅当电站名与订单一致时用真实电站坐标（示例种子数据 id 不完全一致，需按名兜底）
    function stationCoord(id, name) {
        for (var i = 0; i < stationsAll.length; i++) {
            if (Number(stationsAll[i].id) === Number(id) &&
                stationsAll[i].name === name)
                return { lng: Number(stationsAll[i].longitude), lat: Number(stationsAll[i].latitude), hasStn: true }
        }
        return null
    }

    // —— 时间筛选 ——
    property var timeRanges: [
        { label: qsTr("全部"), days: 0 },
        { label: qsTr("近 7 天"), days: 7 },
        { label: qsTr("近 30 天"), days: 30 },
        { label: qsTr("近 90 天"), days: 90 }
    ]
    property int rangeIdx: 0
    property var filteredOrders: []

    // —— 解析订单：只收已完成，附加坐标 ——
    function parseOrders() {
        var raw = UserData.orders()
        var out = []
        for (var i = 0; i < raw.length; i++) {
            var o = raw[i]
            if (o.status !== "completed") continue
            var area = (o.station_area || "")
            var city = area.split("·")[0] || ""
            var cc = cityCenter[city] || null
            var sc = stationCoord(o.station_id, o.station_name)
            // 有真实电站坐标用电站，否则退到城市中心
            var lng = sc && sc.hasStn ? sc.lng : (cc ? cc[0] : 0)
            var lat = sc && sc.hasStn ? sc.lat : (cc ? cc[1] : 0)
            if (!lng && !lat) continue
            var t = o.settle_time || o.end_time || o.create_time || ""
            out.push({
                id: o.id, station_id: o.station_id,
                name: o.station_name, area: area, city: city,
                time: t, date: t.length >= 10 ? t.slice(0, 10) : t,
                energy: Number(o.energy_kwh || 0), amount: Number(o.pay_amount || o.amount || 0),
                lng: lng, lat: lat
            })
        }
        // 按时间倒序（新的在前）
        out.sort(function (a, b) { return (a.time < b.time) ? 1 : (a.time > b.time ? -1 : 0) })
        allOrders = out
    }

    function nowRef() {
        // 以最新一笔订单时间为"现在"（示例数据无系统时间依赖）
        if (!allOrders.length) return new Date()
        return new Date(allOrders[0].time.replace(/-/g, "/"))
    }

    function applyFilter() {
        var days = timeRanges[rangeIdx].days
        var now = nowRef()
        var cutoff = days > 0 ? new Date(now.getTime() - days * 86400000) : null
        var arr = []
        for (var i = 0; i < allOrders.length; i++) {
            var o = allOrders[i]
            if (cutoff && new Date(o.time.replace(/-/g, "/")) < cutoff) continue
            arr.push(o)
        }
        filteredOrders = arr
        updateMap()
    }

    // —— 地图（复用 MapLibre）——
    property bool mapReady: false
    property bool pendingApply: false
    function updateMap() {
        if (!mapReady) { pendingApply = true; return }
        pendingApply = false
        var ids = filteredOrders.map(function (o) { return o.station_id })
        var pts = filteredOrders.map(function (o) { return [o.lng, o.lat] })
        footMap.runJavaScript("setStations(" + JSON.stringify(stationsAll) + ")")
        footMap.runJavaScript("setPlannedStops(" + JSON.stringify(ids) + "," + JSON.stringify(pts) + ")")
        footMap.runJavaScript("drawFootline(" + JSON.stringify(pts) + ")")
    }

    // =================== UI ===================
    Rectangle { anchors.fill: parent; color: Theme.background }

    // —— 顶栏 ——
    Rectangle {
        id: header
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: 64
        color: Theme.primary
        Row {
            anchors.fill: parent
            anchors.leftMargin: 4; anchors.rightMargin: 16
            spacing: 4
            MouseArea {
                width: 44; height: 44
                anchors.verticalCenter: parent.verticalCenter
                onClicked: root.requestClose()
                Text { anchors.centerIn: parent; text: "‹"; color: "#ffffff"; font.pixelSize: 32 }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                Text { text: qsTr("历史足迹"); color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true }
                Text { text: qsTr("走过的充电站 · 按时间串联"); color: "#DDEeff"; font.pixelSize: 11 }
            }
        }
    }

    Flickable {
        id: scroller
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        contentHeight: col.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Column {
            id: col
            width: scroller.width
            spacing: 12
            leftPadding: 14; rightPadding: 14; topPadding: 14; bottomPadding: 20

            // —— 时间筛选栏 ——
            Row {
                width: parent.width - 28; x: 14
                spacing: 8
                Repeater {
                    model: root.timeRanges
                    delegate: Rectangle {
                        width: (parent.width - (root.timeRanges.length - 1) * 8) / root.timeRanges.length
                        height: 34; radius: 17
                        color: root.rangeIdx === index ? Theme.primary : Theme.card
                        border.color: root.rangeIdx === index ? Theme.primary : Theme.border; border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: root.rangeIdx === index ? "#ffffff" : Theme.textPrimary
                            font.pixelSize: Theme.fontSizeTiny; font.bold: root.rangeIdx === index
                        }
                        MouseArea { anchors.fill: parent; onClicked: { root.rangeIdx = index; root.applyFilter() } }
                    }
                }
            }

            // 计数
            Text {
                width: parent.width - 28; x: 14
                text: qsTr("共 ") + root.filteredOrders.length + qsTr(" 个足迹点")
                color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
            }

            // —— 地图（足迹连线高亮） ——
            Rectangle {
                width: parent.width - 28; x: 14
                height: 200; radius: Theme.radiusSmall; clip: true
                border.color: Theme.border
                WebEngineView {
                    id: footMap
                    anchors.fill: parent
                    url: "qrc:/UserClient/qml/map/explore_map.html"
                    visible: root.filteredOrders.length > 0
                    onLoadingChanged: function (load) {
                        if (load.status === WebEngineView.LoadSucceededStatus) { root.mapReady = true; root.updateMap() }
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: root.filteredOrders.length === 0
                    text: qsTr("该时间段内暂无充电足迹")
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                }
            }

            // —— 足迹列表 ——
            Column {
                width: parent.width - 28; x: 14
                spacing: 8
                Repeater {
                    model: root.filteredOrders
                    delegate: Rectangle {
                        width: parent.width
                        height: 64
                        color: Theme.card; border.color: Theme.border; radius: Theme.radiusSmall
                        Row {
                            anchors.fill: parent; anchors.margins: 12
                            spacing: 10
                            Rectangle {
                                width: 34; height: 34; radius: 17
                                anchors.verticalCenter: parent.verticalCenter
                                color: Theme.accent + "22"
                                Text { anchors.centerIn: parent; text: "⚡"; font.pixelSize: 16 }
                            }
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 120
                                spacing: 2
                                Text {
                                    width: parent.width; elide: Text.ElideRight
                                    text: modelData.name || ""
                                    font.pixelSize: Theme.fontSizeSmall; font.bold: true; color: Theme.textPrimary
                                }
                                Text {
                                    width: parent.width; elide: Text.ElideRight
                                    text: modelData.area + " · " + modelData.date
                                    font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary
                                }
                            }
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                Text {
                                    text: modelData.energy.toFixed(1) + qsTr(" 度")
                                    font.pixelSize: Theme.fontSizeTiny; color: Theme.textPrimary
                                    horizontalAlignment: Text.AlignRight
                                }
                                Text {
                                    text: "¥" + modelData.amount.toFixed(2)
                                    font.pixelSize: Theme.fontSizeTiny; color: Theme.primary; font.bold: true
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: { parseOrders(); applyFilter() }
}