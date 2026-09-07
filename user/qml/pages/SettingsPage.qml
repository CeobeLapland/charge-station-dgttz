import QtQuick
import QtQuick.Controls
import UserClient

// 设置页：主题风格 + 地图瓦片源（用户可配置，QSettings 持久化）+ 退出账号
Item {
    id: root
    readonly property var stackView: StackView.view

    readonly property var presets: JSON.parse(authStore.mapTilePresetsJson())

    // —— 当前选中 preset 的 index（初始值依据 authStore.mapTileSource 在 onCompleted 里同步）——
    property int presetIndex: 0

    // 默认充电模式选项
    readonly property var chargeModes: [
        { id: "fast", name: qsTr("快充优先") },
        { id: "standard", name: qsTr("标准速度") },
        { id: "full", name: qsTr("稳定慢充") }
    ]

    function chargeModeNames() {
        var arr = []
        for (var i = 0; i < root.chargeModes.length; i++)
            arr.push(root.chargeModes[i].name)
        return arr
    }
    function chargeModeIndex() {
        for (var i = 0; i < root.chargeModes.length; i++)
            if (root.chargeModes[i].id === authStore.chargeMode) return i
        return 0
    }
    function chargeModeId(idx) { return root.chargeModes[idx].id }

    Component.onCompleted: syncPresetIndex()
    Connections {
        target: authStore
        function onMapTileSourceChanged() { syncPresetIndex() }
        function onMapTileCustomUrlChanged() { syncPresetIndex() }
    }
    function syncPresetIndex() {
        var cur = authStore.mapTileSource
        for (var i = 0; i < presets.length; i++)
            if (presets[i].id === cur) { presetIndex = i; return }
        presetIndex = 0
    }

    // 不透明背景
    Rectangle { anchors.fill: parent; color: Theme.background }

    // 顶部返回
    Rectangle {
        width: 40; height: 40
        anchors.top: parent.top; anchors.left: parent.left
        anchors.topMargin: 20; anchors.leftMargin: 16
        radius: 20; color: Theme.background
        border.color: Theme.border; border.width: 1
        Text { anchors.centerIn: parent; text: "‹"; font.pixelSize: 24; color: Theme.primary }
        MouseArea { anchors.fill: parent; onClicked: root.stackView.pop() }
    }

    // 标题
    Text {
        anchors.top: parent.top; anchors.topMargin: 32
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("设置")
        font.pixelSize: Theme.fontSizeTitle
        font.bold: true
        color: Theme.textPrimary
    }

    ScrollView {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: logoutBtn.top
        anchors.topMargin: 88
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: 16
        clip: true

        Column {
            width: parent.width
            spacing: 24

            // —— 主题风格选择（沿用已有 techBlue / minimalDark / springGreen，供顺手操作）——
            SectionCard { title: qsTr("主题风格") }
            ComboBox {
                id: themeCombo
                width: parent.width
                model: ["techBlue", "minimalDark", "springGreen", "royalPurple", "sunsetOrange", "oceanTeal", "midnightBlue"]
                currentIndex: Math.max(0, model.indexOf(Theme.styleName))
                background: Rectangle { color: Theme.card; border.color: Theme.border; border.width: 1; radius: Theme.radiusSmall }
                contentItem: Text {
                    text: themeCombo.displayText
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeBase
                    leftPadding: 12; verticalAlignment: Text.AlignVCenter
                }
                onActivated: function (idx) { Theme.styleName = themeCombo.model[idx] }
            }

            // —— 地图瓦片源选择 ——
            SectionCard { title: qsTr("探索页：地图瓦片源") }
            ComboBox {
                id: tileSourceCombo
                width: parent.width
                property var presetsModel: root.presets
                model: presetsModel.map(function(p){ return p.name })
                currentIndex: root.presetIndex
                background: Rectangle { color: Theme.card; border.color: Theme.border; border.width: 1; radius: Theme.radiusSmall }
                contentItem: Text {
                    text: tileSourceCombo.displayText
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeBase
                    leftPadding: 12; verticalAlignment: Text.AlignVCenter
                }
                onActivated: function (idx) {
                    var presetId = root.presets[idx].id
                    authStore.mapTileSource = presetId
                }
            }

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeTiny
                color: Theme.textSecondary
                text: qsTr("可切换不同厂商的瓦片源；若所选源网络不可达会显示红条提示，请换一个。")
            }

            // 自定义 URL 输入（仅选 custom 时启用）
            Column {
                width: parent.width
                enabled: authStore.mapTileSource === "custom"
                opacity: enabled ? 1.0 : 0.45
                spacing: 8

                Text {
                    text: qsTr("自定义 URL 模板（用 {z} {x} {y} 占位）")
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textSecondary
                }

                Rectangle {
                    width: parent.width; height: 44
                    color: Theme.card; border.color: Theme.border; border.width: 1
                    radius: Theme.radiusSmall
                    TextField {
                        id: customUrl
                        anchors.fill: parent
                        anchors.leftMargin: 12; anchors.rightMargin: 12
                        text: authStore.mapTileCustomUrl
                        placeholderText: qsTr("例如 https://tile.example.org/{z}/{x}/{y}.png")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeBase
                        selectByMouse: true
                        maximumLength: 512
                        background: Rectangle { color: "transparent" }
                        onTextEdited: authStore.mapTileCustomUrl = text
                    }
                }
            }

            // —— 通知设置 ——
            SectionCard { title: qsTr("通知设置") }
            ToggleRow { title: qsTr("充电完成提醒"); sub: qsTr("充电结束后推送通知"); on: authStore.notifyChargeDone; onChanged: (o) => authStore.notifyChargeDone = o }
            ToggleRow { title: qsTr("订单进度通知"); sub: qsTr("预约、充电、结算等动态提醒"); on: authStore.notifyOrder; onChanged: (o) => authStore.notifyOrder = o }
            ToggleRow { title: qsTr("优惠活动推送"); sub: qsTr("接收优惠券与充值活动信息"); on: authStore.notifyPromo; onChanged: (o) => authStore.notifyPromo = o }

            // —— 充电偏好 ——
            SectionCard { title: qsTr("充电偏好") }
            ToggleRow { title: qsTr("充满自动断电"); sub: qsTr("电量到 100% 自动停止，防止过充"); on: authStore.autoStop; onChanged: (o) => authStore.autoStop = o }

            Column {
                width: parent.width
                spacing: 8
                Text {
                    text: qsTr("默认充电模式")
                    font.pixelSize: Theme.fontSizeSmall; font.bold: true; color: Theme.textPrimary
                }
                ComboBox {
                    id: chargeModeCombo
                    width: parent.width
                    model: root.chargeModeNames()
                    currentIndex: root.chargeModeIndex()
                    background: Rectangle { color: Theme.card; border.color: Theme.border; border.width: 1; radius: Theme.radiusSmall }
                    contentItem: Text {
                        text: chargeModeCombo.displayText
                        color: Theme.textPrimary; font.pixelSize: Theme.fontSizeBase
                        leftPadding: 12; verticalAlignment: Text.AlignVCenter
                    }
                    onActivated: function(idx){ authStore.chargeMode = root.chargeModeId(idx) }
                }
            }

            // 低电量提醒阈值
            Column {
                width: parent.width
                spacing: 8
                Text {
                    text: qsTr("低电量提醒阈值：") + authStore.lowBatteryThreshold + qsTr("%")
                    font.pixelSize: Theme.fontSizeSmall; font.bold: true; color: Theme.textPrimary
                }
                Slider {
                    width: parent.width
                    from: 5; to: 30; stepSize: 5
                    value: authStore.lowBatteryThreshold
                    onValueChanged: authStore.lowBatteryThreshold = value
                }
                Text {
                    text: qsTr("车辆电量低于该阈值时，自动推送补电提醒")
                    width: parent.width
                    font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary
                }
            }

            // —— 隐私与数据 ——
            SectionCard { title: qsTr("隐私与数据") }
            ToggleRow { title: qsTr("参与大屏互动展示"); sub: qsTr("允许在运营大屏匿名展示我的充电统计"); on: authStore.shareToScreen; onChanged: (o) => authStore.shareToScreen = o }

            // —— 退出按钮 ——
        }
    }

    // 退出账号（钉在底部不参与 ScrollView）
    Button {
        id: logoutBtn
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 32; anchors.rightMargin: 32; anchors.bottomMargin: 40
        width: parent.width - 64; height: 48
        text: qsTr("退出账号")
        onClicked: authStore.logout()
        background: Rectangle { radius: Theme.radiusSmall; color: Theme.danger }
        contentItem: Text {
            text: logoutBtn.text; color: "#ffffff"
            font.pixelSize: Theme.fontSizeBase; font.bold: true
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        }
    }

    // —— 分组标题 ——
    component SectionCard: Column {
        property string title
        spacing: 8
        width: parent.width
        Text {
            text: title
            font.pixelSize: Theme.fontSizeSmall
            font.bold: true
            color: Theme.textPrimary
        }
    }

    // 开关行（标题 + 副标题 + 右侧 Switch）
    component ToggleRow: Rectangle {
        id: tr
        property string title
        property string sub
        property bool on: false
        signal changed(bool on)
        width: parent.width
        height: col.implicitHeight + 18
        color: Theme.card
        border.color: Theme.border; border.width: 1
        radius: Theme.radiusSmall

        Column {
            id: col
            anchors.left: parent.left; anchors.leftMargin: 14
            anchors.right: parent.right; anchors.rightMargin: 62
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3
            Text {
                width: parent.width
                text: tr.title
                color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: tr.sub
                color: Theme.textSecondary; font.pixelSize: Theme.fontSizeTiny
                wrapMode: Text.Wrap
            }
        }
        Switch {
            anchors.right: parent.right; anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            Component.onCompleted: checked = tr.on
            onToggled: tr.changed(checked)
        }
    }
}