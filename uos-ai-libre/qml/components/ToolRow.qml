import QtQuick
import UosAiLibre

Item {
    id: root
    property var tool: ({})
    property bool expanded: false
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
            name: tool && tool.status === "failed" ? "warning" : "check"
            size: 13
            color: root.statusColor()
        }

        Text {
            x: 31
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(130, implicitWidth)
            text: tool ? tool.name : ""
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            font.weight: Font.Medium
            elide: Text.ElideRight
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Math.min(130, Math.max(70, toolNameWidth())) + 38
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
        return tool && tool.name ? Math.min(130, tool.name.length * 7 + 8) : 70
    }

    Rectangle {
        id: detail
        visible: root.expanded
        y: row.height
        width: parent.width
        height: visible ? detailColumn.implicitHeight + 18 : 0
        radius: Theme.radiusSmall
        color: Theme.codeBg
        border.width: 1
        border.color: Theme.border

        Column {
            id: detailColumn
            x: 12
            y: 8
            width: parent.width - 24
            spacing: 6

            Text {
                width: parent.width
                visible: root.tool && root.tool.args && root.tool.args.length > 0
                text: "参数  " + (root.tool ? root.tool.args : "")
                color: Theme.textSecondary
                font.family: "DejaVu Sans Mono"
                font.pixelSize: 11
                wrapMode: Text.WrapAnywhere
                lineHeight: 1.45
            }

            Text {
                width: parent.width
                visible: root.tool && root.tool.result && root.tool.result.length > 0
                text: "结果  " + (root.tool ? root.tool.result : "")
                color: Theme.textSecondary
                font.family: "DejaVu Sans Mono"
                font.pixelSize: 11
                wrapMode: Text.WrapAnywhere
                lineHeight: 1.45
            }
        }
    }
}
