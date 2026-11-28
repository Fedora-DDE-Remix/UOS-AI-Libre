import QtQuick
import UosAiLibre

Item {
    id: root
    property var tools: []
    property bool active: false
    property int elapsedMs: 0
    property bool expanded: false
    width: parent ? parent.width : 0
    implicitHeight: card.height

    readonly property bool running: {
        for (let i = 0; i < tools.length; ++i) {
            if (tools[i].status === "running")
                return true
        }
        return false
    }

    onRunningChanged: {
        if (running)
            expanded = true
        else if (tools.length <= 1)
            expanded = false
    }

    onToolsChanged: {
        // Keep a multi-call step open so every sequentially executed tool is
        // visible, not just the first summary row.
        if (tools.length > 1)
            expanded = true
    }

    Component.onCompleted: {
        if (running || tools.length > 1)
            expanded = true
    }

    function summaryText() {
        if (!tools || tools.length === 0)
            return "执行过程"
        if (tools.length === 1) {
            const tool = tools[0]
            let label = tool && tool.name ? String(tool.name) : "工具"
            if (tool && tool.agentType)
                label += " · " + tool.agentType
            if (tool && tool.target)
                label += " · " + tool.target
            return label
        }
        const names = []
        for (let i = 0; i < Math.min(2, tools.length); ++i)
            names.push(tools[i] && tools[i].name ? String(tools[i].name) : "工具")
        return names.join("、") + (tools.length > 2
                                   ? " 等 " + tools.length + " 个工具"
                                   : "")
    }

    function seconds() {
        return Math.max(1, Math.round(elapsedMs / 1000))
    }

    Rectangle {
        id: card
        width: parent.width
        height: header.height + (root.expanded ? body.height : 0)
        radius: Theme.radiusMedium
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder

        Item {
            id: header
            width: parent.width
            height: 40

            HoverHandler { id: headerHover }

            AppIcon {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                name: "tool"
                size: 15
                color: Theme.textSecondary
            }

            Text {
                x: 36
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 72
                text: {
                    let text = root.summaryText()
                    if (root.running)
                        text += " · 执行中"
                    return text
                }
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBase
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            Spinner {
                visible: root.running
                anchors.right: parent.right
                anchors.rightMargin: 34
                anchors.verticalCenter: parent.verticalCenter
                width: 13
                height: 13
                color: Theme.textTertiary
            }

            AppIcon {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                name: root.expanded ? "chevron-down" : "chevron-right"
                size: 12
                color: Theme.iconSecondary
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.expanded = !root.expanded
            }
        }

        Column {
            id: body
            visible: root.expanded
            y: header.height
            width: parent.width
            height: visible ? implicitHeight + 8 : 0
            spacing: 0

            Repeater {
                model: root.tools
                delegate: ToolRow {
                    width: body.width - 12
                    x: 6
                    tool: modelData
                }
            }
        }
    }
}
