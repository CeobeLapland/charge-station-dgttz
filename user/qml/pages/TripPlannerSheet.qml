import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine
import UserClient

// =========================================================
// 旅行规划器：选择车型 + 起点/目的地 + 经停（可多个），
// 用「续航范围 + 直距」贪心计算沿途需经过的充电站，
// 并在复用 MapLibre 地图上高亮，同时给出超续航风险提示。
//
// 接口预留（未来接真实数据）：
//  · kEffKmPerKwh / kSafety / kTargetSoc：真实车型续航系数，之后由服务端车型库下发
//  · km() 直距：之后替换为高德真实路线距离/时长（途径点坐标已就绪）
//  · 「打开导航」按钮：已接高德 web 深链（uri.amap.com/navigation），坐标即导航参数
// =========================================================
Item {
    id: root
    readonly property var stackView: StackView.view
    signal requestClose()

    // —— 数据与常量 ——
    property var allStns: ExploreData.stations()          // 全部电站（坐标即充电候选点）
    property var vehicles: UserData.vehicles()            // 用户车型库
    readonly property double kEffKmPerKwh: 5.5            // 每度电续航（示例效率常数）
    readonly property double kSafety: 0.85                // 续航安全系数（留冗余）
    readonly property double kTargetSoc: 90               // 途后充至目标电量 %
    property var cityCenters: [
        { name: "我的位置·北京", lng: 116.397128, lat: 39.916527 },
        { name: "北京市", lng: 116.397128, lat: 39.916527 },
        { name: "上海市", lng: 121.473701, lat: 31.230416 },
        { name: "深圳市", lng: 114.061944, lat: 22.543096 },
        { name: "杭州市", lng: 120.155070, lat: 30.274084 }
    ]
    property var places: []                                // [{name,lng,lat}]
    property var placeNames: []                            // ComboBox 模型（含"请选择…"）
    function buildPlaces() {
        var arr = []
        var seen = {}
        for (var i = 0; i < cityCenters.length; i++) {
            var c = cityCenters[i]
            if (!seen[c.name]) { seen[c.name] = true; arr.push({ name: c.name, lng: c.lng, lat: c.lat }) }
        }
        for (var j = 0; j < allStns.length; j++) {
            var s = allStns[j]
            var nm = s.name || ("电站" + s.id)
            if (seen[nm]) { nm = nm + "（" + s.area_city + "）" }
            seen[nm] = true
            arr.push({ name: nm, lng: Number(s.longitude), lat: Number(s.latitude), station: true, id: s.id })
        }
        places = arr
        placeNames = [qsTr("请选择…")].concat(arr.map(function (p) { return p.name }))
    }

    // —— 结果状态 ——
    property var stops: []          // [{type:'charge'|'risk'|'leg', name/from/to, distKm, socAtArrival, id}]
    property var mapIds: []
    property var routePts: []
    property bool hasComputed: false
    property bool hasRisk: false
    property string riskHint: ""
    property string status: ""      // 顶部/结果提示

    // 经停列表（下标对应的 ComboBox，-1 表示未选择）
    property var wayChosen: [ -1 ]

    // —— 工具函数 ——
    function km(lng1, lat1, lng2, lat2) {
        var R = 6371.0
        var dLat = (lat2 - lat1) * Math.PI / 180
        var dLng = (lng2 - lng1) * Math.PI / 180
        var a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
                Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
                Math.sin(dLng / 2) * Math.sin(dLng / 2)
        return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a))
    }
    function fmtKm(d) { return d >= 100 ? Math.round(d) + " km" : d.toFixed(1) + " km" }
    function socTxt(s) { return s < 0 ? "--" : Math.max(0, Math.round(s)) + "%" }

    function placeAt(i) { return (i > 0 && i - 1 < places.length) ? places[i - 1] : null }

    // 贪心规划：沿途最近可达电站充电，无站可达则标风险
    function compute() {
        var v = vehicles[vehCombo.currentIndex]
        if (!v) { status = qsTr("请选择车型"); return }
        var dest = placeAt(destCombo.currentIndex)
        var start = placeAt(startCombo.currentIndex)
        if (!start) { status = qsTr("请选择起点"); return }
        if (!dest)  { status = qsTr("请选择目的地"); return }
        if (dest.name === start.name) { status = qsTr("起点与目的地相同"); return }

        var fullKm = (v.battery_kwh || 0) * kEffKmPerKwh
        var soc = 100.0
        var pivots = [ { name: start.name, lng: start.lng, lat: start.lat } ]
        for (var w = 0; w < wayChosen.length; w++) {
            var wp = placeAt(wayChosen[w])
            if (wp) pivots.push({ name: wp.name, lng: wp.lng, lat: wp.lat })
        }
        pivots.push({ name: dest.name, lng: dest.lng, lat: dest.lat })

        var out = []
        var ids = []
        var usedIds = {}
        var pts = [ [start.lng, start.lat] ]
        var guard = 0

        for (var i = 0; i < pivots.length - 1 && guard < 60; i++) {
            var from = pivots[i]
            var to = pivots[i + 1]
            var dist = km(from.lng, from.lat, to.lng, to.lat)
            var avail = fullKm * (soc / 100) * kSafety

            if (dist <= avail) {
                soc -= (dist / fullKm) * 100
                out.push({ type: "leg", from: from.name, to: to.name, dist: dist, soc: soc })
                pts.push([to.lng, to.lat])
                continue
            }

            // 需要充电：找「当前可达」且尽量靠近下一目标 的电站（跳过已用过的）
            var best = null, bestScore = 1e18
            for (var j = 0; j < allStns.length; j++) {
                var s = allStns[j]
                if (usedIds[Number(s.id)]) continue
                var dCur = km(from.lng, from.lat, Number(s.longitude), Number(s.latitude))
                if (dCur > avail) continue
                var dNext = km(Number(s.longitude), Number(s.latitude), to.lng, to.lat)
                var score = dCur + dNext
                if (score < bestScore) { bestScore = score; best = s }
            }

            if (!best) {
                // 无可达电站 → 风险段
                out.push({ type: "risk", from: from.name, to: to.name, dist: dist, soc: soc })
                hasRisk = true
                riskHint = from.name + qsTr(" → ") + to.name + qsTr(" 途中没有任何「当前可达」电站，超出续航，需就近补电！")
                soc = 0
                pts.push([to.lng, to.lat])
                continue
            }

            var d1 = km(from.lng, from.lat, Number(best.longitude), Number(best.latitude))
            soc -= (d1 / fullKm) * 100
            out.push({
                type: "charge",
                name: best.name,
                dist: d1,
                socAtArrival: soc,
                id: Number(best.id)
            })
            ids.push(Number(best.id))
            usedIds[Number(best.id)] = true
            pts.push([Number(best.longitude), Number(best.latitude)])
            soc = kTargetSoc
            // 插入充电点作为中转，继续同一段
            pivots.splice(i + 1, 0, { name: best.name, lng: Number(best.longitude), lat: Number(best.latitude) })
            guard++
            i--   // 回退，重新算 from→(充电点)→to
        }
        // 终点标记
        pts.push([dest.lng, dest.lat])

        stops = out
        mapIds = ids
        routePts = JSON.parse(JSON.stringify(pts))
        hasComputed = true
        hasRisk = out.some(function (o) { return o.type === "risk" })
        riskHint = hasRisk ? riskHint : ""
        status = out.filter(function (o) { return o.type === "charge" }).length + qsTr(" 个沿途电站 · 全程 ") + Math.round(sumKm(out)) + " km"
        applyMap()
    }
    function sumKm(arr) {
        var t = 0
        for (var i = 0; i < arr.length; i++) if (arr[i].dist) t += arr[i].dist
        return t
    }

    // —— 地图（复用探索页 MapLibre） ——
    property bool mapReady: false
    property bool planPending: false
    function applyMap() {
        if (!mapReady) { planPending = true; return }
        planPending = false
        tripMap.runJavaScript("setStations(" + JSON.stringify(allStns) + ")")
        tripMap.runJavaScript("setPlannedStops(" + JSON.stringify(mapIds) + "," + JSON.stringify(routePts) + ")")
    }

    // =================== UI ===================
    Rectangle { anchors.fill: parent; color: Theme.background }

    // —— 顶栏（顶格，无圆角留白） ——
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
                Text { text: qsTr("旅行规划器"); color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true }
                Text { text: qsTr("选车型 · 定路线 · 沿途充电"); color: "#DDEeff"; font.pixelSize: 11; visible: true }
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

            // —— 表单卡 ——
            Rectangle {
                id: formCard
                width: parent.width - 28; x: 14
                height: fieldsCol.implicitHeight + 24
                color: Theme.card; border.color: Theme.border; radius: Theme.radiusSmall
                Column {
                    id: fieldsCol
                    width: parent.width - 24; x: 12; y: 12
                    spacing: 12
                    RowLayout { width: parent.width; spacing: 8
                        Text { text: qsTr("车型"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall }
                        ComboBox {
                            id: vehCombo
                            Layout.fillWidth: true; height: 38
                            model: vehicles.map(function (v) { return (v.name || qsTr("车辆")) + "（" + Math.round(v.battery_kwh || 0) + "kWh）" })
                            currentIndex: defaultVehicleIdx()
                            function defaultVehicleIdx() {
                                for (var i = 0; i < vehicles.length; i++)
                                    if (vehicles[i].type === "car" && (vehicles[i].is_default === 1 || vehicles[i].is_default === true))
                                        return i
                                return 0
                            }
                            background: Rectangle { color: Theme.background; border.color: Theme.border; radius: Theme.radiusSmall }
                            contentItem: Text {
                                text: parent.displayText; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall
                                leftPadding: 10; verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                    // 起点 / 目的地
                    RowLayout { width: parent.width; spacing: 8
                        Text { text: qsTr("起点"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall }
                        ComboBox {
                            id: startCombo
                            Layout.fillWidth: true; height: 38
                            model: root.placeNames
                            currentIndex: 1   // 默认“我的位置·北京”
                            onCurrentIndexChanged: clearResult()
                            background: Rectangle { color: Theme.background; border.color: Theme.border; radius: Theme.radiusSmall }
                            contentItem: Text {
                                text: parent.displayText; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall
                                leftPadding: 10; verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                    RowLayout { width: parent.width; spacing: 8
                        Text { text: qsTr("目的"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall }
                        ComboBox {
                            id: destCombo
                            Layout.fillWidth: true; height: 38
                            model: root.placeNames
                            onCurrentIndexChanged: clearResult()
                            background: Rectangle { color: Theme.background; border.color: Theme.border; radius: Theme.radiusSmall }
                            contentItem: Text {
                                text: parent.displayText; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall
                                leftPadding: 10; verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                    // 经停（可多个）
                    Text { text: qsTr("经停地（可选）"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall; topPadding: 2 }
                    Column {
                        width: parent.width; spacing: 8
                        Repeater {
                            model: root.wayChosen
                            delegate: RowLayout { width: parent.width; spacing: 8
                                ComboBox {
                                    id: wpCombo
                                    Layout.fillWidth: true; height: 38
                                    model: root.placeNames
                                    onCurrentIndexChanged: clearResult()
                                    background: Rectangle { color: Theme.background; border.color: Theme.border; radius: Theme.radiusSmall }
                                    contentItem: Text {
                                        text: parent.displayText; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall
                                        leftPadding: 10; verticalAlignment: Text.AlignVCenter
                                    }
                                    onActivated: function (idx) { root.wayChosen[index] = idx; root.wayChosen = root.wayChosen.slice() }
                                    Component.onCompleted: currentIndex = root.wayChosen[index]
                                }
                                Rectangle {
                                    Layout.preferredWidth: 34; height: 34; radius: 6
                                    color: Theme.danger + "22"
                                    Text { anchors.centerIn: parent; text: "✕"; color: Theme.danger; font.bold: true }
                                    MouseArea { anchors.fill: parent; onClicked: { root.wayChosen.splice(index, 1); root.wayChosen = root.wayChosen.slice(); clearResult() } }
                                }
                            }
                        }
                        Rectangle {
                            width: parent.width; height: 34; radius: 6
                            color: Theme.accent + "18"; border.color: Theme.accent; border.width: 1
                            Text { anchors.centerIn: parent; text: qsTr("＋ 添加经停"); color: Theme.primary; font.pixelSize: Theme.fontSizeSmall; font.bold: true }
                            MouseArea { anchors.fill: parent; onClicked: { root.wayChosen.push(-1); root.wayChosen = root.wayChosen.slice() } }
                        }
                    }
                    // 规划按钮
                    Rectangle {
                        width: parent.width; height: 46; radius: 23
                        gradient: Gradient { GradientStop { position: 0; color: Theme.primary } GradientStop { position: 1; color: Theme.accent } }
                        Text { anchors.centerIn: parent; text: qsTr("规划路线"); color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeBase }
                        MouseArea { anchors.fill: parent; onClicked: root.compute() }
                    }
                    // 状态提示
                    Text {
                        width: parent.width; wrapMode: Text.Wrap
                        visible: root.status !== ""
                        text: root.status
                        color: root.hasComputed ? (root.hasRisk ? Theme.danger : "#12B76A") : Theme.textSecondary
                        font.pixelSize: Theme.fontSizeTiny; font.bold: root.hasComputed
                    }
                }
            }

            // —— 风险提示条 ——
            Rectangle {
                width: parent.width - 28; x: 14
                visible: root.hasRisk
                color: Theme.danger + "1A"; border.color: Theme.danger; border.width: 1; radius: Theme.radiusSmall
                height: 46
                Row { anchors.fill: parent; anchors.margins: 12; spacing: 8
                    Text { text: "\u26A0"; color: Theme.danger; font.pixelSize: 18; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        width: parent.width - 28
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.riskHint; wrapMode: Text.Wrap; color: Theme.danger
                        font.pixelSize: Theme.fontSizeTiny; font.bold: true
                    }
                }
            }

            // —— 地图高亮 ——
            Rectangle {
                width: parent.width - 28; x: 14
                height: 220; radius: Theme.radiusSmall; clip: true
                border.color: Theme.border
                WebEngineView {
                    id: tripMap
                    anchors.fill: parent
                    url: "qrc:/UserClient/qml/map/explore_map.html"
                    visible: root.hasComputed
                    onLoadingChanged: function (load) {
                        if (load.status === WebEngineView.LoadSucceededStatus) { root.mapReady = true; root.applyMap() }
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: !root.hasComputed
                    text: qsTr("提交规划后，沿途充电站将在此地图高亮")
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                }
            }

            // —— 结果列表 ——
            Column {
                width: parent.width - 28; x: 14
                visible: root.hasComputed
                spacing: 8
                Text { text: qsTr("沿途结算") ; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall; font.bold: true }
                Repeater {
                    model: root.stops
                    delegate: Rectangle {
                        width: parent.width
                        height: Math.max(rowH.implicitHeight + 14, 46)
                        color: Theme.card; border.color: modelData.type === "risk" ? Theme.danger : Theme.border
                        radius: Theme.radiusSmall
                        Row {
                            id: rowH
                            anchors.fill: parent; anchors.margins: 12
                            spacing: 10
                            Rectangle {
                                width: 26; height: 26; radius: 13
                                anchors.verticalCenter: parent.verticalCenter
                                color: modelData.type === "risk" ? Theme.danger + "22" : (modelData.type === "charge" ? Theme.accent + "22" : Theme.primary + "18")
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.type === "charge" ? "⚡" : (modelData.type === "risk" ? "⚠" : "→")
                                    color: modelData.type === "risk" ? Theme.danger : (modelData.type === "charge" ? Theme.primary : Theme.textSecondary)
                                    font.pixelSize: 15
                                }
                            }
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 150
                                Text {
                                    width: parent.width; elide: Text.ElideRight
                                    text: modelData.type === "charge" ? (modelData.name || "") :
                                          (modelData.type === "risk" ? qsTr("超续航风险路段") : (modelData.from + " → " + modelData.to))
                                    font.pixelSize: Theme.fontSizeSmall; font.bold: true
                                    color: modelData.type === "risk" ? Theme.danger : Theme.textPrimary
                                }
                                Text {
                                    width: parent.width; elide: Text.ElideRight
                                    visible: modelData.type === "charge"
                                    text: qsTr("到站剩余 ") + root.socTxt(modelData.socAtArrival)
                                    font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary
                                }
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: (modelData.type === "charge" ? qsTr("充至 90%") : "") + (modelData.dist ? ("  " + root.fmtKm(modelData.dist)) : "")
                                font.pixelSize: Theme.fontSizeTiny
                                color: modelData.type === "risk" ? Theme.danger : Theme.textSecondary
                            }
                        }
                    }
                }
            }

            // —— 导航动作（接口占位；坐标已就绪，未来接高德路线） ——
            Rectangle {
                width: parent.width - 28; x: 14
                visible: root.hasComputed
                height: 44; radius: 22
                color: Theme.primary
                Text { anchors.centerIn: parent; text: qsTr("🧭 开始导航（高德）"); color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeBase }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (!mapIds.length) return
                        var last = allStns.filter(function (s) { return Number(s.id) === Number(mapIds[mapIds.length - 1]) })[0]
                        var placesForNav = mapIds.map(function (id) {
                            var s = allStns.filter(function (x) { return Number(x.id) === Number(id) })[0]
                            return s || null
                        }).filter(function (s) { return !!s })
                        // 终点取目的地电站；导航起点用“我的位置”
                        var destLast = placesForNav[placesForNav.length - 1] || last
                        stackView.push("qrc:/UserClient/qml/pages/NavRoutePage.qml", {
                            fromLng: 116.397128, fromLat: 39.916527,
                            toLng: Number(destLast.longitude), toLat: Number(destLast.latitude),
                            toName: destLast.name || qsTr("充电站")
                        })
                    }
                }
            }
        }
    }

    function clearResult() {
        hasComputed = false
        stops = []
        mapIds = []
        routePts = []
        hasRisk = false
        riskHint = ""
        status = ""
    }

    Component.onCompleted: { buildPlaces(); startCombo.currentIndex = 1 }
}