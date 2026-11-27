import QtQuick
import UosAiLibre

SettingsScroll {
    id: page

    onVisibleChanged: { if (visible) app.refreshToolCatalog() }

    function sourceLabel(source) {
        if (source === "mcp")
            return "MCP"
        if (source === "subagent")
            return "子代理"
        if (source === "skill")
            return "技能"
        return "aish"
    }

    function sourceIcon(source) {
        if (source === "mcp")
            return "connector"
        if (source === "subagent")
            return "thinking"
        if (source === "skill")
            return "skill"
        return "tool"
    }

    Text {
        text: "工具与子代理"
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: 18
        font.weight: Font.DemiBold
    }
    Text {
        width: parent.width
        text: "这些工具由 aish 直接注册。GUI 不提供工具开关；能否执行、是否需要审批由 aish 安全策略和工具自身的 preflight 决定。"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Row {
        spacing: 8
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "已注册 " + (app.toolCatalog.length || 0) + " 个工具"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }
        AppButton {
            text: "刷新"
            onClicked: app.refreshToolCatalog()
        }
    }

    Rectangle {
        width: parent.width
        height: toolsColumn.implicitHeight + 28
        radius: Theme.radiusMedium
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder

        Column {
            id: toolsColumn
            x: 16
            y: 14
            width: parent.width - 32
            spacing: 12
            Repeater {
                model: app.toolCatalog
                delegate: Item {
                    width: toolsColumn.width
                    height: Math.max(32, toolTextColumn.implicitHeight)

                    AppIcon {
                        x: 0
                        anchors.verticalCenter: parent.verticalCenter
                        name: page.sourceIcon(modelData.source)
                        size: 16
                        color: Theme.iconSecondary
                    }

                    Column {
                        id: toolTextColumn
                        x: 26
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(0, sourceBadge.x - x - 10)
                        spacing: 2
                        Text {
                            width: parent.width
                            text: modelData.name
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            height: 16
                            text: (modelData.description || "").replace(/\n/g, " ")
                            color: Theme.textTertiary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTiny
                            elide: Text.ElideRight
                            wrapMode: Text.NoWrap
                        }
                    }

                    Rectangle {
                        id: sourceBadge
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: sourceText.implicitWidth + 14
                        height: 20
                        radius: 4
                        color: modelData.source === "mcp" ? Theme.accentSoft : Theme.codeBg
                        Text {
                            id: sourceText
                            anchors.centerIn: parent
                            text: page.sourceLabel(modelData.source)
                            color: modelData.source === "mcp" ? Theme.accent : Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: 9
                        }
                    }
                }
            }
        }
    }

    Text {
        width: parent.width
        text: "子代理类型：explore / plan / troubleshoot 为只读工具集；general-purpose 继承父会话工具，包含写入与执行能力。"
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeTiny
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }
}
