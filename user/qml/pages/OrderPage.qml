import QtQuick
import QtQuick.Controls
import UserClient

// 订单页：订单列表 + 订单详情（时间轴 + 智能充电报告 + 绿色贡献）。
// 数据来自 UserData（charging_order / order_timeline 对齐 DATA_STRUCTURE.md）。
// 列表可按状态筛选；点击进入详情（同一页面内覆盖层展示，通过 selectedOrderId 驱动）。
Item {
    id: root
    property string filterStatus: "all"   // all / reserved / charging / pending_settle / completed / cancelled
    property int selectedOrderId: 0
    readonly property var stackView: StackView.view

    // 碳排因子（spec 待定项建议初值 0.7 kg CO2/kWh）
    readonly property real carbonFactor: 0.7

    readonly property var allOrders: UserData.orders()

    function statusText(s) {
        if (s === "reserved")       return qsTr("预约中")
        if (s === "charging")       return qsTr("充电中")
        if (s === "pending_settle") return qsTr("待结算")
        if (s === "completed")      return qsTr("已完成")
        if (s === "cancelled")      return qsTr("已取消")
        return s
    }
    function statusColor(s) {
        if (s === "charging")       return Theme.primary
        if (s === "pending_settle") return Theme.warn
        if (s === "completed")      return Theme.success
        if (s === "cancelled")      return Theme.textSecondary
        return Theme.accent
    }
    function priceLevelText(lv) {
        if (lv === "valley") return qsTr("谷")
        if (lv === "flat")   return qsTr("平")
        if (lv === "peak")   return qsTr("峰")
        return ""
    }
    function typeText(t) { return t === "fast" ? qsTr("快充") : qsTr("慢充") }

    // 过滤后的列表
    readonly property var filtered: {
        var out = []
        for (var i = 0; i < allOrders.length; i++) {
            var o = allOrders[i]
            if (filterStatus === "all" || o.status === filterStatus)
                out.push(o)
        }
        return out
    }

    // 当前选中订单与其时间轴
    readonly property var order: selectedOrderId ? UserData.orderById(selectedOrderId) : ({})
    readonly property var timeline: selectedOrderId ? UserData.orderTimeline(selectedOrderId) : []

    // —— 历史平均（用于报告对比，由已完成订单聚合，不入库）——
    function historyAverage() {
        var n = 0, e = 0.0, d = 0, a = 0.0
        for (var i = 0; i < allOrders.length; i++) {
            var o = allOrders[i]
            if (o.status === "completed") {
                n++
                e += Number(o.energy_kwh || 0)
                d += Number(o.duration_min || 0)
                a += Number(o.pay_amount || 0)
            }
        }
        return {
            count: n,
            avg_energy: n ? e / n : 0.0,
            avg_duration: n ? d / n : 0,
            avg_amount: n ? a / n : 0.0
        }
    }
    function avgPower(order) {
        var d = Number(order.duration_min || 0)
        return d > 0 ? Number(order.energy_kwh || 0) / (d / 60.0) : 0.0
    }

    // 不透明背景
    Rectangle { anchors.fill: parent; color: Theme.background }

    // ============================ 列表模式 ============================
    Column {
        visible: selectedOrderId === 0
        anchors.fill: parent
        spacing: 0

        // 顶部返回 + 标题
        Rectangle {
            width: parent.width
            height: 72
            color: Theme.primary
            Row {
                anchors.left: parent.left; anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 8; anchors.rightMargin: 20
                spacing: 8
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "‹"; font.pixelSize: 28; color: "#ffffff"
                    MouseArea {
                        anchors.fill: parent; anchors.margins: -8
                        onClicked: root.stackView.pop()
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("我的订单")
                    color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true
                }
            }
        }

        // 状态筛选栏
        Row {
            width: parent.width
            height: 48
            property var tabs: [
                { key: "all",           label: qsTr("全部") },
                { key: "charging",      label: qsTr("进行中") },
                { key: "pending_settle",label: qsTr("待结算") },
                { key: "completed",     label: qsTr("已完成") },
                { key: "cancelled",     label: qsTr("已取消") }
            ]
            Repeater {
                model: parent.tabs
                delegate: Item {
                    width: parent.width / 5
                    height: parent.height
                    Column {
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.label
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: root.filterStatus === modelData.key
                            color: root.filterStatus === modelData.key ? Theme.primary : Theme.textSecondary
                        }
                        Rectangle {
                            width: 20; height: 3; radius: 1.5
                            color: root.filterStatus === modelData.key ? Theme.primary : "transparent"
                        }
                    }
                    MouseArea { anchors.fill: parent; onClicked: root.filterStatus = modelData.key }
                }
            }
        }

        // 底部列表
        ListView {
            id: listView
            width: parent.width
            height: parent.height - 72 - 48
            clip: true
            model: root.filtered
            spacing: 12
            topMargin: 12
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

                    // 站名 + 状态
                    Row {
                        width: parent.width
                        spacing: 8
                        Text {
                            width: parent.width - 80
                            text: modelData.station_name || ""
                            font.pixelSize: Theme.fontSizeBase
                            font.bold: true
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                        }
                        OrderBadge { text: root.statusText(modelData.status); c: root.statusColor(modelData.status) }
                    }

                    // 桩号 + 类型 + 时间
                    Text {
                        width: parent.width
                        text: (modelData.charger_code || "") + " · " + root.typeText(modelData.charger_type)
                              + (modelData.start_time ? (" · " + modelData.start_time) : "")
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                        elide: Text.ElideRight
                    }

                    // 充电量 / 金额
                    Row {
                        width: parent.width
                        spacing: 14
                        Text {
                            text: (Number(modelData.energy_kwh || 0).toFixed(1)) + " kWh"
                            color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                        }
                        Text {
                            visible: Number(modelData.pay_amount || 0) !== 0
                            text: "\u{FFE5}" + (Number(modelData.pay_amount || 0).toFixed(2))
                            color: Theme.danger; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                        }
                        Text {
                            visible: !!modelData.price_level
                            text: qsTr("档位 ") + root.priceLevelText(modelData.price_level)
                            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.selectedOrderId = modelData.id
                }
            }
        }
    }

    // ============================ 详情模式 ============================
    Rectangle {
        visible: selectedOrderId !== 0
        anchors.fill: parent
        color: Theme.background
        z: 10

        Column {
            width: parent.width
            height: 72
            color: Theme.primary
            Row {
                anchors.left: parent.left; anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 8; anchors.rightMargin: 20
                spacing: 8
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "‹"; font.pixelSize: 28; color: "#ffffff"
                    MouseArea {
                        anchors.fill: parent; anchors.margins: -8
                        onClicked: root.selectedOrderId = 0
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("订单详情")
                    color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true
                }
            }
        }

        ScrollView {
            anchors.top: parent.top
            anchors.topMargin: 72
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true

            Column {
                width: root.width - 32
                x: 16
                spacing: 14
                topPadding: 14
                bottomPadding: 20

                // 状态头卡
                Rectangle {
                    width: parent.width
                    height: 96
                    radius: Theme.radiusSmall
                    gradient: Gradient {
                        GradientStop { position: 0; color: Theme.primary }
                        GradientStop { position: 1; color: Theme.accent }
                    }
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 16; anchors.rightMargin: 16
                        spacing: 14
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.statusText(root.order.status)
                            color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true
                        }
                        Item { width: 1; height: 1 }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter; anchors.right: parent.right
                            spacing: 2
                            Text { horizontalAlignment: Text.AlignRight; text: root.order.station_name || ""; color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall; font.bold: true }
                            Text { horizontalAlignment: Text.AlignRight; text: (root.order.charger_code || "") + " · " + root.typeText(root.order.charger_type); color: "#E6F0FF"; font.pixelSize: Theme.fontSizeTiny }
                        }
                    }
                }

                // 充电概览
                SectionTitle { text: qsTr("充电概览") }
                Rectangle {
                    width: parent.width
                    height: overviewCol.implicitHeight + 20
                    color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
                    Column {
                        id: overviewCol
                        width: parent.width - 24
                        anchors.left: parent.left; anchors.leftMargin: 12
                        anchors.top: parent.top; anchors.topMargin: 10
                        spacing: 6
                        KV { k: qsTr("充电量"); v: (Number(root.order.energy_kwh || 0).toFixed(1)) + " kWh" }
                        KV { k: qsTr("充电时长"); v: (root.order.duration_min || 0) + qsTr(" 分钟") }
                        KV { k: qsTr("开始 / 目标 / 结束电量"); v: (root.order.start_soc || 0) + "% / " + (root.order.target_soc || 0) + "% / " + (root.order.end_soc || 0) + "%" }
                        KV { k: qsTr("开始时间"); v: root.order.start_time || "-" }
                        KV { k: qsTr("结束时间"); v: root.order.end_time || "-" }
                    }
                }

                // 费用明细
                SectionTitle { text: qsTr("费用明细") }
                Rectangle {
                    width: parent.width
                    height: feeCol.implicitHeight + 20
                    color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
                    Column {
                        id: feeCol
                        width: parent.width - 24
                        anchors.left: parent.left; anchors.leftMargin: 12
                        anchors.top: parent.top; anchors.topMargin: 10
                        spacing: 6
                        KV { k: qsTr("电价档位"); v: root.priceLevelText(root.order.price_level) + qsTr("时") }
                        KV { k: qsTr("应收"); v: "\u{FFE5}" + (Number(root.order.amount || 0).toFixed(2)) }
                        KV { k: qsTr("优惠抵扣"); v: "\u{FFE5}" + (Number(root.order.discount_amount || 0).toFixed(2)) }
                        KV { k: qsTr("积分抵扣"); v: (root.order.points_used || 0) + qsTr(" 积分") }
                        KV { k: qsTr("实付"); v: "\u{FFE5}" + (Number(root.order.pay_amount || 0).toFixed(2)); c: Theme.danger }
                        KV { k: qsTr("获得积分"); v: "+" + (root.order.points_earned || 0); c: Theme.success }
                    }
                }

                // 时间轴
                SectionTitle { text: qsTr("充电时间轴") }
                Rectangle {
                    width: parent.width
                    height: timelineCol.implicitHeight + 20
                    visible: root.timeline && root.timeline.length > 0
                    color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
                    Column {
                        id: timelineCol
                        width: parent.width - 24
                        anchors.left: parent.left; anchors.leftMargin: 12
                        anchors.top: parent.top; anchors.topMargin: 10
                        spacing: 0
                        Repeater {
                            model: root.timeline
                            delegate: Row {
                                width: parent.width
                                height: nodeCol.implicitHeight + 10
                                // 左侧圆点 + 竖线
                                Column {
                                    width: 16; height: parent.height
                                    Rectangle {
                                        x: 4; width: 8; height: 8; radius: 4
                                        anchors.top: parent.top; anchors.topMargin: 2
                                        color: Theme.primary
                                    }
                                    Rectangle {
                                        visible: index < root.timeline.length - 1
                                        x: 7; width: 2
                                        anchors.top: parent.top; anchors.topMargin: 12
                                        anchors.bottom: parent.bottom
                                        color: Theme.border
                                    }
                                }
                                Column {
                                    id: nodeCol
                                    width: parent.width - 16
                                    spacing: 2
                                    Text { text: modelData.label || ""; font.pixelSize: Theme.fontSizeSmall; font.bold: true; color: Theme.textPrimary }
                                    Text { text: modelData.event_time || ""; font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary }
                                    Text { visible: !!modelData.detail; text: modelData.detail || ""; font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary }
                                }
                            }
                        }
                    }
                }
                Text {
                    visible: !root.timeline || root.timeline.length === 0
                    text: qsTr("暂无时间轴节点")
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                }

                // 报告（仅已完成）
                SectionTitle { visible: root.order.status === "completed"; text: qsTr("智能充电报告") }
                Rectangle {
                    visible: root.order.status === "completed"
                    width: parent.width
                    height: reportCol.implicitHeight + 20
                    color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
                    Column {
                        id: reportCol
                        width: parent.width - 24
                        anchors.left: parent.left; anchors.leftMargin: 12
                        anchors.top: parent.top; anchors.topMargin: 10
                        spacing: 6

                        Text { text: qsTr("本次数据"); font.pixelSize: Theme.fontSizeSmall; font.bold: true; color: Theme.textPrimary }
                        KV { k: qsTr("平均功率"); v: root.avgPower(root.order).toFixed(1) + " kW" }
                        KV { k: qsTr("总费用"); v: "\u{FFE5}" + (Number(root.order.pay_amount || 0).toFixed(2)) }

                        Rectangle { width: parent.width; height: 1; color: Theme.border }

                        Text { text: qsTr("与历史平均对比"); font.pixelSize: Theme.fontSizeSmall; font.bold: true; color: Theme.textPrimary }
                        KV { k: qsTr("平均充电量"); v: (root.historyAverage().avg_energy).toFixed(1) + " kWh" }
                        KV { k: qsTr("平均时长"); v: (root.historyAverage().avg_duration).toFixed(0) + qsTr(" 分钟") }

                        Rectangle { width: parent.width; height: 1; color: Theme.border }

                        Text { text: qsTr("绿色贡献"); font.pixelSize: Theme.fontSizeSmall; font.bold: true; color: Theme.textPrimary }
                        KV { k: qsTr("本次充电量"); v: (Number(root.order.energy_kwh || 0).toFixed(1)) + " kWh" }
                        KV { k: qsTr("估算减碳"); v: (Number(root.order.energy_kwh || 0) * root.carbonFactor).toFixed(2) + " kg"; c: Theme.success }
                        Text {
                            width: parent.width
                            text: "\u{1F331} " + root.evaluation()
                            color: Theme.success; font.pixelSize: Theme.fontSizeTiny; wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }
    }

    // 报告一句话评价：按电费单价比对历史平均
    function evaluation() {
        var unit = Number(root.order.energy_kwh || 0) > 0
                 ? Number(root.order.amount || 0) / Number(root.order.energy_kwh) : 0.0
        var avg = root.historyAverage()
        var avgUnit = avg.avg_energy > 0 ? avg.avg_amount / avg.avg_energy : unit
        if (unit < avgUnit) return qsTr("本次单价低于历史平均，性价比不错")
        if (unit > avgUnit) return qsTr("本次单价略高于历史平均，可关注错峰充电")
        return qsTr("本次费用与历史平均水平相当")
    }

    // —— 复用小组件 ——
    component OrderBadge: Rectangle {
        property string text
        property color c: Theme.primary
        height: 22; width: badgeTxt.implicitWidth + 14; radius: 11
        color: c + "1A"; border.color: c; border.width: 1
        Text {
            id: badgeTxt
            anchors.centerIn: parent
            text: parent.text
            color: parent.c; font.pixelSize: Theme.fontSizeTiny; font.bold: true
        }
    }
    component SectionTitle: Text {
        font.pixelSize: Theme.fontSizeSmall + 1
        font.bold: true
        color: Theme.textPrimary
    }
    component KV: Row {
        property string k
        property string v
        property color c: Theme.textPrimary
        spacing: 6
        Text { text: parent.k + "："; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
        Text { text: parent.v; color: parent.c; font.pixelSize: Theme.fontSizeTiny; font.bold: (parent.c !== Theme.textPrimary) }
    }
}