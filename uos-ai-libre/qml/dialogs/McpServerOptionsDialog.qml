import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root
    property string serverName: ""
    property bool serverEnabled: true
    property string trust: "ask"
    property string sideEffects: "unknown"
    property string expose: "gateway"
    signal saved(var args)

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    width: Math.min(500, parent ? parent.width - 48 : 500)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    function openFor(server) {
        serverName = server.name || ""
        serverEnabled = server.enabled !== false
        trust = server.trust || "ask"
        sideEffects = server.declared_side_effects || "unknown"
        expose = server.expose || "gateway"
        open()
    }

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder
    }

    contentItem: Column {
        width: root.width
        padding: 22
        spacing: 13

        Text {
            text: "MCP 设置 · " + root.serverName
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 16
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            width: parent.width
        }
        Text {
            width: parent.width
            text: "连接参数（命令、URL、env、headers）仍在 config.yaml 中编辑；这里只调整信任与暴露策略。"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeTiny
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }

        Row {
            width: parent.width
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "启用服务器"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }
            Item { width: Math.max(0, parent.width - 120); height: 1 }
            ToggleSwitch {
                anchors.verticalCenter: parent.verticalCenter
                checked: root.serverEnabled
                onToggled: (value) => root.serverEnabled = value
            }
        }

        Column {
            width: parent.width
            spacing: 5
            Text { text: "服务器默认 trust"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
            SettingChoice {
                options: [{value: "ask", label: "ask"},
                          {value: "allow", label: "allow"},
                          {value: "block", label: "block"}]
                value: root.trust
                onChanged: (value) => root.trust = value
            }
            Text {
                width: parent.width
                text: "allow 只对可证明为只读的工具免确认；unknown/write 仍会确认，block 永远阻断。"
                color: Theme.textTertiary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTiny
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
        }

        Column {
            width: parent.width
            spacing: 5
            Text { text: "声明副作用"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
            SettingChoice {
                options: [{value: "read", label: "read"},
                          {value: "network", label: "network"},
                          {value: "write", label: "write"},
                          {value: "unknown", label: "unknown"}]
                value: root.sideEffects
                onChanged: (value) => root.sideEffects = value
            }
        }

        Column {
            width: parent.width
            spacing: 5
            Text { text: "工具暴露方式"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
            SettingChoice {
                options: [{value: "gateway", label: "gateway"},
                          {value: "tools", label: "tools"}]
                value: root.expose
                onChanged: (value) => root.expose = value
            }
        }

        Row {
            anchors.right: parent.right
            spacing: 8
            AppButton { text: "取消"; onClicked: root.close() }
            AppButton {
                text: "保存"
                variant: "primary"
                onClicked: {
                    const args = {
                        enabled: root.serverEnabled,
                        trust: root.trust,
                        side_effects: root.sideEffects,
                        expose: root.expose
                    }
                    root.close()
                    root.saved(args)
                }
            }
        }
    }
}
