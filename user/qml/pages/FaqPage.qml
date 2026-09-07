import QtQuick
import QtQuick.Controls
import UserClient

// 常见问题与解答：预设常用问题，点击展开查看简要解答；顶部提示可咨询客服。
Item {
    id: root
    readonly property var stackView: StackView.view

    // 预设问题与解答（Q 问题 / A 解答）
    readonly property var faqs: [
        { q: qsTr("如何找到最近的充电站？"),
          a: qsTr("进入首页即可看到周边电站，支持按距离、价格与空闲枪位排序，也可在地图上拖拽查看任意范围的电站。") },
        { q: qsTr("预约充电后可以取消吗？"),
          a: qsTr("可以。若未开始充电，可在订单详情中取消预约，预约费会按规定退还；已开始充电的订单请先联系客服处理。") },
        { q: qsTr("充电费用如何计算？"),
          a: qsTr("费用 = 充电电量 × 实时电价 + 服务费。单价在扫码启动前会明确展示，会员还可享受专属折扣。") },
        { q: qsTr("为什么我在充电时拔枪会无法启动？"),
          a: qsTr("请确认充电枪已完全插好且车辆处于可充电状态。若仍无法启动，可尝试更换空闲枪位或联系客服协助排查。") },
        { q: qsTr("如何申请开发票？"),
          a: qsTr("在「我的订单」中选择对应订单，点击「开发票」填写开票信息即可。电子发票将在提交后尽快开具并发送至您的邮箱。") },
        { q: qsTr("优惠券怎么使用？有效期多久？"),
          a: qsTr("下单结算时可选择可用的优惠券自动抵扣。每张券均有明确有效期，请在有效期内使用，过期自动失效。") },
        { q: qsTr("余额如何提现或退款？"),
          a: qsTr("未消费的余额可联系客服申请退款，审核通过后按原支付渠道退回。已产生订单的费用不在此范围内。") },
        { q: qsTr("遇到充电桩故障或异常账单怎么办？"),
          a: qsTr("请先保留现场照片与订单截图，并在故障或账单页面提交反馈或直接咨询客服，我们会在核实后尽快处理。") },
        { q: qsTr("为什么我的会员状态显示已过期？"),
          a: qsTr("会员到期后未续费即显示为过期。可在「会员中心」查看到期时间并续费，续费成功后折扣与积分权益立即恢复。") }
    ]

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
        text: qsTr("常见问题与解答")
        font.pixelSize: Theme.fontSizeTitle
        font.bold: true
        color: Theme.textPrimary
    }

    ScrollView {
        anchors.fill: parent
        anchors.topMargin: 88
        clip: true

        Column {
            width: root.width
            spacing: 12

            // —— 顶部提示：可咨询客服 ——
            Rectangle {
                width: parent.width - 32; x: 16
                height: bannerCol.implicitHeight + 28
                color: Theme.primary + "10"
                border.color: Theme.primary + "33"
                border.width: 1
                radius: Theme.radiusSmall
                Column {
                    id: bannerCol
                    width: parent.width - 28; x: 14; y: 14
                    spacing: 8
                    Text {
                        text: qsTr("工作时间 8:00 - 22:00，其余时间请留言")
                        font.pixelSize: Theme.fontSizeTiny
                        color: Theme.primary
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        lineHeight: 1.5
                        text: qsTr("如果以上解答没能解决您的问题，欢迎点击下方按钮联系在线客服，我们会尽快为您处理。")
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textPrimary
                    }
                    Rectangle {
                        height: 38; width: parent.width
                        radius: Theme.radiusSmall
                        color: Theme.primary
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("联系客服")
                            color: "#ffffff"
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: true
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: serviceDlg.open()
                        }
                    }
                }
            }

            // —— 常见问题列表 ——
            Repeater {
                model: root.faqs
                delegate: FaqItem {
                    faq: modelData
                    faqIndex: index
                }
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeTiny
                color: Theme.textSecondary
                topPadding: 8; bottomPadding: 16
                text: qsTr("—— 暂无更多 ——")
            }
        }
    }

    // 单条问答项（点击展开/收起）
    component FaqItem: Rectangle {
        id: item
        property var faq: ({})
        property int faqIndex: -1
        readonly property bool open: itemState === faqIndex
        property int itemState: -1
        width: parent.width - 32
        x: 16
        height: item.open ? 60 + ansCol.implicitHeight : 52
        color: Theme.card
        border.color: Theme.border; border.width: 1
        radius: Theme.radiusSmall

        // 问题行
        Rectangle {
            id: qRow
            width: parent.width
            height: 52
            color: "transparent"
            Text {
                anchors.left: parent.left; anchors.leftMargin: 14
                anchors.right: parent.right; anchors.rightMargin: 40
                anchors.verticalCenter: parent.verticalCenter
                text: item.faq.q || ""
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeSmall
                font.bold: true
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }
            Text {
                anchors.right: parent.right; anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: item.open ? "−" : "+"
                color: Theme.primary
                font.pixelSize: 20
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                onClicked: item.itemState = item.open ? -1 : item.faqIndex
            }
        }

        // 解答内容（展开后由 faqItem.height 计入高度）
        Column {
            id: ansCol
            visible: item.open
            width: parent.width - 28
            anchors.left: parent.left; anchors.leftMargin: 14
            anchors.top: qRow.bottom; anchors.topMargin: 10
            spacing: 10
            Rectangle {
                width: parent.width; height: 1; color: Theme.border
            }
            Text {
                width: parent.width
                lineHeight: 1.6
                wrapMode: Text.Wrap
                text: (item.faq.a || "")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.textSecondary
            }
        }
    }

    // 联系客服弹窗
    Dialog {
        id: serviceDlg
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(320, Overlay.overlay ? Overlay.overlay.width - 64 : 320)
        padding: 20
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { radius: Theme.radius; color: Theme.card }

        Column {
            spacing: 14
            Text { text: qsTr("联系客服"); font.pixelSize: Theme.fontSizeTitle; font.bold: true; color: Theme.textPrimary }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                lineHeight: 1.6
                text: qsTr("客服热线：400-888-0000\n")
                      + qsTr("在线客服：进入「我的」→ 我的评论 留言\n")
                      + qsTr("服务时间：每天 8:00 - 22:00")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.textPrimary
            }
            Rectangle {
                height: 40; width: parent.width
                radius: Theme.radiusSmall
                color: Theme.primary
                Text {
                    anchors.centerIn: parent
                    text: qsTr("知道了")
                    color: "#ffffff"; font.bold: true
                }
                MouseArea { anchors.fill: parent; onClicked: serviceDlg.close() }
            }
        }
    }
}