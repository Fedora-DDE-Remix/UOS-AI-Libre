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
        else
            expanded = false
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
                    let text = "执行过程"
                    if (root.running)
                        text += " · 执行中"
                    else if (root.elapsedMs > 0)
                        text += " 已处理" + root.seconds() + "秒"
                    else if (root.tools.length > 0)
                        text += " · " + root.tools.length + " 个步骤"
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
