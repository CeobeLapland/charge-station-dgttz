import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import UserClient

// 我要申诉页：账号 + 文字描述（≤2000 字）+ 图片（≤5 张）。
// 提交后本地记录申诉（对应 work_order：type=user_complaint、status=pending），提示等待审核并返回登录页。
Item {
    id: root

    // 从登录页带入的账号（可为空，允许手动输入）
    property string account: ""

    readonly property var stackView: StackView.view

    // 图片列表（file:// 路径，最多 5 张）
    ListModel { id: imageModel }

    // 不透明背景：遮住下层登录页（页面根 Item 默认透明）
    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    // 顶部返回
    Rectangle {
        width: 40
        height: 40
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 20
        anchors.leftMargin: 16
        radius: 20
        color: Theme.background
        border.color: Theme.border
        border.width: 1
        Text {
            anchors.centerIn: parent
            text: "‹"
            font.pixelSize: 24
            color: Theme.primary
        }
        MouseArea {
            anchors.fill: parent
            onClicked: root.stackView.pop()
        }
    }

    // 标题
    Column {
        anchors.top: parent.top
        anchors.topMargin: 40
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("我要申诉")
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            color: Theme.textPrimary
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("账号异常？提交申诉等待平台审核")
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.textSecondary
        }
    }

    // 表单（可滚动）
    Flickable {
        id: formFlick
        anchors.top: parent.top
        anchors.topMargin: 130
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 32
        anchors.rightMargin: 32
        clip: true
        contentHeight: formCol.height

        Column {
            id: formCol
            width: formFlick.width
            spacing: 14

            // 账号
            Column {
                width: parent.width
                spacing: 6
                Text {
                    text: qsTr("账号")
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textSecondary
                }
                Rectangle {
                    width: parent.width
                    height: 48
                    radius: Theme.radiusSmall
                    color: Theme.card
                    border.color: accountField.activeFocus ? Theme.primary : Theme.border
                    border.width: 1
                    TextField {
                        id: accountField
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        verticalAlignment: Text.AlignVCenter
                        placeholderText: qsTr("请输入被冻结的账号")
                        maximumLength: 20
                        text: root.account
                    }
                }
            }

            // 文字描述（≤2000 字）
            Column {
                width: parent.width
                spacing: 6
                Text {
                    text: qsTr("申诉描述")
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textSecondary
                }
                Rectangle {
                    width: parent.width
                    height: 140
                    radius: Theme.radiusSmall
                    color: Theme.card
                    border.color: descField.activeFocus ? Theme.primary : Theme.border
                    border.width: 1

                    Flickable {
                        id: descFlick
                        anchors.fill: parent
                        anchors.margins: 8
                        clip: true
                        contentWidth: descField.width
                        contentHeight: descField.height
                        TextArea.flickable: TextArea {
                            id: descField
                            width: descFlick.width
                            wrapMode: TextArea.Wrap
                            placeholderText: qsTr("请描述您的账号情况、遇到的问题等（最多 2000 字）")
                            // TextArea 没有 maximumLength 属性（TextField 才有），手动截断到 2000 字
                            onTextChanged: {
                                if (text.length > 2000)
                                    text = text.substring(0, 2000)
                            }
                        }
                    }
                }
                // 字数统计
                Text {
                    anchors.right: parent.right
                    text: qsTr("%1 / 2000").arg(descField.text.length)
                    font.pixelSize: Theme.fontSizeTiny
                    color: Theme.textSecondary
                }
            }

            // 图片（≤5 张）
            Column {
                width: parent.width
                spacing: 6
                Text {
                    text: qsTr("图片证据（最多 5 张）")
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textSecondary
                }
                Flow {
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: imageModel
                        delegate: Rectangle {
                            required property int index
                            required property string url
                            width: 72
                            height: 72
                            radius: Theme.radiusSmall
                            clip: true
                            color: Theme.background
                            border.color: Theme.border
                            border.width: 1

                            Image {
                                anchors.fill: parent
                                source: url
                                fillMode: Image.PreserveAspectCrop
                            }
                            // 右上角删除
                            Rectangle {
                                anchors.top: parent.top
                                anchors.right: parent.right
                                width: 20
                                height: 20
                                radius: 10
                                color: Theme.danger
                                Text {
                                    anchors.centerIn: parent
                                    text: "×"
                                    color: "#ffffff"
                                    font.pixelSize: 14
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: imageModel.remove(index)
                                }
                            }
                        }
                    }

                    // 添加图片（未满 5 张时显示）
                    Rectangle {
                        width: 72
                        height: 72
                        radius: Theme.radiusSmall
                        visible: imageModel.count < 5
                        color: Theme.card
                        border.color: Theme.border
                        border.width: 1
                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "+"
                                font.pixelSize: 24
                                color: Theme.primary
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: qsTr("添加")
                                font.pixelSize: Theme.fontSizeTiny
                                color: Theme.textSecondary
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: fileDlg.open()
                        }
                    }
                }
            }

            // 提示/错误
            Text {
                id: errText
                width: parent.width
                visible: false
                text: qsTr("")
                color: Theme.danger
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.Wrap
            }

            // 提交按钮
            Button {
                id: submitBtn
                width: parent.width
                height: 48
                text: qsTr("提交申诉")
                onClicked: submit()
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.primary
                }
                contentItem: Text {
                    text: submitBtn.text
                    color: "#ffffff"
                    font.pixelSize: Theme.fontSizeBase
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    // 本地图片选择
    FileDialog {
        id: fileDlg
        title: qsTr("选择图片")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)")]
        onAccepted: {
            var remain = 5 - imageModel.count
            var picked = fileDlg.selectedFiles
            for (var i = 0; i < picked.length && i < remain; i++)
                imageModel.append({ "url": picked[i] })
        }
    }

    // 提交成功弹窗：返回登录页
    ConfirmDialog {
        id: successDlg
        titleText: qsTr("提交成功")
        messageText: qsTr("您的申诉已提交，请等待平台审核。")
        okText: qsTr("确定")
        cancelText: qsTr("取消")
        onConfirmed: root.stackView.pop()
        onCancelled: { }
    }

    function submit() {
        var acc = accountField.text.trim()
        if (acc.length === 0) { showErr(qsTr("请输入账号")); return }
        if (descField.text.trim().length === 0) { showErr(qsTr("请填写申诉描述")); return }

        var urls = []
        for (var i = 0; i < imageModel.count; i++)
            urls.push(imageModel.get(i).url)

        authStore.submitAppeal(acc, descField.text, urls)
        successDlg.open()
    }

    function showErr(msg) {
        errText.text = msg
        errText.visible = true
    }
}
