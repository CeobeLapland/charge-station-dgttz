import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UserClient

// 结算页：充电完成后的统一结算。展示明细 + 选券 + 占位费，确认后从余额扣款。
// 结算动作本属服务端(order.settle)，当前经 ChargingFlow mock 完成；给出充值与失败提示。
Item {
    id: root
    readonly property var stackView: StackView.view
    readonly property var f: ChargingFlow.flow

    readonly property var carbonFactor: 0.7
    readonly property var coupons: UserData.coupons().filter(function(c){ return c.status === "unused" })
    property int matchedCouponIndex: -1

    function couponLabel() { return matchedCouponIndex >= 0 ? coupons[matchedCouponIndex].title : "" }
    function couponDiscount() { return matchedCouponIndex >= 0 ? Number(coupons[matchedCouponIndex].discount_amount) : 0 }

    Rectangle { anchors.fill: parent; color: Theme.background }

    // 返回 + 标题
    Rectangle {
        width: parent.width; height: 76; color: Theme.primary
        Row {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 8; anchors.rightMargin: 20; spacing: 8
            Text {
                text: "‹"; font.pixelSize: 28; color: "#ffffff"
                MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: root.stackView.pop() }
            }
            Text { text: qsTr("结算"); color: "#ffffff"; font.pixelSize: Theme.fontSizeTitle; font.bold: true }
        }
    }

    ScrollView {
        id: sv
        anchors.top: parent.top; anchors.topMargin: 76
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: payBar.visible ? payBar.top : parent.bottom
        clip: true
        Column {
            width: sv.width; spacing: 16; leftPadding: 16; rightPadding: 16; topPadding: 16; bottomPadding: 24

            // 状态卡
            Rectangle {
                width: parent.width; height: 84; radius: Theme.radiusSmall
                gradient: Gradient { GradientStop { position: 0; color: Theme.primary } GradientStop { position: 1; color: Theme.accent } }
                Column { anchors.left: parent.left; anchors.leftMargin: 16; anchors.verticalCenter: parent.verticalCenter; spacing: 3
                    Text { text: qsTr("充电完成，请确认结算"); color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeBase }
                    Text { text: f.station_name + " · " + f.charger_code; color: "#E0ECFF"; font.pixelSize: Theme.fontSizeTiny }
                }
            }

            Section { title: qsTr("本期充电") }
            Rectangle {
                width: parent.width
                height: col.implicitHeight + 20
                radius: Theme.radiusSmall; color: Theme.card; border.color: Theme.border
                Column {
                    id: col
                    anchors.left: parent.left; anchors.leftMargin: 14; anchors.top: parent.top; anchors.topMargin: 10
                    spacing: 6
                    KV { k: qsTr("充电量"); v: (Number(f.energy_kwh || 0).toFixed(1)) + " kWh" }
                    KV { k: qsTr("时长"); v: (Number(f.duration_min || 0).toFixed(1)) + qsTr(" 分钟") }
                    KV { k: qsTr("起始 → 结束电量"); v: Number(f.start_soc || 0) + "% → " + Number(f.end_soc || 0) + "%" }
                    KV { k: qsTr("结算单价"); v: "\u{FFE5}" + (Number(f.unit_price || 0).toFixed(2)) + " /kWh（含服务费）" }
                }
            }

            Section { title: qsTr("费用明细") }
            Rectangle {
                width: parent.width
                height: feeCol.implicitHeight + 20
                radius: Theme.radiusSmall; color: Theme.card; border.color: Theme.border
                Column {
                    id: feeCol
                    anchors.left: parent.left; anchors.leftMargin: 14; anchors.top: parent.top; anchors.topMargin: 10
                    spacing: 6
                    KV { k: qsTr("应收金额"); v: "\u{FFE5}" + (Number(f.amount || 0).toFixed(2)) }
                    KV { k: qsTr("占位费"); v: "\u{FFE5}" + (Number(f.occupy_fee || 0).toFixed(2)) + (Number(f.occupy_min||0)>0 ? ("（占位 " + Number(f.occupy_min) + " 秒）") : "")
                         ; c: (Number(f.occupy_fee) > 0) ? Theme.warn : Theme.textSecondary }
                    KV { k: qsTr("优惠券"); v: couponLabel() === "" ? qsTr("未使用") : ("-" + couponDiscount().toFixed(2) + " 元 · " + couponLabel()) ; c: Theme.success }
                    Rectangle { width: parent.width - 28; height: 1; color: Theme.border }
                    KV { k: qsTr("本单实付"); v: "\u{FFE5}" + (Number(f.pay_amount || 0).toFixed(2)); c: Theme.danger }
                    KV { k: qsTr("预计获得积分"); v: "+" + (f.points_earned || 0); c: Theme.success }
                }
            }

            Section { title: qsTr("优惠券") }
            Rectangle {
                width: parent.width
                height: couponArea.implicitHeight + 20
                radius: Theme.radiusSmall; color: Theme.card; border.color: Theme.border
                Column {
                    id: couponArea
                    anchors.left: parent.left; anchors.leftMargin: 14; anchors.top: parent.top; anchors.topMargin: 10
                    width: parent.width - 28
                    spacing: 8
                    Text {
                        visible: root.coupons.length > 0
                        text: qsTr("可抵扣：请选择一张券"); font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary
                    }
                    Repeater {
                        model: root.coupons
                        delegate: Rectangle {
                            width: parent.width; height: 34; radius: Theme.radiusSmall
                            color: root.matchedCouponIndex === index ? Theme.success + "1A" : Theme.background
                            border.color: root.matchedCouponIndex === index ? Theme.success : Theme.border; border.width: 1
                            Row {
                                anchors.left: parent.left; anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 10; anchors.rightMargin: 10
                                Text { text: modelData.title || ""; font.pixelSize: Theme.fontSizeTiny; color: Theme.textPrimary
                                       elide: Text.ElideRight; width: parent.width - 60 }
                                Text { text: qsTr("满 ") + (Number(modelData.min_amount || 0).toFixed(0)) + qsTr(" 减 ")
                                + (Number(modelData.discount_amount || 0).toFixed(2))
                                ; color: Theme.success; font.bold: true; font.pixelSize: Theme.fontSizeTiny }
                            }
                            MouseArea { anchors.fill: parent; onClicked: root.matchedCouponIndex = (root.matchedCouponIndex === index ? -1 : index) }
                        }
                    }
                    Text {
                        visible: root.coupons.length === 0
                        text: qsTr("暂无可用券（新人券登录时自动发放）"); font.pixelSize: Theme.fontSizeTiny; color: Theme.textSecondary
                    }
                }
            }

            Section { title: qsTr("绿色贡献") }
            Row { width: parent.width; spacing: 14
                Item { width: 1; height: 1 }
                Text { text: "\u{1F331} 本次充电 " + (Number(f.energy_kwh||0).toFixed(1)) + " kWh，估算减碳 "
                         + (Number(f.energy_kwh||0) * carbonFactor).toFixed(2) + " kg CO\u2082"
                       ; color: Theme.success; font.pixelSize: Theme.fontSizeSmall; wrapMode: Text.Wrap; width: parent.width - 20 }
            }
        }
    }

    // 底部支付栏
    Rectangle {
        id: payBar
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 90; color: "#00000000"
        Rectangle {
            width: parent.width - 32; x: 16; height: 54; radius: 27
            gradient: Gradient { GradientStop { position: 0; color: Theme.danger } GradientStop { position: 1; color: "#f87171" } }
            Text {
                anchors.centerIn: parent
                text: qsTr("确认支付 ¥") + root.displayPay().toFixed(2)
                color: "#ffffff"; font.bold: true; font.pixelSize: Theme.fontSizeBase
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    var cid = root.matchedCouponIndex >= 0 ? Number(root.coupons[root.matchedCouponIndex].id) : 0
                    ChargingFlow.settle(cid)
                    if (ChargingFlow.phase === "done") root.showSuccess()
                }
            }
        }
        function doPay() {
            var pay = Number(f.pay_amount || 0) // 已在 settle 内扣减过券
            var occ = Number(f.occupy_fee || 0)
            return (pay + occ)
        }
    }
    // 结算前展示的预估实付（券由页面选中，结算后由服务端重算 pay_amount）
    function displayPay() {
        var amount = Number(f.amount || 0)
        var disc = couponDiscount()
        var occ = Number(f.occupy_fee || 0)
        return Math.max(0, amount - disc) + occ
    }

    // 结算成功覆盖层：由支付成功触发；服务端异常通过 toast 反馈
    function showSuccess() {
        success.visible = true
    }
    Connections {
        target: ChargingFlow
        function onAbnormal(title, sub) {
            toastText.text = title + "：" + sub
            toast.visible = true
            toastTimer.restart()
        }
    }
    Rectangle {
        id: success
        visible: false
        anchors.fill: parent; color: "#E6FFFFFF"; z: 40
        Column {
            anchors.centerIn: parent; spacing: 16
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "✅"; font.pixelSize: 56 }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("结算成功"); font.bold: true; font.pixelSize: Theme.fontSizeTitle; color: Theme.textPrimary }
            Text { anchors.horizontalCenter: parent.horizontalCenter
                   text: qsTr("实付 ¥") + (Number(f.pay_amount||0) + Number(f.occupy_fee||0)).toFixed(2)
                         + " · 获得 +" + (f.points_earned || 0) + " 积分"
                   ; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall }
            Rectangle {
                width: 200; height: 46; radius: 23; color: Theme.primary
                Text { anchors.centerIn: parent; text: qsTr("完成，返回首页"); color: "#ffffff"; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: root.backHome() }
            }
        }
    }

    function backHome() {
        ChargingFlow.reset()
        for (var i = 0; i < root.stackView.depth; i++) root.stackView.pop()
        root.stackView.replace("qrc:/UserClient/qml/pages/HomePage.qml")
    }

    // 轻提示
    Rectangle {
        id: toast; visible: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 110
        width: Math.min(toastText.implicitWidth + 32, root.width - 48); height: 40; radius: 20
        color: "#B3000000"; z: 30
        Text { id: toastText; anchors.centerIn: parent; text: ""; color: "#ffffff"; font.pixelSize: Theme.fontSizeSmall }
    }
    Timer { id: toastTimer; interval: 2400; onTriggered: toast.visible = false }

    component Section: Column {
        property string title
        spacing: 2
        Text { text: parent.title; font.pixelSize: Theme.fontSizeSmall + 1; font.bold: true; color: Theme.textPrimary }
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