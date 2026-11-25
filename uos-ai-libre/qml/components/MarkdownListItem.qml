import QtQuick
import UosAiLibre

Item {
    id: root
    property string blockText
    property string marker
    property int indent: 0
    property int checked: -1
    width: parent ? parent.width : 0
    implicitHeight: textItem.implicitHeight + 4

    readonly property int baseX: Math.max(0, Math.min(root.width - 120, root.indent * 20))

    Rectangle {
        id: bullet
        visible: root.marker.length === 0 && root.checked < 0
        x: root.baseX + 6
        y: textItem.y + 13
        width: 4
        height: 4
        radius: 2
        color: Theme.textTertiary
    }

    Text {
        id: markerText
        visible: root.marker.length > 0
        x: root.baseX
        y: textItem.y + 3
        width: 24
        horizontalAlignment: Text.AlignRight
        text: root.marker
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeChat
        lineHeight: 1.68
    }

    Rectangle {
        id: checkbox
        visible: root.checked >= 0
        x: root.baseX + 2
        y: textItem.y + 5
        width: 14
        height: 14
        radius: 3
        color: root.checked === 1 ? Theme.accent : "transparent"
        border.width: 1
        border.color: root.checked === 1 ? Theme.accent : Theme.borderStrong
        AppIcon {
            anchors.centerIn: parent
            visible: root.checked === 1
            name: "check"
            size: 10
            color: "#FFFFFF"
        }
    }

    Text {
        id: textItem
        x: root.baseX + (root.marker.length > 0 ? 32 : (root.checked >= 0 ? 26 : 18))
        width: root.width - x
        text: root.blockText
        textFormat: Text.MarkdownText
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeChat
        lineHeight: 1.66
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        topPadding: 3
        bottomPadding: 3
        linkColor: Theme.accent
        onLinkActivated: (link) => app.openExternal(link)
    }
}
