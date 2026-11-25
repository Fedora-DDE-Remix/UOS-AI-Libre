import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root
    property string initialModel
    property string initialApiBase
    property bool decisionMade: false
    signal saveRequested(string model, string apiBase, string apiKey)

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    width: 480
    padding: 0
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openWith(model, apiBase) {
        initialModel = model
        initialApiBase = apiBase
        modelField.text = model
        baseField.text = apiBase
        keyField.text = ""
        decisionMade = false
        open()
        modelField.forceInputFocus()
    }

    function save() {
        const model = modelField.text.trim()
        const base = baseField.text.trim()
        if (model.length === 0 || base.length === 0)
            return
        root.decisionMade = true
        root.close()
        root.saveRequested(model, base, keyField.text)
    }

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder
    }

    contentItem: Column {
        width: root.width
        topPadding: 24
        leftPadding: 26
        rightPadding: 26
        bottomPadding: 20
        spacing: 14

        Text {
            text: "模型配置"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 17
            font.weight: Font.DemiBold
        }

        Text {
            width: parent.width - 52
            text: "配置将写入 aish 的 config.yaml。API Key 只保存在本机，不会显示在界面日志中。"
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            lineHeight: 1.5
            wrapMode: Text.Wrap
        }

        Column {
            width: parent.width - 52
            spacing: 6
            Text {
                text: "模型名称"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }
            AppTextField {
                id: modelField
                width: parent.width
                placeholder: "例如 deepseek-chat"
            }
        }

        Column {
            width: parent.width - 52
            spacing: 6
            Text {
                text: "API 地址"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }
            AppTextField {
                id: baseField
                width: parent.width
                placeholder: "https://api.deepseek.com"
            }
        }

        Column {
            width: parent.width - 52
            spacing: 6
            Text {
                text: "API Key（留空则保留现有密钥）"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }
            AppTextField {
                id: keyField
                width: parent.width
                placeholder: "sk-..."
                password: true
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 26
            spacing: 10

            AppButton {
                text: "取消"
                minWidth: 82
                onClicked: root.close()
            }

            AppButton {
                text: "保存"
                minWidth: 82
                variant: "primary"
                onClicked: root.save()
            }
        }
    }
}
