import QtQuick
import QtQuick.Controls
import UserClient

// 「充电画像/年度报告」：分页幻灯片式展示。7 页，底部 上一页/下一页，最后一页「退出」。
// 数据来自 UserData.portrait() + orders() 聚合 + profile()，趣味文案 + emoji。
Item {
    id: root
    readonly property var stackView: StackView.view
    property int currentIndex: 0

    // —— 聚合数据（onCompleted 一次性计算）——
    property double totalEnergy: 0
    property double totalSpend: 0
    property double maxSingle: 0
    property int completedCount: 0
    property int uniqueStations: 0
    property double avgEnergy: 0
    property double avgSpend: 0
    property double carbonKg: 0
    property int trees: 0
    property string monthAvg: "0.0"
    property string usualHours: "18:00–21:00"
    property string favStation: "-"
    property string preferType: "快充"
    property int totalPoints: 0

    function compute() {
        var orders = UserData.orders()
        var seens = {}
        var ec = 0
        var spend = 0
        var maxE = 0
        for (var i = 0; i < orders.length; i++) {
            var st = orders[i].status
            if (st === "completed" || st === "pending_settle") {
                ec++
                spend += Number(orders[i].pay_amount || 0)
                maxE = Math.max(maxE, Number(orders[i].energy_kwh || 0))
            }
            if (st === "completed") {
                var sid = Number(orders[i].station_id)
                if (!seens[sid]) seens[sid] = 1
            }
        }
        var energy = UserData.totalEnergyKwh()
        var p = UserData.portrait() || {}
        var prof = UserData.profile() || {}

        root.completedCount = ec
        root.totalEnergy = energy
        root.totalSpend = spend
        root.maxSingle = maxE
        root.uniqueStations = Object.keys(seens).length
        root.avgEnergy = ec > 0 ? energy / ec : 0
        root.avgSpend = ec > 0 ? spend / ec : 0
        root.carbonKg = energy * 0.7
        root.trees = Math.round(energy)   // 趣味：用电 kWh ≈ 植树数占位
        root.monthAvg = Number(p.month_avg_count || 0).toFixed(1)
        root.usualHours = p.usual_hours || "18:00–21:00"
        root.favStation = p.favorite_station || "-"
        root.preferType = p.prefer_type || "快充"
        root.totalPoints = Number(prof.points || 0)
    }
    Component.onCompleted: compute()

    // —— 页面元数据 ——
    readonly property var pages: [
        { emoji: "\u{1F389}", title: qsTr("你的充电年度报告"), lines: [
            { k: qsTr("完成充电"), v: String(root.completedCount) + qsTr(" 次") },
            { k: qsTr("累计用电"), v: Number(root.totalEnergy).toFixed(1) + " kWh" },
            { k: qsTr("累计支出"), v: "\u{FFE5} " + Number(root.totalSpend).toFixed(2) }
          ], note: qsTr("这一年，每一度电都为你的路充电 \u{1F680}") },
        { emoji: "\u{1F50B}", title: qsTr("电力篇"), lines: [
            { k: qsTr("累计用电"), v: Number(root.totalEnergy).toFixed(1) + " kWh" },
            { k: qsTr("平均每次"), v: Number(root.avgEnergy).toFixed(1) + " kWh" },
            { k: qsTr("单次最高"), v: Number(root.maxSingle).toFixed(1) + " kWh" },
            { k: qsTr("低碳贡献"), v: "约 " + Number(root.carbonKg).toFixed(0) + " kg CO₂" }
          ], note: qsTr("相当于少烧了约 " + Math.round(root.trees) + " 升汽油 \u{1F331}") },
        { emoji: "\u{1F4B0}", title: qsTr("消费篇"), lines: [
            { k: qsTr("累计支出"), v: "\u{FFE5} " + Number(root.totalSpend).toFixed(2) },
            { k: qsTr("平均每次"), v: "\u{FFE5} " + Number(root.avgSpend).toFixed(2) },
            { k: qsTr("当前积点"), v: String(root.totalPoints) + " 分" }
          ], note: qsTr("每次" + root.monthAvg + " 次充电，钱包表示很稳 \u{1F4B2}") },
        { emoji: "\u{1F552}", title: qsTr("时间篇"), lines: [
            { k: qsTr("常用时段"), v: String(root.usualHours) },
            { k: qsTr("月均次数"), v: String(root.monthAvg) + qsTr(" 次/月") }
          ], note: qsTr("常在下班后用电压力小，你是会省钱的 \u{1F60E}") },
        { emoji: "\u{1F4CD}", title: qsTr("电站篇"), lines: [
            { k: qsTr("常去电站"), v: String(root.favStation) },
            { k: qsTr("去过电站"), v: String(root.uniqueStations) + qsTr(" 座") }
          ], note: qsTr("老地方常去，轻车熟路不迷路 \u{1F3C1}") },
        { emoji: "\u{1F697}", title: qsTr("偏好篇"), lines: [
            { k: qsTr("充电偏好"), v: String(root.preferType) }
          ], note: qsTr("比谁都更懂" + root.preferType + "的脾气 \u{2601}\u{FE0F}") },
        { emoji: "\u{1F3C6}", title: qsTr("报告完成"), lines: [
            { k: qsTr("充电达人成就"), v: "\u{2714}" }
          ], note: qsTr("以上就是你的充电故事，继续低碳出行吧 \u{1F49A}") }
    ]
    readonly property int pageCount: root.pages.length

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
                MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: root.stackView.pop() }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("充电画像"); font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.textPrimary
            }
        }
    }

    // 内容卡片（随页切换）
    Rectangle {
        anchors.top: parent.top; anchors.topMargin: 100
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: bottomBar.top
        anchors.leftMargin: 24; anchors.rightMargin: 24
        anchors.bottomMargin: 16
        radius: Theme.radius
        color: Theme.card
        border.color: Theme.border

        Column {
            id: cardCol
            anchors.fill: parent
            anchors.topMargin: 24
            anchors.bottomMargin: 24
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 0

            // 大 emoji
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: root.pages[root.currentIndex].emoji
                font.pixelSize: 72
            }
            // 页标题
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: root.pages[root.currentIndex].title
                color: Theme.primary
                font.pixelSize: Theme.fontSizeTitle; font.bold: true
                topPadding: 8
                bottomPadding: 20
            }
            // 数据行
            Column {
                width: parent.width
                spacing: 14
                Repeater {
                    model: root.pages[root.currentIndex].lines
                    delegate: Row {
                        width: parent.width
                        spacing: 12
                        Text {
                            width: 92; text: modelData.k + "："
                            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                        }
                        Text {
                            width: parent.width - 92
                            text: modelData.v
                            color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }
        // 趣味文案（卡片底部，作为卡片子项用 anchors 固定，不放在 Column 里）
        Text {
            id: noteTxt
            anchors.left: parent.left; anchors.leftMargin: 16
            anchors.right: parent.right; anchors.rightMargin: 16
            anchors.bottom: parent.bottom; anchors.bottomMargin: 20
            horizontalAlignment: Text.AlignHCenter
            text: root.pages[root.currentIndex].note
            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.Wrap
            lineHeight: 1.5
        }
    }

    // 底部翻页栏
    Rectangle {
        id: bottomBar
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 96
        color: Theme.card; border.color: Theme.border
        // 上一页（左）
        Rectangle {
            id: prevBtn
            anchors.left: parent.left; anchors.leftMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            width: 120; height: 44; radius: Theme.radiusSmall
            color: root.currentIndex > 0 ? Theme.background : Theme.border
            border.color: root.currentIndex > 0 ? Theme.border : "transparent"
            Text {
                anchors.centerIn: parent
                text: qsTr("上一页"); color: root.currentIndex > 0 ? Theme.textPrimary : Theme.textSecondary
                font.bold: true; font.pixelSize: Theme.fontSizeBase
            }
            MouseArea {
                anchors.fill: parent
                enabled: root.currentIndex > 0
                onClicked: root.currentIndex--
            }
        }
        // 下一页/退出（右）
        Rectangle {
            id: nextBtn
            anchors.right: parent.right; anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            width: 120; height: 44; radius: Theme.radiusSmall
            color: (root.currentIndex >= root.pageCount - 1) ? Theme.danger : Theme.primary
            Text {
                anchors.centerIn: parent
                text: (root.currentIndex >= root.pageCount - 1) ? qsTr("退出报告") : qsTr("下一页")
                color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeBase
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (root.currentIndex >= root.pageCount - 1)
                        root.stackView.pop()
                    else
                        root.currentIndex++
                }
            }
        }
        // 页码点（中间）
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            Repeater {
                model: root.pageCount
                delegate: Rectangle {
                    width: 8; height: 8; radius: 4
                    color: (index === root.currentIndex) ? Theme.primary : Theme.border
                }
            }
        }
    }
}