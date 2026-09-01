import QtQuick
import QtQuick.Controls
import UserClient

// 手机号免密登录（spec-用户端：非 11 位数字禁用登录按钮并提示）
Item {
    id: root
    property int navIndex: 0

    Column {
        anchors.centerIn: parent
        spacing: 16
        width: root.width - 48

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("欢迎使用充电用户端")
            font.pixelSize: 26
            font.bold: true
            color: "#212121"
        }

        TextField {
            id: phoneField
            width: parent.width
            placeholderText: qsTr("请输入 11 位手机号")
            inputMethodHints: Qt.ImhDigitsOnly
            maxLength: 11
            validator: RegularExpressionValidator { regularExpression: /[0-9]+/ }
            onTextChanged: hint.visible = text.length > 0 && !/^[0-9]{11}$/.test(text)
        }

        Text {
            id: hint
            visible: false
            text: qsTr("请输入 11 位数字手机号")
            color: "#e53935"
            font.pixelSize: 12
        }

        Button {
            id: loginBtn
            width: parent.width
            text: qsTr("登录")
            enabled: /^[0-9]{11}$/.test(phoneField.text)
            onClicked: backend.login(phoneField.text)
        }
    }
}