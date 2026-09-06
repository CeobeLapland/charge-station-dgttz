import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UserClient

// 电站详情页：由首页/探索页进入。展示 station 满字段 + 分时电价 + 电桩 + 天气 + 评价。
// 数据全部来自 ExploreData 种子（对齐 DATA_STRUCTURE.md），不直连数据库。
Item {
    id: root
    property int stationId: 0
    readonly property var stackView: StackView.view

    readonly property var st: ExploreData.stationById(stationId) || ({})
    readonly property var chargers: ExploreData.chargersForStation(stationId)
    readonly property var priceRules: ExploreData.priceRulesForStation(stationId)
    property var reviews: []
    readonly property var weather: ExploreData.weatherForArea((st.weather_area || st.area) || "")

    // 评论刷新：发评论/点赞/回复后由 ExploreData.reviewsChanged 触发
    function reloadReviews() { reviews = ExploreData.reviewsForStation(stationId) }
    Connections {
        target: ExploreData
        function onReviewsChanged() { reloadReviews() }
    }
    readonly property var myNickname: UserData.profile().nickname || ""

    function statusLabel(s) {
        if (s === "idle")      return qsTr("空闲")
        if (s === "charging")  return qsTr("充电中")
        if (s === "reserved")  return qsTr("预约中")
        if (s === "fault")     return qsTr("故障")
        if (s === "offline")   return qsTr("离线")
        if (s === "rebooting") return qsTr("重启中")
        return s
    }
    function statusColor(s) {
        if (s === "idle")     return Theme.success
        if (s === "charging") return Theme.primary
        if (s === "reserved") return Theme.accent
        if (s === "fault" || s === "offline") return Theme.danger
        return Theme.warn
    }
    function weatherEmoji(c) {
        if (c === "sunny") return "☀️"
        if (c === "cloudy") return "⛅"
        if (c === "rain") return "🌧️"
        if (c === "hot") return "🔥"
        if (c === "extreme") return "🌪️"
        return "❔"
    }
    function facilityText(f) {
        if (f === "washroom") return qsTr("洗手间")
        if (f === "convenience_store") return qsTr("便利店")
        if (f === "rest_area") return qsTr("休息区")
        if (f === "wifi") return qsTr("WiFi")
        if (f === "rain_shelter") return qsTr("雨棚")
        if (f === "underground_parking") return qsTr("地下车库")
        return f
    }

    // 不透明背景
    Rectangle { anchors.fill: parent; color: Theme.background }

    // 顶部返回
    Rectangle {
        width: 40; height: 40
        anchors.top: parent.top; anchors.left: parent.left
        anchors.topMargin: 20; anchors.leftMargin: 16
        radius: 20; color: Theme.background; border.color: Theme.border
        Text { anchors.centerIn: parent; text: "‹"; font.pixelSize: 24; color: Theme.primary }
        MouseArea { anchors.fill: parent; onClicked: root.stackView.pop() }
    }

    // 标题
    Text {
        anchors.top: parent.top; anchors.topMargin: 28
        anchors.left: parent.left; anchors.leftMargin: 64
        anchors.right: parent.right; anchors.rightMargin: 64
        text: qsTr("电站详情")
        elide: Text.ElideRight
        font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.textPrimary
    }

    // 收藏按钮（右上角，点击收藏/取消）
    property bool isFavorite: false
    function refreshFav() { root.isFavorite = UserData.isFavorite(stationId) }
    Component.onCompleted: { refreshFav(); reloadReviews() }
    Connections { target: UserData; function onFavoritesChanged() { refreshFav() } }
    Rectangle {
        anchors.top: parent.top; anchors.topMargin: 18
        anchors.right: parent.right; anchors.rightMargin: 16
        width: 40; height: 40; radius: 20
        color: Theme.card; border.color: Theme.border; border.width: 1
        Text {
            anchors.centerIn: parent
            text: root.isFavorite ? "\u{2764}" : "\u{2661}"
            color: root.isFavorite ? "#F04438" : Theme.textSecondary
            font.pixelSize: 22
        }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                var on = UserData.toggleFavorite(stationId)
                showToast(on ? qsTr("已收藏该电站") : qsTr("已取消收藏"))
            }
        }
    }

    // 顶部渐变装饰
    Rectangle {
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 8
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.primary }
            GradientStop { position: 1; color: Theme.accent }
        }
    }

    // 内容滚动区
    ScrollView {
        id: sv
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: actionBar.top
        anchors.topMargin: 76
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: 8
        clip: true

        Column {
            width: sv.width
            spacing: 16

            // 站名 + 评分
            Column {
                width: parent.width; spacing: 6
                Row {
                    width: parent.width; spacing: 10
                    Text {
                        width: parent.width - 96
                        text: st.name || qsTr("未知电站")
                        font.pixelSize: Theme.fontSizeTitle + 2; font.bold: true
                        color: Theme.textPrimary; wrapMode: Text.Wrap
                    }
                    Row {
                        spacing: 2
                        anchors.verticalCenter: parent.verticalCenter
                        Text { text: "⭐"; font.pixelSize: 16; color: "#F59E0B" }
                        Text { text: Number(st.rating || 0).toFixed(1); font.bold: true; color: Theme.textPrimary }
                        Text { text: "(" + (st.rating_count || 0) + ")"; font.pixelSize: Theme.fontSizeSmall; color: Theme.textSecondary }
                    }
                }
                Text {
                    width: parent.width
                    text: "\u{1F4CD} " + (st.address || "")
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall; wrapMode: Text.Wrap
                }
            }

            // 标签
            Flow {
                width: parent.width; spacing: 8
                Chip { text: st.owner_label || ""; c: Theme.primary }
                Chip { text: qsTr("空闲 ") + (st.fast_idle || 0) + "快/" + (st.slow_idle || 0) + "慢"
                       c: ((Number(st.fast_idle) + Number(st.slow_idle)) > 0) ? Theme.success : Theme.warn }
                Chip { text: qsTr("支持换电"); c: Theme.success; visible: !!st.has_swap }
                Chip { text: qsTr("营业 ") + (st.business_hours || ""); c: Theme.textPrimary }
                Chip { text: (Number(st.parking_fee) > 0)
                             ? (qsTr("停车 ") + st.parking_fee + " 元/h") : qsTr("停车免费"); c: Theme.textPrimary }
            }

            // 天气
            SectionTitle { text: qsTr("天气 & 时间") }
            Row {
                width: parent.width; spacing: 20
                Item { width: 1; height: 1 }
                Column { spacing: 2
                    Text { text: qsTr("天气"); font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary }
                    Row { spacing: 4
                        Text { text: weatherEmoji(weather.condition); font.pixelSize: 18 }
                        Text { text: statusChText(weather.condition); font.pixelSize: Theme.fontSizeSmall; font.bold: true; color: Theme.textPrimary }
                    }
                }
                Column { spacing: 2
                    Text { text: qsTr("温度"); font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary }
                    Text { text: (Number(weather.temperature) || 0).toFixed(1) + " ℃"; font.pixelSize: Theme.fontSizeSmall; color: Theme.textPrimary }
                }
                Column { spacing: 2
                    Text { text: qsTr("当前时间"); font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary }
                    Text { text: Qt.formatDateTime(new Date(), "HH:mm"); font.pixelSize: Theme.fontSizeSmall; color: Theme.textPrimary }
                }
            }

            // 分时电价
            SectionTitle { text: qsTr("分时电价"); sub: qsTr("价格已含服务费 ") + (st.service_fee || 0) + " 元/kWh" }
            Column {
                width: parent.width; spacing: 6
                Repeater {
                    model: priceRules
                    delegate: Row {
                        width: parent.width
                        Text {
                            width: 56; text: levelText(modelData.level); font.bold: true
                            color: (modelData.level === "valley") ? Theme.success
                                 : (modelData.level === "peak") ? Theme.danger : Theme.primary
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        Text {
                            width: parent.width - 56 - 90
                            text: modelData.time_range || ""; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall
                        }
                        Text {
                            text: (Number(modelData.price || 0) + Number(st.service_fee || 0)).toFixed(2) + " 元/kWh"
                            font.bold: true; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }
            }

            // 电桩
            SectionTitle { text: qsTr("电桩（共 ") + (st.total_chargers || 0) + qsTr(" 个）") }
            Column {
                width: parent.width; spacing: 8
                Repeater {
                    model: chargers
                    delegate: Rectangle {
                        width: parent.width
                        height: chargerCol.implicitHeight + 20
                        color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
                        Column {
                            id: chargerCol
                            width: parent.width - 24
                            anchors.left: parent.left; anchors.leftMargin: 12
                            anchors.top: parent.top; anchors.topMargin: 10
                            spacing: 6
                            Row {
                                width: parent.width
                                Text { text: modelData.code || ""; font.bold: true; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeBase }
                                Item { width: 8; height: 1 }
                                Chip { text: statusLabel(modelData.status); c: statusColor(modelData.status) }
                                Item { width: 4; height: 1 }
                                Chip { text: (modelData.type === "fast" ? qsTr("快充 ") : qsTr("慢充 ")) + (modelData.power || 0) + "kW"
                                       c: (modelData.type === "fast" ? Theme.primary : Theme.textPrimary) }
                            }
                            Row {
                                width: parent.width; spacing: 14
                                KV { k: qsTr("功率"); v: (modelData.power || 0) + " kW" }
                                KV { k: qsTr("电压"); v: (modelData.voltage || 0) + " V" }
                                KV { k: qsTr("电流"); v: (modelData.current || 0) + " A" }
                                KV { k: qsTr("温度"); v: (modelData.temperature || 0) + " ℃" }
                            }
                            Row {
                                width: parent.width; spacing: 14
                                KV { k: qsTr("健康"); v: (modelData.health_score || 0) + "/100"
                                     c: (Number(modelData.health_score) >= 90) ? Theme.success
                                      : (Number(modelData.health_score) >= 75) ? Theme.textPrimary : Theme.warn }
                                KV { k: qsTr("通信"); v: (modelData.comm_status === "normal" ? qsTr("正常") : qsTr("异常"))
                                     c: (modelData.comm_status === "normal") ? Theme.success : Theme.danger }
                                KV { k: qsTr("累计"); v: (modelData.total_charge_count || 0) + qsTr("次") }
                            }
                            KV { visible: !!modelData.fault_code; k: qsTr("故障码"); v: modelData.fault_code || ""; c: Theme.danger }
                        }
                    }
                }
            }

            // 站点详情 + 设施
            SectionTitle { text: qsTr("站点详情") }
            Column {
                width: parent.width; spacing: 6
                KV { k: qsTr("区域"); v: st.area || "-" }
                KV { k: qsTr("在线率"); v: (Number(st.online_rate || 0) * 100).toFixed(0) + " %" }
                KV { k: qsTr("服务费"); v: (st.service_fee || 0) + " 元/kWh" }
                KV { k: qsTr("停车费"); v: (Number(st.parking_fee) > 0) ? (st.parking_fee + " 元/h") : qsTr("免费") }
                KV { k: qsTr("设施"); v: {
                        var arr = st.facilities || []
                        if (arr.length === 0) return qsTr("无")
                        return arr.map(facilityText).join("、")
                    } }
            }

            // 评价
            SectionTitle { text: qsTr("用户评价") }
            Row {
                width: parent.width
                Rectangle {
                    width: 84; height: 30; radius: 15
                    color: Theme.primary
                    Text {
                        anchors.centerIn: parent; text: qsTr("写评价");
                        color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                    }
                    MouseArea { anchors.fill: parent; onClicked: reviewDialog.open() }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("共 ") + (reviews.length||0) + qsTr(" 条")
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                }
            }
            Column {
                width: parent.width; spacing: 10
                Repeater {
                    model: reviews
                    delegate: Rectangle {
                        width: parent.width
                        height: reviewCol.implicitHeight + 20
                        color: Theme.card; radius: Theme.radiusSmall; border.color: Theme.border
                        function submitReply() {
                            var t = replyInput.text
                            if (!t || !t.trim()) return
                            ExploreData.addReply(modelData.id, root.myNickname, t.trim())
                            replyInput.text = ""
                            replyBox.visible = false
                        }
                        Column {
                            id: reviewCol
                            width: parent.width - 24
                            anchors.left: parent.left; anchors.leftMargin: 12
                            anchors.top: parent.top; anchors.topMargin: 10
                            spacing: 6
                            RowLayout {
                                width: parent.width
                                Text { text: modelData.nickname || ""; font.bold: true; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall }
                                Item { width: 8; height: 1 }
                                Text { text: "⭐ " + Number(modelData.overall_score || 0).toFixed(1); color: "#F59E0B"; font.bold: true; font.pixelSize: Theme.fontSizeSmall }
                                Item { Layout.fillWidth: true; height: 1 }
                                Text { visible: !!modelData.is_mine; text: qsTr("我"); color: Theme.primary; font.pixelSize: Theme.fontSizeTiny; font.bold: true }
                            }
                            Text { width: parent.width; text: modelData.content || ""; wrapMode: Text.Wrap; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall }
                            RowLayout {
                                width: parent.width; spacing: 8
                                Repeater {
                                    model: modelData.tags || []
                                    delegate: Text {
                                        text: "#" + modelData; color: Theme.accent; font.pixelSize: Theme.fontSizeTiny
                                    }
                                }
                                Item { Layout.fillWidth: true; height: 1 }
                                // 点赞「有用」
                                Rectangle {
                                    Layout.alignment: Qt.AlignVCenter
                                    width: useTxt.implicitWidth + 14; height: 22; radius: 11
                                    color: modelData.liked_by_me ? "#F59E0B22" : "transparent"
                                    border.color: modelData.liked_by_me ? "#F59E0B" : Theme.border; border.width: 1
                                    Text {
                                        id: useTxt; anchors.centerIn: parent
                                        text: "\u{1F44D} 有用 " + (modelData.useful_count||0)
                                        color: modelData.liked_by_me ? "#F59E0B" : Theme.textSecondary
                                        font.pixelSize: 11; font.bold: modelData.liked_by_me
                                    }
                                    MouseArea { anchors.fill: parent; onClicked: ExploreData.toggleUseful(modelData.id) }
                                }
                                // 回复
                                Rectangle {
                                    Layout.alignment: Qt.AlignVCenter
                                    width: repTxt.implicitWidth + 14; height: 22; radius: 11
                                    color: "transparent"; border.color: Theme.border; border.width: 1
                                    Text { id: repTxt; anchors.centerIn: parent; text: qsTr("回复"); color: Theme.textSecondary; font.pixelSize: 11 }
                                    MouseArea { anchors.fill: parent; onClicked: replyBox.visible = !replyBox.visible }
                                }
                                Text { Layout.alignment: Qt.AlignVCenter; text: modelData.create_time || ""; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
                            }
                            // 回复列表
                            Column {
                                width: parent.width; spacing: 4
                                Repeater {
                                    model: modelData.replies || []
                                    delegate: Column {
                                        width: parent.width
                                        Text {
                                            width: parent.width; wrapMode: Text.Wrap
                                            text: modelData.author + qsTr("：") + modelData.content
                                            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                                        }
                                        Text { visible: !!modelData.create_time; text: modelData.create_time; color: Theme.textSecondary; font.pixelSize: 9 }
                                    }
                                }
                            }
                            // 回复输入框（点击「回复」展开）
                            Rectangle {
                                id: replyBox
                                visible: false
                                width: parent.width; height: 32
                                color: Theme.background; radius: 6; border.color: Theme.border
                                Row {
                                    anchors.fill: parent; anchors.margins: 4; spacing: 4
                                    TextInput {
                                        id: replyInput
                                        width: parent.width - 66
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: Theme.textPrimary; font.pixelSize: Theme.fontSizeTiny
                                        clip: true
                                        onEditingFinished: submitReply()
                                        Keys.onReturnPressed: submitReply()
                                    }
                                    Rectangle {
                                        width: 60; height: 24; radius: 5; color: Theme.primary
                                        anchors.verticalCenter: parent.verticalCenter
                                        Text { anchors.centerIn: parent; text: qsTr("发送"); color: "#fff"; font.pixelSize: 11 }
                                        MouseArea { anchors.fill: parent; onClicked: submitReply() }
                                    }
                                }
                            }
                        }
                    }
                }
                Text {
                    visible: !reviews || reviews.length === 0
                    text: qsTr("暂无评价"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                }
            }
        }
    }

    // 底部操作条
    Rectangle {
        id: actionBar
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 76
        color: Theme.card; border.color: Theme.border

        Row {
            anchors.centerIn: parent
            spacing: 16
            // 导航
            Rectangle {
                width: 140; height: 48; radius: Theme.radiusSmall
                color: Theme.background; border.color: Theme.primary; border.width: 1
                Text { anchors.centerIn: parent; text: qsTr("一键导航"); color: Theme.primary; font.bold: true; font.pixelSize: Theme.fontSizeBase }
                MouseArea { anchors.fill: parent; onClicked: showToast(qsTr("导航（示例）：腾讯地图路线规划待接入")) }
            }
            // 立即预约（充电全流程统一入口）
            Rectangle {
                width: 170; height: 48; radius: Theme.radiusSmall; color: Theme.primary
                Text { anchors.centerIn: parent; text: qsTr("立即预约"); color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeBase }
                MouseArea { anchors.fill: parent; onClicked: startReserve() }
            }
        }
    }

    // 轻提示
    Rectangle {
        id: toast
        visible: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 96
        width: Math.min(toastText.implicitWidth + 32, root.width - 48)
        height: 40; radius: 20
        color: "#B3000000"
        Text {
            id: toastText
            anchors.centerIn: parent
            text: ""
            color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall
        }
    }
    Timer { id: toastTimer; interval: 2000; onTriggered: toast.visible = false }
    function showToast(msg) { toastText.text = msg; toast.visible = true; toastTimer.restart() }

    // 发起预约：跳转预约表单（充电全流程统一入口，有空位/排队由服务端决策）
    function startReserve() {
        stackView.push("qrc:/UserClient/qml/pages/ReservePage.qml",
                       { stationId: root.stationId })
    }

    function levelText(lv) {
        if (lv === "valley") return qsTr("谷")
        if (lv === "flat") return qsTr("平")
        if (lv === "peak") return qsTr("峰")
        return lv
    }
    function statusChText(c) {
        if (c === "sunny") return qsTr("晴")
        if (c === "cloudy") return qsTr("多云")
        if (c === "rain") return qsTr("雨")
        if (c === "hot") return qsTr("高温")
        if (c === "extreme") return qsTr("极端")
        return c
    }

    // —— 写评价弹窗 ——
    Popup {
        id: reviewDialog
        parent: Overlay.overlay
        width: Math.min(root.width - 48, 400)
        height: Math.min(root.height - 160, 560)
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        modal: true; focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 0

        property var tagPool: ["充电快","车位多","位置方便","免费停车","设备老旧","排队严重","适合快充","晚上人少","服务好","价格实惠"]
        property var choseTags: []

        function toggleTag(t) {
            var idx = choseTags.indexOf(t)
            if (idx >= 0) choseTags.splice(idx, 1)
            else choseTags.push(t)
            choseTags = choseTags.slice()   // 触发依赖重算
        }

        contentItem: Rectangle {
            id: reviewForm
            color: Theme.card; radius: Theme.radius
            clip: true
            function submitReview() {
                if (srOverall.value < 0.5) { root.showToast(qsTr("请先选择综合评分")); return }
                var ok = ExploreData.addReview({
                            station_id: root.stationId,
                            nickname: root.myNickname || qsTr("我"),
                            overall_score: srOverall.value,
                            speed_score: srSpeed.value,
                            device_score: srDevice.value,
                            parking_score: srParking.value,
                            hygiene_score: srHygiene.value,
                            service_score: srService.value,
                            tags: reviewDialog.choseTags.slice(), content: contentText.text
                        })
                reviewDialog.close()
                root.showToast(ok ? qsTr("评价已发布") : qsTr("发布失败"))
            }
            function clearForm() {
                srOverall.value = 0; srSpeed.value = 0; srDevice.value = 0
                srParking.value = 0; srHygiene.value = 0; srService.value = 0
                reviewDialog.choseTags = []; if (contentText) contentText.text = ""
            }
            Connections {
                target: reviewDialog
                function onOpened() { reviewForm.clearForm() }
            }
            // 标题栏
            Rectangle {
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                height: 52; color: Theme.background
                Text {
                    anchors.centerIn: parent
                    text: qsTr("写评价"); font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.textPrimary
                }
            }
            // 表单滚动区
            ScrollView {
                id: reviewScroll
                anchors.left: parent.left; anchors.right: parent.right
                anchors.top: parent.top; anchors.topMargin: 52
                anchors.bottom: parent.bottom; anchors.bottomMargin: 56
                clip: true
                Column {
                    width: reviewScroll.width
                    padding: 16; spacing: 10
                    // 六个维度评分（每个 Row 单独闭合，避免嵌套触发布局 polish() 循环）
                    Row { spacing: 16
                        Text { text: qsTr("综合"); width: 40; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall }
                        StarRating { id: srOverall }
                    }
                    Row { spacing: 16
                        Text { text: qsTr("速度"); width: 40; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall }
                        StarRating { id: srSpeed }
                    }
                    Row { spacing: 16
                        Text { text: qsTr("设备"); width: 40; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall }
                        StarRating { id: srDevice }
                    }
                    Row { spacing: 16
                        Text { text: qsTr("停车"); width: 40; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall }
                        StarRating { id: srParking }
                    }
                    Row { spacing: 16
                        Text { text: qsTr("卫生"); width: 40; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall }
                        StarRating { id: srHygiene }
                    }
                    Row { spacing: 16
                        Text { text: qsTr("服务"); width: 40; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall }
                        StarRating { id: srService }
                    }
                    // 标签
                    Text { text: qsTr("标签（可多选）"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
                    Flow {
                        width: parent.width; spacing: 8
                        Repeater {
                            model: reviewDialog.tagPool
                            delegate: Rectangle {
                                property bool on: reviewDialog.choseTags.indexOf(modelData) >= 0
                                width: tagTxt.implicitWidth + 18; height: 26; radius: 13
                                color: on ? Theme.primary + "22" : Theme.card
                                border.color: on ? Theme.primary : Theme.border; border.width: 1
                                Text {
                                    id: tagTxt; anchors.centerIn: parent
                                    text: modelData; color: on ? Theme.primary : Theme.textSecondary
                                    font.pixelSize: Theme.fontSizeTiny
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: reviewDialog.toggleTag(modelData)
                                }
                            }
                        }
                    }
                    // 内容
                    Text { text: qsTr("文字评价"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
                    Rectangle {
                        width: parent.width; height: 96
                        color: Theme.background; radius: Theme.radiusSmall; border.color: Theme.border
                        TextArea {
                            id: contentText
                            width: parent.width; height: parent.height; padding: 10
                            placeholderText: qsTr("说说你的充电体验……")
                            color: Theme.textPrimary; wrapMode: TextEdit.Wrap
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }
            }
            // 底部按钮行
            Row {
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                height: 56
                Rectangle {
                    width: parent.width/2; height: parent.height
                    color: "transparent"
                    Text { anchors.centerIn: parent; text: qsTr("取消"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeBase }
                    MouseArea { anchors.fill: parent; onClicked: reviewDialog.close() }
                }
                Rectangle {
                    width: parent.width/2; height: parent.height
                    color: Theme.primary
                    Text { anchors.centerIn: parent; text: qsTr("发布"); color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeBase }
                    MouseArea { anchors.fill: parent; onClicked: reviewForm.submitReview() }
                }
            }
        }
    }

    // —— 复用小组件 ——
    component Chip: Rectangle {
        property string text
        property color c: Theme.primary
        height: 24; width: chipTxt.implicitWidth + 16; radius: 12
        color: c + "1A"; border.color: c; border.width: 1
        Text { id: chipTxt; anchors.centerIn: parent; text: parent.text; color: parent.c; font.pixelSize: Theme.fontSizeTiny; font.bold: true }
    }
    component SectionTitle: Column {
        property string text
        property string sub
        spacing: 2
        Text { text: parent.text; font.pixelSize: Theme.fontSizeSmall + 1; font.bold: true; color: Theme.textPrimary }
        Text { visible: !!sub; text: sub; font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary }
    }
    component KV: Row {
        property string k
        property string v
        property color c: Theme.textPrimary
        spacing: 6
        Text { text: parent.k + "："; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny }
        Text { text: parent.v; color: parent.c; font.pixelSize: Theme.fontSizeTiny; font.bold: (parent.c !== Theme.textPrimary) }
    }
    component StarRating: Row {
        id: sr
        property real value: 0
        spacing: 2
        // 内联组件内禁用 Repeater/delegate（运行时解析报错），故显式铺 5 颗星
        Text { font.pixelSize: 24; text: sr.value >= 1 ? "★" : "☆"; color: sr.value >= 1 ? "#F59E0B" : Theme.border; MouseArea { anchors.fill: parent; onClicked: sr.value = 1 } }
        Text { font.pixelSize: 24; text: sr.value >= 2 ? "★" : "☆"; color: sr.value >= 2 ? "#F59E0B" : Theme.border; MouseArea { anchors.fill: parent; onClicked: sr.value = 2 } }
        Text { font.pixelSize: 24; text: sr.value >= 3 ? "★" : "☆"; color: sr.value >= 3 ? "#F59E0B" : Theme.border; MouseArea { anchors.fill: parent; onClicked: sr.value = 3 } }
        Text { font.pixelSize: 24; text: sr.value >= 4 ? "★" : "☆"; color: sr.value >= 4 ? "#F59E0B" : Theme.border; MouseArea { anchors.fill: parent; onClicked: sr.value = 4 } }
        Text { font.pixelSize: 24; text: sr.value >= 5 ? "★" : "☆"; color: sr.value >= 5 ? "#F59E0B" : Theme.border; MouseArea { anchors.fill: parent; onClicked: sr.value = 5 } }
    }
}