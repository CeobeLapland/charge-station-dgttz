import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine
import UserClient

// 首页（改造版）：附近电站 + 地图 + 会话化操作区。
// 从上到下：搜索栏+扫码 → 城市/天气 → 地图(可拖可缩，我的定位点/电站小水滴/目标点)
//          → “到这里去/选点”操作条 → 折叠箭头(列表上移遮住地图) → 正在充电栏
//          → 筛选栏(排序/车辆/四个勾选) → 列表栏 → 智能推荐与现在就充(占位)。
// 地图与探索页共用同一套 MapLibre 底图 + 瓦片源配置（authStore），各自独立实例。
Item {
    id: root
    property int navIndex: 0
    readonly property var stackView: StackView.view

    // ===================== 数据与定位 =====================
    readonly property var myLoc: { "lng": 116.397128, "lat": 39.916527 }   // 模拟定位（北京·朝阳）
    readonly property var stationsAll: ExploreData.stations()
    readonly property var vehiclesData: UserData.vehicles()
    readonly property var chargingOrders: {                                // 正在充电订单（可多笔）
        var out = []
        var all = UserData.orders()
        for (var i = 0; i < all.length; i++)
            if (all[i].status === "charging") out.push(all[i])
        return out
    }
    readonly property var weather: ExploreData.weatherForArea("北京市/北京市/朝阳区")
    property var dest: null        // {lng,lat,name} 目标点

    // ===================== 距离 / 起价 =====================
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
    function distText(km) {
        return km < 1 ? Math.round(km * 1000) + " m" : km.toFixed(1) + " km"
    }
    function weatherIcon(c) {
        if (c === "sunny") return "☀️"
        if (c === "cloudy") return "⛅"
        if (c === "rain") return "🌧️"
        if (c === "hot") return "🔥"
        if (c === "extreme") return "🌪️"
        return "❔"
    }
    function weatherText(c) {
        if (c === "sunny") return qsTr("晴")
        if (c === "cloudy") return qsTr("多云")
        if (c === "rain") return qsTr("雨")
        if (c === "hot") return qsTr("高温")
        if (c === "extreme") return qsTr("极端")
        return c
    }

    // ===================== 筛选 / 排序 =====================
    property int sortIdx: 0
    property int vehicleIdx: 0
    property bool onlySelf: false
    property bool needWashroom: false
    property bool needShelter: false
    property bool onlyFavorite: false

    readonly property var sortLabels: [qsTr("综合"), qsTr("距离"), qsTr("评分"),
                                       qsTr("空位数"), qsTr("充电快慢")]
    readonly property var vehicleLabels: {
        var labels = []
        for (var i = 0; i < vehiclesData.length; i++) {
            var tag = Number(vehiclesData[i].is_default || 0) === 1 ? qsTr("（默认）") : ""
            labels.push((vehiclesData[i].name || "") + tag)
        }
        if (!labels.length) labels.push(qsTr("未添加车辆"))
        return labels
    }
    function hasFacility(st, f) {
        var fa = st.facilities || []
        for (var i = 0; i < fa.length; i++) if (fa[i] === f) return true
        return false
    }
    ListModel { id: listModel }
    function recompute() {
        listModel.clear()
        var arr = []
        for (var i = 0; i < stationsAll.length; i++) {
            var st = stationsAll[i]
            if (onlySelf && st.owner_type !== "self_run") continue
            if (needWashroom && !hasFacility(st, "washroom")) continue
            if (needShelter && !hasFacility(st, "rain_shelter")) continue
            if (onlyFavorite && !UserData.isFavorite(st.id)) continue
            arr.push({
                data: st,
                distance_km: haversine(myLoc.lng, myLoc.lat, Number(st.longitude), Number(st.latitude)),
                start_price: startPrice(st),
                free: Number(st.fast_idle) + Number(st.slow_idle)
            })
        }
        arr.sort(function (a, b) {
            if (root.sortIdx === 2) return (b.data.rating || 0) - (a.data.rating || 0)
            if (root.sortIdx === 3) return b.free - a.free
            if (root.sortIdx === 4) {
                var af = Number(a.data.fast_count || 0) > 0 ? 1 : 0
                var bf = Number(b.data.fast_count || 0) > 0 ? 1 : 0
                if (bf !== af) return bf - af
                return a.distance_km - b.distance_km
            }
            return a.distance_km - b.distance_km
        })
        for (var j = 0; j < arr.length; j++) listModel.append(arr[j])
        recomputedCountHolder = arr.length
    }
    property int recomputedCountHolder: 0   // 占位，列表标题引用用

    // ===================== 地图（共用探索页底图 / 瓦片源） =====================
    readonly property var mapPresets: JSON.parse(authStore.mapTilePresetsJson())
    property bool mapReady: false
    property int lastPickSeq: 0
    property bool pickActive: false
    property int mapHeight: 320            // 展开/折叠

    function templateForCurrent() {
        var id = authStore.mapTileSource
        if (id === "custom") return authStore.mapTileCustomUrl || ""
        for (var i = 0; i < mapPresets.length; i++)
            if (mapPresets[i].id === id) return mapPresets[i].template
        return ""
    }
    function js(cmd) { if (mapReady) mapView.runJavaScript(cmd) }

    function initMap() {
        if (!mapReady) return
        applyTileTemplate()
        js("setInteractive(true)")
        js("setStations(" + JSON.stringify(stationsAll) + ")")
        js("setMyLocation(" + myLoc.lng + "," + myLoc.lat + ")")
        exposePickCallback()
        applyDestination()
    }
    function applyTileTemplate() { js("setTileTemplate(" + JSON.stringify(templateForCurrent()) + ")") }
    function flyTo(lng, lat, zoom) { js("flyTo(" + lng + "," + lat + "," + (zoom || 14) + ")") }
    function exposePickCallback() {
        var code = "window.__onPickPoint = function(lng,lat) {"
                 + "  window.__pickSeq = (window.__pickSeq||0)+1;"
                 + "  document.title = 'pick:'+lng+':'+lat+':'+window.__pickSeq;"
                 + "};"
        js(code)
    }
    function applyDestination() {
        if (dest) js("setDestination(" + dest.lng + "," + dest.lat + ")")
        else js("clearDestination()")
    }
    function onPickFromMap(lng, lat) {
        dest = { lng: Number(lng), lat: Number(lat), name: qsTr("选点位置") }
        applyDestination()
        pickActive = false
        js("setPickMode(false)")
    }
    Connections {
        target: authStore
        function onMapTileSourceChanged()    { root.applyTileTemplate() }
        function onMapTileCustomUrlChanged() { if (authStore.mapTileSource === "custom") root.applyTileTemplate() }
    }

    // ===================== 页面布局 =====================
    Rectangle { anchors.fill: parent; color: Theme.background }

    // —— 顶部：搜索栏 + 扫码 ——
    Rectangle {
        id: searchBar
        width: parent.width
        height: 54
        color: Theme.primary
        Row {
            anchors.fill: parent
            anchors.leftMargin: 16; anchors.rightMargin: 16
            spacing: 8
            Rectangle {
                width: parent.width - scanBtn.width - parent.spacing
                height: 36
                anchors.verticalCenter: parent.verticalCenter
                color: "#ffffff"; radius: 18
                Row {
                    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 8
                    spacing: 6
                    Text { text: "\u{1F50D}"; anchors.verticalCenter: parent.verticalCenter; font.pixelSize: 15 }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("搜索地址 / 充电站")
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                        elide: Text.ElideRight
                    }
                }
                MouseArea { anchors.fill: parent; onClicked: root.openSearch() }
            }
            Rectangle {
                id: scanBtn
                width: 44; height: 36
                anchors.verticalCenter: parent.verticalCenter
                color: "#ffffff"; radius: 18
                Text { anchors.centerIn: parent; text: "\u{1F4F1}"; font.pixelSize: 16 }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.stackView.push("qrc:/UserClient/qml/pages/ScanPage.qml")
                }
            }
        }
    }

    // —— 城市 + 天气 ——
    Rectangle {
        id: cityBar
        y: searchBar.height
        width: parent.width
        height: 28
        color: Theme.primary
        Row {
            anchors.fill: parent
            anchors.leftMargin: 20; anchors.rightMargin: 20
            spacing: 10
            Text { text: "\u{1F4CD} 北京市 · 朝阳区"; color: "#ffffff"; font.pixelSize: Theme.fontSizeTiny; anchors.verticalCenter: parent.verticalCenter }
            Item { width: 1; height: 1 }
            Text {
                text: root.weatherIcon(root.weather.condition) + "  "
                      + root.weatherText(root.weather.condition) + " · "
                      + Number(root.weather.temperature || 0).toFixed(1) + "℃"
                color: "#ffffff"; font.pixelSize: Theme.fontSizeTiny; anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // —— 地图区 ——
    Rectangle {
        id: mapArea
        y: cityBar.y + cityBar.height
        width: parent.width
        height: root.mapHeight
        clip: true
        color: "#e0e4e8"
        Behavior on height { NumberAnimation { duration: 240; easing.type: Easing.InOutQuad } }

        WebEngineView {
            id: mapView
            anchors.fill: parent
            url: "qrc:/UserClient/qml/map/home_map.html"
            backgroundColor: "transparent"
            onLoadingChanged: function (load) {
                if (load.status === WebEngineView.LoadSucceededStatus) {
                    root.mapReady = true
                    root.initMap()
                }
            }
            onTitleChanged: {
                var t = mapView.title
                if (t && t.indexOf("pick:") === 0) {
                    var parts = t.slice("pick:".length).split(":")
                    if (parts.length >= 3) {
                        var seq = parseInt(parts[2], 10) || 0
                        if (seq && seq !== root.lastPickSeq) {
                            root.lastPickSeq = seq
                            root.onPickFromMap(parts[0], parts[1])
                        }
                    }
                }
            }
        }

        // —— 导航与选点操作条 ——
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 46
            color: "#F7F9FC"
            border.color: Theme.border
            Row {
                anchors.fill: parent
                anchors.leftMargin: 16; anchors.rightMargin: 16
                spacing: 10
                Button {
                    id: navBtn
                    width: parent.width * 0.62
                    height: 34
                    anchors.verticalCenter: parent.verticalCenter
                    enabled: root.dest !== null
                    text: qsTr("到这里去")
                    onClicked: {
                        if (root.dest)
                            root.stackView.push("qrc:/UserClient/qml/pages/NavRoutePage.qml",
                                { fromLng: root.myLoc.lng, fromLat: root.myLoc.lat,
                                  toLng: root.dest.lng, toLat: root.dest.lat, toName: root.dest.name })
                    }
                    contentItem: Text {
                        text: navBtn.text; font.pixelSize: Theme.fontSizeBase; font.bold: true
                        color: navBtn.enabled ? "#ffffff" : "#B9C4D4"
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle { radius: Theme.radiusSmall; color: navBtn.enabled ? Theme.primary : "#E4E9F2" }
                }
                Button {
                    id: pickBtn
                    width: parent.width * 0.34
                    height: 34
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.pickActive ? qsTr("在地图上点选") : qsTr("选点")
                    onClicked: {
                        root.pickActive = !root.pickActive
                        root.js("setPickMode(" + root.pickActive + ")")
                    }
                    contentItem: Text {
                        text: pickBtn.text; font.pixelSize: Theme.fontSizeBase; font.bold: true
                        color: root.pickActive ? "#ffffff" : Theme.primary
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: root.pickActive ? Theme.danger : (Theme.primary + "14")
                        border.width: root.pickActive ? 0 : 1; border.color: Theme.primary
                    }
                }
            }
        }
    }

    // —— 折叠箭头 ——
    Rectangle {
        id: divider
        y: mapArea.y + mapArea.height
        width: parent.width
        height: 26
        color: Theme.background
        border.color: Theme.border
        Row {
            anchors.centerIn: parent
            spacing: 6
            Text {
                text: root.dest ? qsTr("目标已标记") : qsTr("附近电站")
                color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: root.mapHeight > 0 ? "▲" : "▼"
                color: Theme.primary; font.pixelSize: 14; font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: root.mapHeight = (root.mapHeight > 0) ? 0 : 320
        }
    }

    // —— 下半部分 ——
    Item {
        id: lower
        y: divider.y + divider.height
        width: parent.width
        height: parent.height - lower.y
        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // —— 正在充电栏 ——
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: root.chargingOrders.length ? 72 : 34
                color: Theme.card
                border.color: Theme.border
                border.width: 1
                radius: Theme.radiusSmall
                Text {
                    visible: !root.chargingOrders.length
                    anchors.centerIn: parent
                    text: qsTr("当前没有正在充电的订单")
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                }
                ListView {
                    id: chargingListView
                    visible: root.chargingOrders.length
                    anchors.fill: parent
                    orientation: Qt.Horizontal
                    clip: true
                    model: root.chargingOrders
                    spacing: 10
                    topMargin: 6; bottomMargin: 6
                    leftMargin: 10; rightMargin: 10
                    delegate: Rectangle {
                        width: 300
                        height: chargingListView.height - 12
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.primary
                        radius: Theme.radiusSmall
                        Column {
                            anchors.left: parent.left; anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 12; anchors.rightMargin: 12
                            spacing: 4
                            Row {
                                spacing: 8; width: parent.width
                                Text { text: "\u{26A1}"; font.pixelSize: 14 }
                                Text {
                                    width: 210
                                    text: modelData.station_name || ""
                                    color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeSmall
                                    elide: Text.ElideRight
                                }
                            }
                            Row { spacing: 12
                                Text { text: modelData.charger_code || ""; color: "#E6F0FF"; font.pixelSize: Theme.fontSizeTiny }
                                Text { text: (modelData.charger_type === "fast" ? qsTr("快充") : qsTr("慢充")); color: "#E6F0FF"; font.pixelSize: Theme.fontSizeTiny }
                                Text {
                                    text: qsTr("电量 ") + Number(modelData.start_soc || 0) + "% → " + Number(modelData.target_soc || 0) + "%"
                                    color: "#ffffff"; font.pixelSize: Theme.fontSizeTiny; font.bold: true
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.stackView.push("qrc:/UserClient/qml/pages/OrderPage.qml",
                                            { initialStatus: "charging", selectedOrderId: modelData.id })
                        }
                    }
                }
            }

            // —— 筛选栏（横向滑动）——
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                color: Theme.card
                border.color: Theme.border
                border.width: 1
                Flickable {
                    anchors.fill: parent
                    contentWidth: filterRow.width
                    contentHeight: parent.height
                    clip: true
                    Row {
                        id: filterRow
                        height: parent.height
                        spacing: 8
                        ComboBox {
                            id: sortCombo
                            height: 36
                            anchors.verticalCenter: parent.verticalCenter
                            model: root.sortLabels
                            currentIndex: root.sortIdx
                            onActivated: function (idx) { root.sortIdx = idx; root.recompute() }
                            background: Rectangle { color: Theme.background; border.color: Theme.border; radius: Theme.radiusSmall }
                            contentItem: Text {
                                leftPadding: 10; rightPadding: 6
                                text: sortCombo.displayText; color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeTiny; verticalAlignment: Text.AlignVCenter
                            }
                        }
                        ComboBox {
                            id: vehicleCombo
                            height: 36
                            anchors.verticalCenter: parent.verticalCenter
                            model: root.vehicleLabels
                            currentIndex: root.vehicleIdx
                            onActivated: function (idx) { root.vehicleIdx = idx }
                            background: Rectangle { color: Theme.background; border.color: Theme.border; radius: Theme.radiusSmall }
                            contentItem: Text {
                                leftPadding: 10; rightPadding: 6
                                text: vehicleCombo.displayText; color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeTiny; verticalAlignment: Text.AlignVCenter
                            }
                        }
                        CheckBox { text: qsTr("只看自营"); anchors.verticalCenter: parent.verticalCenter
                            checked: root.onlySelf; onToggled: { root.onlySelf = checked; root.recompute() } }
                        CheckBox { text: qsTr("有厕所"); anchors.verticalCenter: parent.verticalCenter
                            checked: root.needWashroom; onToggled: { root.needWashroom = checked; root.recompute() } }
                        CheckBox { text: qsTr("有雨棚"); anchors.verticalCenter: parent.verticalCenter
                            checked: root.needShelter; onToggled: { root.needShelter = checked; root.recompute() } }
                        CheckBox { text: qsTr("我的收藏"); anchors.verticalCenter: parent.verticalCenter
                            checked: root.onlyFavorite; onToggled: { root.onlyFavorite = checked; root.recompute() } }
                    }
                }
            }

            // —— 智能推荐 / 现在就充（占位）——
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                color: Theme.card
                border.color: Theme.border
                border.width: 1
                Row {
                    anchors.fill: parent
                    anchors.topMargin: 2; anchors.bottomMargin: 2
                    anchors.leftMargin: 12; anchors.rightMargin: 12
                    spacing: 10
                    Button {
                        width: (parent.width - parent.spacing) / 2
                        height: 34; anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("🤖 智能推荐")
                        onClicked: root.showToast(qsTr("智能推荐功能建设中（机器学习未接入）"))
                        background: Rectangle { color: Theme.accent + "22"; radius: Theme.radiusSmall; border.color: Theme.accent; border.width: 1 }
                        contentItem: Text { text: parent.text; color: Theme.primary; font.pixelSize: Theme.fontSizeBase; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                    Button {
                        width: (parent.width - parent.spacing) / 2
                        height: 34; anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("⚡ 现在就充")
                        onClicked: root.showToast(qsTr("现在充功能建设中（机器学习未接入）"))
                        background: Rectangle { color: Theme.primary; radius: Theme.radiusSmall }
                        contentItem: Text { text: parent.text; color: "#ffffff"; font.pixelSize: Theme.fontSizeBase; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                }
            }

            // —— 电站列表栏 ——
            ListView {
                id: stationList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: listModel
                spacing: 12
                topMargin: 6
                bottomMargin: 12

                delegate: Rectangle {
                    property var listTags: [
                        model.data.owner_label || "",
                        (model.free > 0 ? qsTr("有空闲") : qsTr("已满")),
                        (model.data.has_swap ? qsTr("换电") : "")
                    ]
                    width: stationList.width - 24
                    x: 12
                    height: cardCol.implicitHeight + 24
                    color: Theme.card
                    radius: Theme.radiusSmall
                    border.color: Theme.border

                    Column {
                        id: cardCol
                        width: parent.width - 24
                        anchors.left: parent.left; anchors.leftMargin: 12
                        anchors.top: parent.top; anchors.topMargin: 10
                        spacing: 6
                        Row {
                            width: parent.width; spacing: 8
                            Text {
                                width: parent.width - 96
                                text: model.data.name || ""
                                font.pixelSize: Theme.fontSizeBase + 1; font.bold: true; color: Theme.textPrimary
                                elide: Text.ElideRight
                            }
                            Row {
                                anchors.verticalCenter: parent.verticalCenter; spacing: 2
                                Text { text: "⭐"; font.pixelSize: 14; color: "#F59E0B" }
                                Text { text: Number(model.data.rating || 0).toFixed(1); font.pixelSize: Theme.fontSizeSmall; font.bold: true; color: Theme.textPrimary }
                            }
                        }
                        Text {
                            width: parent.width
                            text: model.data.address || ""
                            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                            elide: Text.ElideRight
                        }
                        Row {
                            width: parent.width; spacing: 6
                            Repeater {
                                model: listTags
                                delegate: Rectangle {
                                    visible: modelData !== ""
                                    height: 20; width: txt.implicitWidth + 12; radius: 10
                                    color: Theme.primary + "14"
                                    Text {
                                        id: txt
                                        anchors.centerIn: parent
                                        text: modelData; color: Theme.primary
                                        font.pixelSize: 10; font.bold: true
                                    }
                                }
                            }
                        }
                    }
                    Row {
                        anchors.right: parent.right; anchors.bottom: parent.bottom
                        anchors.rightMargin: 12; anchors.bottomMargin: 8
                        spacing: 14
                        Text { text: "\u{FFE5}" + model.start_price.toFixed(2) + qsTr(" 起/度"); color: Theme.danger; font.pixelSize: Theme.fontSizeSmall; font.bold: true }
                        Text {
                            text: qsTr("空闲 ") + model.free + "/" + (model.data.total_chargers || 0)
                            color: model.free > 0 ? Theme.success : Theme.warn
                            font.pixelSize: Theme.fontSizeSmall; font.bold: true
                        }
                        Text { text: root.distText(model.distance_km); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.stackView.push("qrc:/UserClient/qml/pages/StationDetailPage.qml",
                                        { stationId: model.data.id })
                    }
                }
            }
        }
    }

    // ===================== 搜索覆盖层 =====================
    property bool searchOpen: false
    function openSearch() {
        searchInput.text = ""
        searchResults()
        searchInput.forceActiveFocus()
        searchOpen = true
    }
    Rectangle {
        visible: root.searchOpen
        anchors.fill: parent
        color: "#66000000"
        z: 60
        MouseArea { anchors.fill: parent; onClicked: root.searchOpen = false }
    }
    Rectangle {
        visible: root.searchOpen
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 300
        color: Theme.card
        z: 61
        Row {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 16; anchors.rightMargin: 8; anchors.topMargin: 12
            spacing: 8
            TextField {
                id: searchInput
                width: parent.width - cancelBtn.width - parent.spacing
                height: 40
                placeholderText: qsTr("搜索地址 / 充电站")
                background: Rectangle { radius: Theme.radiusSmall; color: Theme.background; border.color: Theme.border; border.width: 1 }
                onTextChanged: root.searchResults()
            }
            Button {
                id: cancelBtn
                width: 56; height: 40
                text: qsTr("取消")
                onClicked: root.searchOpen = false
                contentItem: Text { text: cancelBtn.text; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: "transparent" }
            }
        }
        ListView {
            id: searchList
            anchors.top: parent.top; anchors.topMargin: 60
            anchors.left: parent.left; anchors.right: parent.right
            anchors.bottom: parent.bottom
            model: searchModel
            clip: true
            delegate: Rectangle {
                width: searchList.width
                height: 58
                color: Theme.card
                border.color: Theme.border
                border.width: 1
                Column {
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 20; anchors.rightMargin: 20
                    spacing: 3
                    Text {
                        width: parent.width
                        text: model.name
                        font.pixelSize: Theme.fontSizeBase; font.bold: true; color: Theme.textPrimary
                        elide: Text.ElideRight
                    }
                    Text {
                        visible: !!model.address
                        width: parent.width
                        text: model.address + "  ·  " + root.distText(model.distance_km)
                        font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary
                        elide: Text.ElideRight
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (!model.empty) {
                            root.dest = { lng: model.lng, lat: model.lat, name: model.name }
                            root.applyDestination()
                            root.flyTo(model.lng, model.lat, 15)
                        }
                        root.searchOpen = false
                    }
                }
            }
        }
    }
    ListModel { id: searchModel }
    function searchResults() {
        searchModel.clear()
        var kw = searchInput.text.trim().toLowerCase()
        if (!kw) return
        var n = 0
        for (var i = 0; i < stationsAll.length && n < 8; i++) {
            var st = stationsAll[i]
            var name = (st.name || "").toLowerCase()
            var addr = (st.address || "").toLowerCase()
            if (name.indexOf(kw) >= 0 || addr.indexOf(kw) >= 0) {
                searchModel.append({
                    name: st.name || "", address: st.address || "",
                    lng: st.longitude, lat: st.latitude,
                    distance_km: haversine(myLoc.lng, myLoc.lat, Number(st.longitude), Number(st.latitude))
                })
                n++
            }
        }
        if (!n)
            searchModel.append({ name: qsTr("未找到匹配的电站"), address: "", lng: 0, lat: 0, distance_km: 0, empty: true })
    }

    // ===================== toast =====================
    function showToast(msg) { toastLbl.text = msg; toastBox.visible = true; toastTimer.restart() }
    Rectangle {
        id: toastBox
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 40
        width: toastLbl.implicitWidth + 24
        height: 34
        radius: 17
        color: "#E6000000"
        visible: false
        z: 70
        Text {
            id: toastLbl
            anchors.centerIn: parent
            color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall
        }
    }
    Timer { id: toastTimer; interval: 1500; onTriggered: toastBox.visible = false }

    Component.onCompleted: root.recompute()
}