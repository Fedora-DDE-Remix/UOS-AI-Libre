import QtQuick
import UosAiLibre

Item {
    id: root
    property string reasoning: ""
    property bool active: false
    property int elapsedMs: 0
    property bool expanded: false
    property double startedAt: 0
    property int liveSeconds: 1
    width: parent ? parent.width : 0
    implicitHeight: header.height + (expanded ? body.height : 0)

    onActiveChanged: {
        if (active) {
            expanded = true
            startedAt = Date.now()
            liveSeconds = 1
        } else {
            expanded = false
        }
    }

    function seconds() {
        if (active)
            return Math.max(1, liveSeconds)
        return Math.max(1, Math.round(elapsedMs / 1000))
    }

    Timer {
        interval: 500
        repeat: true
        running: root.active
        onTriggered: {
            if (root.startedAt > 0)
                root.liveSeconds = Math.max(1, Math.round((Date.now() - root.startedAt) / 1000))
        }
    }

    Rectangle {
        id: header
        width: parent.width
        height: 40
        radius: Theme.radiusMedium
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder

        HoverHandler { id: headerHover }
        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusMedium
            color: headerHover.hovered ? Theme.hover : "transparent"
        }

        AppIcon {
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            name: "thinking"
            size: 15
            color: Theme.textSecondary
        }

        Text {
            x: 36
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 72
            text: root.active
                  ? "深度思考中…"
                  : (root.elapsedMs > 0
                     ? "已深度思考（用时" + root.seconds() + "秒）"
                     : "已深度思考")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBase
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Spinner {
            visible: root.active
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

    Item {
        id: body
        visible: root.expanded
        y: header.height
        width: parent.width
        height: visible ? Math.min(reasonText.implicitHeight + 20, 240) : 0

        Flickable {
            anchors.fill: parent
            anchors.leftMargin: 28
            anchors.rightMargin: 14
            contentHeight: reasonText.implicitHeight + 20
            clip: true
            interactive: contentHeight > height
            boundsBehavior: Flickable.StopAtBounds

            Text {
                id: reasonText
                y: 10
                width: parent.width
                text: root.reasoning
                color: Theme.reasonText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                lineHeight: 1.65
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
        }
    }
}
