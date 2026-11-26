import QtQuick
import UosAiLibre

Item {
    id: root
    property var tool: ({})
    property bool expanded: false
    readonly property string kind: tool && tool.kind ? tool.kind : "tool"
    width: parent ? parent.width : 0
    implicitHeight: row.height + (expanded ? detail.height : 0)

    function statusColor() {
        if (!tool)
            return Theme.textTertiary
        if (tool.status === "failed")
            return Theme.danger
        if (tool.status === "done")
            return Theme.success
        return Theme.accent
    }

    function kindIcon() {
        if (kind === "bash")
            return "tool"
        if (kind === "file")
            return "file"
        if (kind === "web")
            return "globe"
        if (kind === "agent")
            return "thinking"
        if (kind === "mcp")
            return "connector"
        if (kind === "skill")
            return "skill"
        if (kind === "memory")
            return "knowledge"
        if (kind === "plan")
            return "report"
        return "tool"
    }

    function toolLabel() {
        if (!tool)
            return ""
        let label = tool.name || ""
        if (tool.agentType && tool.agentType.length > 0)
            label += " · " + tool.agentType
        return label
    }

    function cleanResult(value) {
        let text = value ? String(value) : ""
        if (kind === "bash") {
            text = text.replace(/<\/?stdout>/g, "")
                       .replace(/<\/?stderr>/g, "")
                       .replace(/<\/?output>/g, "")
                       .replace(/<return ?code>[\s\S]*?<\/return ?code>/g, "")
        }
        return text.trim()
    }

    Item {
        id: row
        width: parent.width
        height: 34

        HoverHandler { id: rowHover }
        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusSmall
            color: rowHover.hovered ? Theme.hover : "transparent"
        }

        Spinner {
            visible: tool && tool.status === "running"
            x: 10
            anchors.verticalCenter: parent.verticalCenter
            width: 13
            height: 13
            color: Theme.accent
        }

        AppIcon {
            visible: !tool || tool.status !== "running"
            x: 10
            anchors.verticalCenter: parent.verticalCenter
            name: root.kindIcon()
            size: 13
            color: root.statusColor()
        }

        Text {
            x: 31
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(170, implicitWidth)
            text: root.toolLabel()
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            font.weight: Font.Medium
            elide: Text.ElideRight
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Math.min(170, Math.max(70, toolNameWidth())) + 38
            anchors.right: durationText.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: tool ? tool.target : ""
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideRight
        }

        Text {
            id: durationText
            visible: tool && tool.durationMs > 0 && tool.status !== "running"
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: tool ? (Math.max(0.1, tool.durationMs / 1000).toFixed(1) + "s") : ""
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeTiny
        }

        AppIcon {
            anchors.right: parent.right
            anchors.rightMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            visible: !(tool && tool.durationMs > 0 && tool.status !== "running")
            name: root.expanded ? "chevron-down" : "chevron-right"
            size: 10
            color: Theme.iconSecondary
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.expanded = !root.expanded
        }
    }

    function toolNameWidth() {
        if (!tool || !tool.name)
            return 70
        return Math.min(170, (tool.name.length + (tool.agentType ? tool.agentType.length + 3 : 0)) * 7 + 8)
    }

    Rectangle {
        id: detail
        visible: root.expanded
        y: row.height
        width: parent.width
        height: visible ? Math.min(detailColumn.implicitHeight + 18, 300) : 0
        clip: true
        radius: Theme.radiusSmall
        color: Theme.codeBg
        border.width: 1
        border.color: Theme.border

        Flickable {
            x: 12
            y: 8
            width: parent.width - 24
            height: parent.height - 16
            contentHeight: detailColumn.implicitHeight
            contentWidth: width
            clip: true
            interactive: contentHeight > height
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: detailColumn
                width: parent.width
                spacing: 6

            Text {
                width: parent.width
                visible: root.tool && root.tool.args && root.tool.args.length > 0
                text: root.kind === "bash"
                      ? "命令  " + (root.tool ? root.tool.target : "")
                      : "参数  " + (root.tool ? root.tool.args : "")
                color: Theme.textSecondary
                font.family: "DejaVu Sans Mono"
                font.pixelSize: 11
                wrapMode: Text.WrapAnywhere
                lineHeight: 1.45
            }

            Text {
                width: parent.width
                visible: root.tool && root.tool.result && root.tool.result.length > 0
                text: "结果  " + root.cleanResult(root.tool ? root.tool.result : "")
                color: Theme.textSecondary
                font.family: "DejaVu Sans Mono"
                font.pixelSize: 11
                wrapMode: Text.WrapAnywhere
                lineHeight: 1.45
            }

            Text {
                width: parent.width
                visible: root.tool && root.tool.offloadPath
                         && root.tool.offloadPath.length > 0
                text: "完整输出已保存，点击打开"
                color: Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: 10
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: app.openPath(root.tool.offloadPath)
                }
            }
            }
        }
    }
}
