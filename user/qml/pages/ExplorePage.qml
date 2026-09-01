import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine
import UserClient

// 探索页：整页地图为底 + 悬浮/下层面板。
// 主状态（grid/map/sheet）见 AGENTS 设计。
// 本版新增：
//   · enterMap 后显示「顶部工具栏」：城市按钮 + 归属筛选 + 评分筛选 + 换电勾选
//   · 点击城市 → 弹出「省-市-区」三级级联面板
//   · 地图 marker 按「缩放阈值 + 筛选条件」显示，点击弹出「电站详情」下层面板
Item {
    id: root
    property int navIndex: 1

    // —— 主状态 ——
    property bool gridVisible: true
    property bool mapInteractive: false
    property string activeSheet: ""      // "" | community | planner | power | cityPicker | stationDetail
    readonly property int sheetHeight: Math.round(root.height * 0.6)
    readonly property int stationHeight: Math.round(root.height * 0.7) // 详情稍高
    readonly property int cityPickerHeight: Math.round(root.height * 0.55)

    property var sheetTitles: ({})
    Component.onCompleted: {
        sheetTitles = {
            "community":     qsTr("社区"),
            "planner":       qsTr("旅行规划器"),
            "power":         qsTr("电力世界"),
            "cityPicker":    qsTr("选择城市"),
            "stationDetail": qsTr("电站详情")
        }
    }

    // —— 筛选/区域 UI 状态（在 QML 层持有，写入 JS）——
    readonly property var stationsAll: ExploreData.stations()
    readonly property var ownerOptions: [
        { label: qsTr("不限归属"),         value: "all" },
        { label: qsTr("仅自营"),           value: "self_run" },
        { label: qsTr("商户: 国家电网"),    value: "merchant:1" },
        { label: qsTr("商户: 特来电特许"),  value: "merchant:2" },
        { label: qsTr("商户: 星星万达"),    value: "merchant:3" }
    ]
    readonly property var ratingOptions: [
        { label: qsTr("不限评分"),       value: 0 },
        { label: qsTr("评分 ≥ 4.5"),     value: 4.5 },
        { label: qsTr("评分 ≥ 4.0"),     value: 4.0 },
        { label: qsTr("评分 ≥ 3.0"),     value: 3.0 }
    ]
    property int ownerIdx: 0
    property int ratingIdx: 0
    property bool swapOnly: false

    // 区域（城市级联）：空串表示不限制
    property string areaProvince: ""
    property string areaCity: ""
    property string areaDistrict: ""
    function areaLabel() {
        if (areaDistrict) return areaProvince + " · " + areaCity + " · " + areaDistrict
        if (areaCity)     return areaProvince + " · " + areaCity
        if (areaProvince) return areaProvince
        return qsTr("全国")
    }

    // —— 详情：选中电站 ID（marker 点击后写入）——
    property int selectedStationId: 0
    function selectedStation() { return ExploreData.stationById(selectedStationId) || ({}) }

    // —— 地图背景（MapLibre GL JS） ——
    WebEngineView {
        id: mapView
        anchors.fill: parent
        url: "qrc:/UserClient/qml/map/explore_map.html"
        backgroundColor: Theme.background
        // 有 sheet 升起时停止渲染（仅遮挡），不释放对象
        visible: activeSheet === ""

        onLoadingChanged: function (load) {
            if (load.status === WebEngineView.LoadSucceededStatus) {
                mapReady = true
                applyTileSource()
                applyMapInteractive()
                applyStations()
                exposeStationCallback()
                applyFilter()
            }
        }
        // 拦截 HTML 发来的 station:<id> 标题（避免走 GIO 拦截 station:// 伪协议）
        onTitleChanged: {
            var t = mapView.title
            if (t.indexOf("station:") === 0) {
                var parts = t.slice("station:".length).split(":")
                var id = parseInt(parts[0], 10) || 0
                if (id > 0) {
                    selectedStationId = id
                    activeSheet = "stationDetail"
                }
            }
        }
    }

    // —— 瓦片源 ——
    readonly property var presets: JSON.parse(authStore.mapTilePresetsJson())
    property bool mapReady: false
    function templateForCurrent() {
        var id = authStore.mapTileSource
        if (id === "custom") return authStore.mapTileCustomUrl || ""
        for (var i = 0; i < presets.length; i++)
            if (presets[i].id === id) return presets[i].template
        return ""
    }
    function applyTileSource() {
        if (!mapReady) return
        mapView.runJavaScript("setTileTemplate(" + JSON.stringify(templateForCurrent()) + ")")
    }
    Connections {
        target: authStore
        function onMapTileSourceChanged()    { applyTileSource() }
        function onMapTileCustomUrlChanged() { if (authStore.mapTileSource === "custom") applyTileSource() }
    }

    // —— 交互/导航 ——
    function applyMapInteractive() {
        if (!mapReady) return
        mapView.runJavaScript("setInteractive(" + (mapInteractive ? "true" : "false") + ")")
    }
    function enterMap() { gridVisible = false; mapInteractive = true; applyMapInteractive() }
    function exitMap()  { gridVisible = true;  mapInteractive = false; applyMapInteractive() }
    function openSheet(name) { gridVisible = false; activeSheet = name }
    function closeSheet()    { activeSheet = "";    gridVisible = true }
    function onGridAction(a) {
        if (a === 0) enterMap()
        else if (a === 1) openSheet("community")
        else if (a === 2) openSheet("planner")
        else openSheet("power")
    }

    // —— 充电站数据 + 筛选注入 ——
    function applyStations() {
        if (!mapReady) return
        mapView.runJavaScript("setStations(" + JSON.stringify(stationsAll) + ")")
    }
    function ownerValue() { return ownerOptions[ownerIdx].value }
    function ratingValue() { return ratingOptions[ratingIdx].value }
    function applyFilter() {
        if (!mapReady) return
        var js = "setFilter("
               + JSON.stringify(ownerValue()) + ","
               + JSON.stringify(ratingValue()) + ","
               + JSON.stringify(swapOnly) + ","
               + JSON.stringify(areaProvince) + ","
               + JSON.stringify(areaCity) + ","
               + JSON.stringify(areaDistrict) + ")"
        mapView.runJavaScript(js)
    }
    // UI 状态变动 → 重新下推筛选
    onOwnerIdxChanged:   applyFilter()
    onRatingIdxChanged:  applyFilter()
    onSwapOnlyChanged:   applyFilter()
    onAreaProvinceChanged: applyFilter()
    onAreaCityChanged:     applyFilter()
    onAreaDistrictChanged: applyFilter()

    // —— 把 JS 里的 __onStationClicked 挂到 QML 回调 ——
    function exposeStationCallback() {
        if (!mapReady) return
        // 用 document.title 作为 HTML→QML 桥，避开 GIO 对 station:// 伪协议的拦截，也不会触发页面重载
        var code = "window.__stationClickSeq = 0;"
                 + "window.__onStationClicked = function(id) {"
                 +   "window.__stationClickSeq += 1;"
                 +   "document.title = 'station:' + id + ':' + window.__stationClickSeq;"
                 + "};"
        mapView.runJavaScript(code)
    }

    // —— 面板单元（须先于 2×2 面板声明） ——
    component GridTile: Rectangle {
        property string label
        property string icon
        property int action: -1
        Layout.fillWidth: true; Layout.fillHeight: true
        radius: Theme.radiusSmall; color: "transparent"
        Column {
            anchors.centerIn: parent; spacing: 8
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: parent.parent.icon;  font.pixelSize: 34; color: Theme.primary }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: parent.parent.label; font.pixelSize: Theme.fontSizeBase; font.bold: true; color: Theme.textPrimary }
        }
        MouseArea { anchors.fill: parent; onClicked: root.onGridAction(parent.action) }
    }

    // —— 通用小组件（供详情 sheet 等复用，避免在 component 里再次嵌套）——
    component SectionCard: Column {
        property string title; property string subtitle
        width: parent.width; spacing: 4
        Text { text: title; font.pixelSize: Theme.fontSizeSmall + 1; font.bold: true; color: Theme.textPrimary }
        Text { visible: !!subtitle; text: subtitle; font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary }
    }
    component TagChip: Rectangle {
        property string tagText
        property color tagColor: Theme.primary
        property bool small: false
        height: small ? 20 : 24
        width: label.implicitWidth + (small ? 14 : 20)
        radius: small ? 10 : 12
        color: tagColor + "1A"; border.color: tagColor; border.width: 1
        Text {
            id: label; anchors.centerIn: parent
            text: parent.tagText; color: parent.tagColor
            font.pixelSize: small ? 10 : Theme.fontSizeTiny; font.bold: true
        }
    }
    component KeyValue: Row {
        property string k; property string v; property color c: Theme.textPrimary
        spacing: 6
        Text { text: parent.k + "："; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
        Text { text: parent.v; color: parent.c; font.pixelSize: Theme.fontSizeTiny; font.bold: (parent.c !== Theme.textPrimary) }
    }

    // —— 左上角返回箭头（进入地图模式） ——
    Rectangle {
        visible: !gridVisible && activeSheet === ""
        anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 16
        width: 44; height: 44; radius: 22
        color: Theme.card; border.color: Theme.border; z: 20
        Text { anchors.centerIn: parent; text: "‹"; font.pixelSize: 30; color: Theme.textPrimary }
        MouseArea { anchors.fill: parent; onClicked: exitMap() }
    }

    // —— 定位提示浮标 ——
    Rectangle {
        visible: !gridVisible && activeSheet === ""
        anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16
        width: locateText.implicitWidth + 20; height: 30
        color: Theme.card; border.color: Theme.border; radius: Theme.radiusSmall; z: 20
        Text { id: locateText; anchors.centerIn: parent; text: qsTr("定位由软件模拟"); font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary }
    }

    // =========================================================
    // 顶部工具栏（进入地图模式可见）：城市 + 筛选 + 换电
    // =========================================================
    Rectangle {
        visible: !gridVisible && activeSheet === ""
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: parent.top; anchors.topMargin: 72
        anchors.leftMargin: 16; anchors.rightMargin: 16
        z: 18
        color: Theme.card; border.color: Theme.border; radius: Theme.radiusSmall
        implicitHeight: col.height + 20

        Column {
            id: col
            width: parent.width
            spacing: 10
            anchors.top: parent.top; anchors.topMargin: 10
            anchors.left: parent.left; anchors.leftMargin: 10
            anchors.right: parent.right; anchors.rightMargin: 10

            // —— 第一行：城市按钮 + 换电勾选 ——
            RowLayout { width: parent.width; spacing: 10
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    height: 40
                    color: Theme.background; border.color: Theme.border; radius: Theme.radiusSmall
                    Row {
                        anchors.fill: parent; anchors.margins: 8; spacing: 6
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "\u{1F4CD}"; font.pixelSize: 18
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            Layout.fillWidth: true
                            text: areaLabel(); elide: Text.ElideRight
                            color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "▾"; color: Theme.textSecondary; font.pixelSize: 14
                        }
                    }
                    MouseArea { anchors.fill: parent; onClicked: openSheet("cityPicker") }
                }

                // 支持换电 勾选
                Row {
                    Layout.preferredWidth: 120; height: 40; spacing: 6
                    Rectangle {
                        width: 20; height: 20
                        anchors.verticalCenter: parent.verticalCenter
                        radius: 4
                        color: root.swapOnly ? Theme.primary : Theme.card
                        border.color: root.swapOnly ? Theme.primary : Theme.border
                        Text {
                            anchors.centerIn: parent; text: "✓"; color: "#ffffff"; font.pixelSize: 14; font.bold: true
                            visible: root.swapOnly
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.swapOnly = !root.swapOnly
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("支持换电")
                        font.pixelSize: Theme.fontSizeSmall; color: Theme.textPrimary
                        MouseArea { anchors.fill: parent; onClicked: root.swapOnly = !root.swapOnly }
                    }
                }
            }

            // —— 第二行：归属筛选 + 评分筛选 ——
            RowLayout { width: parent.width; spacing: 10
                // 归属筛选
                ComboBox {
                    id: ownerCombo
                    Layout.fillWidth: true; height: 36
                    property var vals: root.ownerOptions
                    model: vals.map(function(p){ return p.label })
                    currentIndex: root.ownerIdx
                    background: Rectangle { color: Theme.background; border.color: Theme.border; radius: Theme.radiusSmall }
                    contentItem: Text {
                        text: control.displayText; color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeSmall
                        leftPadding: 10; verticalAlignment: Text.AlignVCenter
                    }
                    onActivated: function (idx) { root.ownerIdx = idx }
                }
                // 评分筛选
                ComboBox {
                    id: ratingCombo
                    Layout.fillWidth: true; height: 36
                    property var vals: root.ratingOptions
                    model: vals.map(function(p){ return p.label })
                    currentIndex: root.ratingIdx
                    background: Rectangle { color: Theme.background; border.color: Theme.border; radius: Theme.radiusSmall }
                    contentItem: Text {
                        text: control.displayText; color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeSmall
                        leftPadding: 10; verticalAlignment: Text.AlignVCenter
                    }
                    onActivated: function (idx) { root.ratingIdx = idx }
                }
            }
        }
    }

    // —— 2×2 悬浮面板 ——
    Rectangle {
        id: gridPanel
        visible: gridVisible
        anchors.centerIn: parent
        width: Math.min(root.width * 0.72, 300)
        height: width
        color: Theme.card; border.color: Theme.border; radius: Theme.radius; z: 30
        GridLayout {
            anchors.fill: parent; anchors.margins: 8
            columns: 2; columnSpacing: 8; rowSpacing: 8
            GridTile { label: qsTr("进入地图");   icon: "\u{1F5FA}\u{FE0F}"; action: 0 }
            GridTile { label: qsTr("社区");       icon: "\u{1F465}";        action: 1 }
            GridTile { label: qsTr("旅行规划器"); icon: "\u{1F9F3}";        action: 2 }
            GridTile { label: qsTr("电力世界");   icon: "\u{26A1}";         action: 3 }
        }
    }

    // =========================================================
    // 通用 sheet：从下升起（根据 activeSheet 决定高度与内容委托）
    // =========================================================
    Loader {
        id: sheetLoader
        width: parent.width
        height: {
            if (activeSheet === "stationDetail") return stationHeight
            if (activeSheet === "cityPicker")    return cityPickerHeight
            return sheetHeight
        }
        y: activeSheet === "" ? root.height : root.height - height
        z: 40
        sourceComponent: {
            if (activeSheet === "cityPicker")    return cityPickerSheet
            if (activeSheet === "stationDetail") return stationDetailSheet
            return placeholderSheet
        }
        Behavior on y {
            enabled: activeSheet !== "stationDetail"
            NumberAnimation { duration: 260; easing.type: Easing.OutCubic }
        }
    }

    // 社区/旅行规划器/电力世界 占位 sheet
    Component {
        id: placeholderSheet
        Rectangle {
            color: Theme.background; radius: Theme.radius
            // 返回按钮
            Rectangle {
                anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 16
                width: 40; height: 40; radius: 20
                color: Theme.card; border.color: Theme.border
                Text { anchors.centerIn: parent; text: "‹"; font.pixelSize: 26; color: Theme.textPrimary }
                MouseArea { anchors.fill: parent; onClicked: root.closeSheet() }
            }
            Column {
                anchors.top: parent.top; anchors.topMargin: 18; anchors.horizontalCenter: parent.horizontalCenter
                spacing: 4
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: root.sheetTitles[root.activeSheet] || ""; font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.textPrimary }
            }
            Text { anchors.centerIn: parent; anchors.verticalCenterOffset: 20; text: qsTr("内容建设中"); color: Theme.textSecondary }
        }
    }

    // ———————————————————————————————————————————————————
    // 城市选择 下层面板（省-市-区 三级级联）
    // ———————————————————————————————————————————————————
    Component {
        id: cityPickerSheet
        Rectangle {
            color: Theme.background; radius: Theme.radius

            readonly property var regions: ExploreData.regionsTree()
        property int pIdx: 0
        property int cIdx: 0
        property int dIdx: 0
        function currentProvinceName() { return regions[pIdx] ? regions[pIdx].name : "" }
        function currentCities()      { return regions[pIdx] ? regions[pIdx].cities : [] }
        function currentCityName()    { return currentCities()[cIdx] ? currentCities()[cIdx].name : "" }
        function currentDistricts()   { return currentCities()[cIdx] ? currentCities()[cIdx].districts : [] }
        function currentDistrictName(){ return currentDistricts()[dIdx] ? currentDistricts()[dIdx].name : "" }

        // 返回按钮
        Rectangle {
            anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 16
            width: 40; height: 40; radius: 20
            color: Theme.card; border.color: Theme.border
            Text { anchors.centerIn: parent; text: "‹"; font.pixelSize: 26; color: Theme.textPrimary }
            MouseArea { anchors.fill: parent; onClicked: root.closeSheet() }
        }

        Text {
            anchors.top: parent.top; anchors.topMargin: 24
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("选择省 / 市 / 区县")
            font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.textPrimary
        }

        RowLayout {
            id: pickerRow
            anchors.top: parent.top; anchors.topMargin: 64
            anchors.left: parent.left; anchors.leftMargin: 16
            anchors.right: parent.right; anchors.rightMargin: 16
            anchors.bottom: applyBtn.top; anchors.bottomMargin: 16
            spacing: 8

            ListView {
                Layout.fillHeight: true; Layout.preferredWidth: parent ? parent.width/3 : 120
                clip: true
                model: regions.map(function(p){ return p.name })
                delegate: Rectangle {
                    width: ListView.view.width; height: 40
                    color: index === pIdx ? Theme.primaryLight : "transparent"
                    Text {
                        anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left
                        anchors.leftMargin: 12
                        text: modelData
                        color: index === pIdx ? "#ffffff" : Theme.textPrimary
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: index === pIdx
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            pIdx = index; cIdx = 0; dIdx = 0
                        }
                    }
                }
                ScrollIndicator.vertical: ScrollIndicator { }
            }
            ListView {
                Layout.fillHeight: true; Layout.fillWidth: true
                clip: true
                model: currentCities().map(function(c){ return c.name })
                delegate: Rectangle {
                    width: ListView.view.width; height: 40
                    color: index === cIdx ? Theme.primaryLight : "transparent"
                    Text {
                        anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left
                        anchors.leftMargin: 12
                        text: modelData
                        color: index === cIdx ? "#ffffff" : Theme.textPrimary
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: index === cIdx
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { cIdx = index; dIdx = 0 }
                    }
                }
                ScrollIndicator.vertical: ScrollIndicator { }
            }
            ListView {
                Layout.fillHeight: true; Layout.fillWidth: true
                clip: true
                model: currentDistricts().map(function(d){ return d.name })
                delegate: Rectangle {
                    width: ListView.view.width; height: 40
                    color: index === dIdx ? Theme.primaryLight : "transparent"
                    Text {
                        anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left
                        anchors.leftMargin: 12
                        text: modelData
                        color: index === dIdx ? "#ffffff" : Theme.textPrimary
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: index === dIdx
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { dIdx = index }
                    }
                }
                ScrollIndicator.vertical: ScrollIndicator { }
            }
        }

        // 确定：写入 root 区域 + 飞镜头
        Button {
            id: applyBtn
            anchors.left: parent.left; anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 24; anchors.rightMargin: 24; anchors.bottomMargin: 24
            height: 48
            text: qsTr("确定并跳转")
            background: Rectangle { radius: Theme.radiusSmall; color: Theme.primary }
            contentItem: Text {
                text: parent.text; color: "#fff"
                font.pixelSize: Theme.fontSizeBase; font.bold: true
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                root.areaProvince = currentProvinceName()
                root.areaCity     = currentCityName()
                root.areaDistrict = currentDistrictName()
                // 跳转到该区县中心（若 district 为空，退化为 city 第一个区；再空退化 province 首城市首区）
                var center
                var d = currentDistricts()[dIdx]
                if (d) center = { lng: d.lng, lat: d.lat }
                else {
                    var districts = currentDistricts()
                    if (districts && districts.length)
                        center = { lng: districts[0].lng, lat: districts[0].lat }
                    else {
                        // 空时按首城市/首区兜底
                        var cs = currentCities()
                        if (cs && cs.length && cs[0].districts && cs[0].districts.length)
                            center = { lng: cs[0].districts[0].lng, lat: cs[0].districts[0].lat }
                    }
                }
                if (center && root.mapReady)
                    mapView.runJavaScript("flyTo(" + center.lng + "," + center.lat + ", 12)")
                root.closeSheet()
            }
        }
        } // close Rectangle (cityPicker body)
    }     // close Component (cityPickerSheet)

    // ———————————————————————————————————————————————————
    // 电站详情 下层面板（信息/电桩/天气/价格/评价 满字段）
    // ———————————————————————————————————————————————————
    Component {
        id: stationDetailSheet
        Rectangle {
            color: Theme.background; radius: Theme.radius
            readonly property var st: root.selectedStation()
            readonly property var chargers:   ExploreData.chargersForStation(root.selectedStationId)
            readonly property var priceRules: ExploreData.priceRulesForStation(root.selectedStationId)
            readonly property var reviews:    ExploreData.reviewsForStation(root.selectedStationId)
            readonly property var weather:    ExploreData.weatherForArea(st.weather_area || st.area || "")

        // 返回按钮
        Rectangle {
            anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 16
            width: 40; height: 40; radius: 20
            color: Theme.card; border.color: Theme.border
            Text { anchors.centerIn: parent; text: "‹"; font.pixelSize: 26; color: Theme.textPrimary }
            MouseArea { anchors.fill: parent; onClicked: root.closeSheet() }
        }

        ScrollView {
            id: sv
            anchors.top: parent.top; anchors.bottom: parent.bottom
            anchors.left: parent.left; anchors.right: parent.right
            anchors.topMargin: 12; anchors.leftMargin: 16; anchors.rightMargin: 16; anchors.bottomMargin: 16
            clip: true

            Column {
                width: sv.width
                spacing: 16

                // —— 标题：站名 + 评分 ——
                Column {
                    width: parent.width; spacing: 6
                    RowLayout { width: parent.width; spacing: 10
                        Text {
                            Layout.fillWidth: true; elide: Text.ElideRight
                            text: st.name || qsTr("未知电站")
                            font.pixelSize: Theme.fontSizeTitle + 2; font.bold: true; color: Theme.textPrimary
                            wrapMode: Text.Wrap
                        }
                        Row { spacing: 2
                            Text { text: "⭐"; font.pixelSize: 16; color: "#F59E0B" }
                            Text { text: Number(st.rating||0).toFixed(1); font.bold: true; color: Theme.textPrimary }
                            Text { text: "(" + (st.rating_count||0) + ")"; font.pixelSize: Theme.fontSizeSmall; color: Theme.textSecondary }
                        }
                    }
                    Text {
                        width: parent.width; text: "\u{1F4CD} " + (st.address||"")
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                        wrapMode: Text.Wrap
                    }
                }

                // —— 状态标签：归属 / 空闲 / 换电 / 营业时间 / 停车费 ——
                Flow { width: parent.width; spacing: 8
                    TagChip { tagText: st.owner_label || ""; tagColor: Theme.primary }
                    TagChip { tagText: qsTr("空闲 ") + (st.fast_idle||0) + "快/" + (st.slow_idle||0) + "慢"; tagColor: (st.fast_idle||0)+(st.slow_idle||0) > 0 ? Theme.success : Theme.warn }
                    TagChip { visible: !!st.has_swap; tagText: qsTr("支持换电"); tagColor: Theme.success }
                    TagChip { visible: !!(st.merchant_name && st.owner_type!=="self_run"); tagText: qsTr("商户 ") + (st.merchant_name||""); tagColor: Theme.accent }
                    TagChip { tagText: qsTr("营业 ") + (st.business_hours||""); tagColor: Theme.textPrimary }
                    TagChip { tagText: qsTr("停车 ") + ((st.parking_fee||0) > 0 ? (st.parking_fee + " 元/h") : qsTr("免费")); tagColor: Theme.textPrimary }
                }

                // —— 天气（按区域）——
                SectionCard { title: qsTr("天气 & 时间"); subtitle: qsTr("模拟数据，取自 weather") }
                Row {
                    spacing: 16
                    Column { spacing: 2
                        Text { text: qsTr("天气"); font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary }
                        Row { spacing: 6
                            Text { text:
                                (weather.condition==="sunny") ? "☀️" :
                                (weather.condition==="cloudy")? "⛅" :
                                (weather.condition==="rain")  ? "🌧️" :
                                (weather.condition==="hot")   ? "🔥" :
                                (weather.condition==="extreme")? "🌪️" : "❔"
                            ; font.pixelSize: 22 }
                            Text { text:
                                (weather.condition==="sunny") ? qsTr("晴") :
                                (weather.condition==="cloudy")? qsTr("多云") :
                                (weather.condition==="rain")  ? qsTr("雨") :
                                (weather.condition==="hot")   ? qsTr("高温") :
                                (weather.condition==="extreme")? qsTr("极端") : weather.condition
                            ; font.pixelSize: Theme.fontSizeBase; color: Theme.textPrimary; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                        }
                    }
                    Column { spacing: 2
                        Text { text: qsTr("温度"); font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary }
                        Text { text: (weather.temperature||0).toFixed(1) + " ℃"; font.pixelSize: Theme.fontSizeTitle - 2; font.bold: true; color: Theme.textPrimary }
                    }
                    Column { spacing: 2
                        Text { text: qsTr("更新时间"); font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary }
                        Text { text: weather.update_time || "-"; font.pixelSize: Theme.fontSizeSmall; color: Theme.textPrimary }
                    }
                    Column { spacing: 2
                        Text { text: qsTr("当前时间"); font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary }
                        Text { text: Qt.formatDateTime(new Date(), "yyyy-MM-dd HH:mm"); font.pixelSize: Theme.fontSizeSmall; color: Theme.textPrimary }
                    }
                }

                // —— 分时电价 ——
                SectionCard { title: qsTr("分时电价"); subtitle: qsTr("price_rule 表数据，含服务费 ") + (st.service_fee||0) + " 元/kWh" }
                Column { width: parent.width; spacing: 6
                    Repeater {
                        model: priceRules
                        delegate: RowLayout { width: parent.width
                            Text {
                                Layout.preferredWidth: 80
                                text: {
                                    var lv = modelData.level || ""
                                    if (lv === "valley") return qsTr("谷")
                                    if (lv === "flat")   return qsTr("平")
                                    if (lv === "peak")   return qsTr("峰")
                                    return lv
                                }
                                font.bold: true
                                color: (modelData.level === "valley") ? Theme.success
                                     : (modelData.level === "peak")   ? Theme.danger
                                     : Theme.primary
                                font.pixelSize: Theme.fontSizeSmall
                            }
                            Text { Layout.fillWidth: true; text: modelData.time_range || ""; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall }
                            Text {
                                text: (Number(modelData.price||0) + Number(st.service_fee||0)).toFixed(2) + " 元/kWh"
                                font.bold: true; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall
                            }
                        }
                    }
                }

                // —— 电桩列表（charger 全字段概览）——
                SectionCard { title: qsTr("电桩（共 ") + (st.total_chargers||0) + qsTr(" 个）"); subtitle: qsTr("charger 表，含状态/功率/电压/电流/温度/健康") }
                Column { width: parent.width; spacing: 8
                    Repeater {
                        model: chargers
                        delegate: Rectangle {
                            width: parent.width
                            height: chargerInner.implicitHeight + 24
                            color: Theme.card; radius: Theme.radiusSmall
                            border.color: Theme.border
                            Column {
                                id: chargerInner
                                width: parent.width - 24
                                anchors.left: parent.left; anchors.leftMargin: 12
                                anchors.top: parent.top; anchors.topMargin: 12
                                spacing: 8
                                RowLayout { width: parent.width
                                    Text { text: modelData.code || ""; font.bold: true; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeBase }
                                    Item { Layout.fillWidth: true; Layout.fillHeight: true }
                                    TagChip { tagText: {
                                            var s = modelData.status || ""
                                            if (s === "idle")     return qsTr("空闲")
                                            if (s === "charging") return qsTr("充电中")
                                            if (s === "reserved") return qsTr("预约中")
                                            if (s === "fault")    return qsTr("故障")
                                            if (s === "offline")  return qsTr("离线")
                                            if (s === "rebooting")return qsTr("重启中")
                                            return s
                                        }
                                        tagColor: {
                                            var s = modelData.status || ""
                                            if (s === "idle")     return Theme.success
                                            if (s === "charging") return Theme.primary
                                            if (s === "reserved") return Theme.accent
                                            if (s === "fault" || s === "offline") return Theme.danger
                                            return Theme.warn
                                        } }
                                    TagChip { tagText: (modelData.type === "fast" ? qsTr("快充 ") : qsTr("慢充 ")) + (modelData.power||0) + "kW"
                                          tagColor: modelData.type === "fast" ? Theme.primary : Theme.textPrimary }
                                }
                                Flow { width: parent.width; spacing: 10
                                    KeyValue { k: qsTr("电压"); v: (modelData.voltage||0) + " V" }
                                    KeyValue { k: qsTr("电流"); v: (modelData.current||0) + " A" }
                                    KeyValue { k: qsTr("温度"); v: (modelData.temperature||0) + " ℃" }
                                    KeyValue { k: qsTr("健康"); v: (modelData.health_score||0) + "/100";
                                         c: (modelData.health_score||0)>=90 ? Theme.success : ((modelData.health_score||0)>=75 ? Theme.textPrimary : Theme.warn) }
                                    KeyValue { k: qsTr("累计"); v: (modelData.total_charge_count||0) + qsTr("次 / ") + (modelData.total_charge_duration||0) + qsTr("分钟") }
                                }
                                KeyValue { visible: !!modelData.fault_code
                                      k: qsTr("故障码")
                                      v: modelData.fault_code || ""; c: Theme.danger }
                                KeyValue { k: qsTr("通信"); v: (modelData.comm_status === "normal" ? qsTr("正常") : qsTr("异常"))
                                      c: modelData.comm_status === "normal" ? Theme.success : Theme.danger }
                                KeyValue { k: qsTr("建桩时间"); v: modelData.created_time || "-" }
                            }
                        }
                    }
                }

                // —— 设施 + 站点字段杂项 ——
                SectionCard { title: qsTr("站点详情"); subtitle: qsTr("station 其余字段 + facilities 数组") }
                Column { width: parent.width; spacing: 6
                    KeyValue { k: qsTr("区域"); v: st.area || "-" }
                    KeyValue { k: qsTr("经纬度"); v: (st.longitude||0).toFixed(4) + ", " + (st.latitude||0).toFixed(4) }
                    KeyValue { k: qsTr("在线率"); v: Number(st.online_rate||0).toFixed(1)*100 + " %" }
                    KeyValue { k: qsTr("服务费"); v: (st.service_fee||0) + " 元/kWh" }
                    KeyValue { k: qsTr("电桩统计"); v:
                         qsTr("快 ") + (st.fast_count||0) + " / 慢 " + (st.slow_count||0)
                         + "｜" + qsTr("充电中 ") + (st.charging_count||0)
                         + "｜" + qsTr("故障 ") + (st.fault_count||0)
                         + "｜" + qsTr("离线 ") + (st.offline_count||0)
                    }
                    KeyValue { k: qsTr("设施"); v: {
                            var arr = st.facilities || []
                            if (arr.length === 0) return qsTr("无")
                            return arr.map(function(f){
                                if (f==="washroom")            return qsTr("洗手间")
                                if (f==="convenience_store")   return qsTr("便利店")
                                if (f==="rest_area")           return qsTr("休息区")
                                if (f==="wifi")                return qsTr("WiFi")
                                if (f==="rain_shelter")        return qsTr("雨棚")
                                if (f==="underground_parking") return qsTr("地下车库")
                                return f
                            }).join("、")
                         } }
                }

                // —— 评价（review 6 维 + 标签）——
                SectionCard { title: qsTr("用户评价"); subtitle: qsTr("review 表 6 维评分 + 标签 + 有用数") }
                Column { width: parent.width; spacing: 10
                    Repeater {
                        model: reviews
                        delegate: Rectangle {
                            width: parent.width
                            height: reviewInner.implicitHeight + 24
                            color: Theme.card; radius: Theme.radiusSmall
                            border.color: Theme.border
                            Column {
                                id: reviewInner
                                width: parent.width - 24
                                anchors.left: parent.left; anchors.leftMargin: 12
                                anchors.top: parent.top; anchors.topMargin: 12
                                spacing: 8
                                RowLayout { width: parent.width
                                    Text { text: modelData.nickname || ""; font.bold: true; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall }
                                    Item { Layout.fillWidth: true; height: 1 }
                                    Text { text: "⭐ " + Number(modelData.overall_score||0).toFixed(1); color: "#F59E0B"; font.bold: true }
                                }
                                Row { spacing: 10
                                    KeyValue { k: qsTr("速度"); v: Number(modelData.speed_score||0).toFixed(1) }
                                    KeyValue { k: qsTr("设备"); v: Number(modelData.device_score||0).toFixed(1) }
                                    KeyValue { k: qsTr("停车"); v: Number(modelData.parking_score||0).toFixed(1) }
                                    KeyValue { k: qsTr("卫生"); v: Number(modelData.hygiene_score||0).toFixed(1) }
                                    KeyValue { k: qsTr("服务"); v: Number(modelData.service_score||0).toFixed(1) }
                                }
                                Text { width: parent.width; text: modelData.content || ""; wrapMode: Text.Wrap; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall }
                                RowLayout { width: parent.width; spacing: 6
                                    Repeater {
                                        model: modelData.tags || []
                                        delegate: TagChip { tagText: "#" + modelData; tagColor: Theme.accent; small: true }
                                    }
                                    Item { Layout.fillWidth: true; height: 1 }
                                    Text { text: qsTr("有用 ") + (modelData.useful_count||0); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
                                    Text { text: "  ·  " + (modelData.create_time||""); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
                                }
                            }
                        }
                    }
                }
                Text { text: qsTr("（更多字段后续接入：收藏、优惠券、预约/排队、钱包、积分）"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
            }
        }
        } // close Rectangle (stationDetail body)
    }     // close Component (stationDetailSheet)
}