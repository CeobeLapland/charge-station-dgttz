import QtQuick
import QtQuick.Controls
import UserClient

// 用户协议与软件介绍：展示本 App 的软件简介与用户协议条款（静态文案，后续可依据服务端同步）。
Item {
    id: root
    readonly property var stackView: StackView.view

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
        text: qsTr("用户协议与软件介绍")
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
            spacing: 20

            // —— 软件介绍 ——
            SectionCard { title: qsTr("⚡ 软件介绍") }
            Rectangle {
                width: parent.width - 32; x: 16
                height: introCol.implicitHeight + 28
                color: Theme.card; border.color: Theme.border; border.width: 1; radius: Theme.radiusSmall
                Column {
                    id: introCol
                    width: parent.width - 24; x: 12; y: 14
                    spacing: 10
                    Text {
                        width: parent.width
                        lineHeight: 1.6
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textPrimary
                        text: qsTr("本软件是一款电动汽车充电运营平台，为用户提供电站查询、实时预约、扫码充电、订单管理与费用结算等一体化服务。")
                    }
                    Text {
                        width: parent.width
                        lineHeight: 1.6
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textSecondary
                        text: qsTr("主要功能包括：\n"
                                 + "· 电站体检：查看附近电站位置、空闲枪位、实时电价\n"
                                 + "· 预约充电：提前锁定枪位，到站即充\n"
                                 + "· 扫码启动：扫码连接充电桩，自动计费并结算\n"
                                 + "· 会员与优惠券：享折扣电价、积分增值\n"
                                 + "· 订单与发票：随时可查订单明细并一键开票\n"
                                 + "· 车辆管理：管理常用车辆与充电偏好")
                    }
                }
            }

            // —— 服务条款 ——
            SectionCard { title: qsTr("📜 用户协议") }
            Rectangle {
                width: parent.width - 32; x: 16
                height: termsCol.implicitHeight + 28
                color: Theme.card; border.color: Theme.border; border.width: 1; radius: Theme.radiusSmall
                Column {
                    id: termsCol
                    width: parent.width - 24; x: 12; y: 14
                    spacing: 14
                    Text {
                        width: parent.width
                        lineHeight: 1.6
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textSecondary
                        text: qsTr("欢迎使用本充电运营平台。请您在使用本软件前仔细阅读以下条款。您开始使用即视为同意本协议全部内容。")
                    }
                    ProtocolItem { title: qsTr("一、账号与安全")
                        body: qsTr("您应如实提供注册信息并对账号下的全部行为负责。请妥善保管账号与密码，因泄露导致的损失由您自行承担。") }
                    ProtocolItem { title: qsTr("二、服务内容")
                        body: qsTr("本软件提供充电站信息查询、预约充电、充电启动与订单结算等服务。服务内容与服务时间可能因运营商安排调整，请以页面实际展示为准。") }
                    ProtocolItem { title: qsTr("三、费用与结算")
                        body: qsTr("充电费用由电价与服务费构成，具体以您在充电前确认的价格为准。订单完成后自动从余额或绑定支付方式中扣款，如需退款请按平台规则申请。") }
                    ProtocolItem { title: qsTr("四、用户行为规范")
                        body: qsTr("不得利用本软件从事违法活动、恶意刷单、攻击系统或损害他人权益。如发生上述行为，平台有权停用相关账号并追究责任。") }
                    ProtocolItem { title: qsTr("五、隐私保护")
                        body: qsTr("我们仅在提供服务所必需的范围内收集与使用您的信息，并采取合理的安全措施予以保护。具体详见《隐私政策》。") }
                    ProtocolItem { title: qsTr("六、免责声明")
                        body: qsTr("因不可抗力、网络故障、第三方服务异常等导致的服务中断，平台在合理范围内不承担责任。请您在允许的时间段合规使用充电服务。") }
                    ProtocolItem { title: qsTr("七、协议变更与联系方式")
                        body: qsTr("平台可适时修订本协议，并在本页面公示。如有疑问，欢迎随时联系在线客服或致电客服热线咨询。") }
                }
            }

            // —— 底部说明 ——
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeTiny
                color: Theme.textSecondary
                topPadding: 8; bottomPadding: 16
                text: qsTr("最后更新：2026-09 · 版本 1.0")
            }
        }
    }

    // 分组标题
    component SectionCard: Text {
        property string title
        text: title
        x: 16
        font.pixelSize: Theme.fontSizeSmall + 1
        font.bold: true
        color: Theme.textPrimary
        topPadding: 8
    }

    // 协议条目（标题加粗 + 正文）
    component ProtocolItem: Column {
        property string title
        property string body
        width: parent.width
        spacing: 4
        Text {
            width: parent.width
            text: title
            font.pixelSize: Theme.fontSizeSmall
            font.bold: true
            color: Theme.textPrimary
        }
        Text {
            width: parent.width
            text: body
            lineHeight: 1.6
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.textSecondary
        }
    }
}