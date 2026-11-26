import QtQuick
import UosAiLibre

SettingsScroll {
    id: page

    onVisibleChanged: { if (visible) app.refreshMcpStatus() }

    function statusColor(status) {
        if (status === "connected")
            return Theme.success
        if (status === "failed")
            return Theme.danger
        if (status === "connecting")
            return Theme.warning
        return Theme.textTertiary
    }

    Text {
        text: "MCP 连接器"
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: 18
        font.weight: Font.DemiBold
    }
    Text {
        width: parent.width
        text: "服务器、工具、信任级别和副作用都来自 aish 的 config.yaml 与运行中的 MCP manager。新增或修改 server 请编辑 aish 配置后重载。"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Row {
        spacing: 8
        AppButton { text: "刷新"; onClicked: app.refreshMcpStatus() }
        AppButton { text: "重载 MCP"; variant: "primary"; onClicked: app.reloadMcp() }
        AppButton { text: "打开 config.yaml"; onClicked: app.openConfigFile() }
    }

    Rectangle {
        width: parent.width
        height: emptyColumn.implicitHeight + 30
        visible: !app.mcpStatus.servers || app.mcpStatus.servers.length === 0
        radius: Theme.radiusMedium
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder
        Column {
            id: emptyColumn
            x: 16
            y: 15
            width: parent.width - 32
            spacing: 8
            Text {
                text: "尚未配置 MCP server"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.Medium
            }
            Text {
                width: parent.width
                text: "可在 aish CLI 中使用 /mcp add，或在 config.yaml 的 mcp.servers 中添加后重载。"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
        }
    }

    Repeater {
        model: app.mcpStatus.servers || []
        delegate: Rectangle {
            width: parent.width
            height: serverColumn.implicitHeight + 28
            radius: Theme.radiusMedium
            color: Theme.card
            border.width: 1
            border.color: Theme.cardBorder

            Column {
                id: serverColumn
                x: 16
                y: 14
                width: parent.width - 32
                spacing: 7
                Row {
                    width: parent.width
                    spacing: 8
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, serverColumn.width - 190)
                        text: modelData.name
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: statusText.implicitWidth + 14
                        height: 20
                        radius: 4
                        color: Qt.rgba(page.statusColor(modelData.status).r,
                                       page.statusColor(modelData.status).g,
                                       page.statusColor(modelData.status).b, 0.12)
                        Text {
                            id: statusText
                            anchors.centerIn: parent
                            text: modelData.status || "unknown"
                            color: page.statusColor(modelData.status)
                            font.family: "DejaVu Sans Mono"
                            font.pixelSize: 9
                        }
                    }
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: trustText.implicitWidth + 14
                        height: 20
                        radius: 4
                        color: Theme.codeBg
                        Text {
                            id: trustText
                            anchors.centerIn: parent
                            text: "trust: " + (modelData.trust || "ask")
                            color: Theme.textSecondary
                            font.family: "DejaVu Sans Mono"
                            font.pixelSize: 9
                        }
                    }
                }
                Text {
                    width: parent.width
                    text: (modelData.transport || "") + "  ·  " + (modelData.endpoint || "")
                    color: Theme.textTertiary
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: Theme.fontSizeTiny
                    elide: Text.ElideMiddle
                }
                Text {
                    width: parent.width
                    text: "副作用: " + (modelData.declared_side_effects || "unknown") +
                          "   ·   工具: " + (modelData.tools ? modelData.tools.length : 0)
                    color: Theme.textTertiary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTiny
                }
                Repeater {
                    model: modelData.tools || []
                    delegate: Text {
                        width: parent.width
                        text: "• " + modelData.name +
                              (modelData.side_effects ? "  [" + modelData.side_effects + "]" : "")
                        color: Theme.textSecondary
                        font.family: "DejaVu Sans Mono"
                        font.pixelSize: Theme.fontSizeTiny
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    Text {
        width: parent.width
        text: "MCP 工具调用会经过 aish 的审批和 trust 规则；unknown 副作用的工具默认不会自动放行。"
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeTiny
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }
}
