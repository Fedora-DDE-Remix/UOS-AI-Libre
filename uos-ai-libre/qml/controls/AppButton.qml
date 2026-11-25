import QtQuick
import UosAiLibre

Rectangle {
    id: root
    property string text
    property string icon
    property string variant: "secondary" // primary | secondary | ghost | danger
    property int minWidth: 0
    property int horizontalPadding: 16
    signal clicked()

    readonly property bool filled: variant === "primary" || variant === "danger"
    readonly property bool hovered: hover.hovered && enabled

    implicitWidth: Math.max(minWidth, contentRow.implicitWidth + horizontalPadding * 2)
    implicitHeight: 34
    width: implicitWidth
    height: implicitHeight
    radius: Theme.radiusSmall
    opacity: enabled ? 1 : 0.5
    border.width: variant === "secondary" ? 1 : 0
    border.color: Theme.border
    color: {
        if (variant === "primary")
            return hovered ? Theme.accentHover : Theme.accent
        if (variant === "danger")
            return hovered ? Theme.dangerHover : Theme.danger
        return hovered ? Theme.hover : "transparent"
    }

    Behavior on color { ColorAnimation { duration: Theme.durationFast } }

    HoverHandler { id: hover }

    Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: root.icon.length > 0 && root.text.length > 0 ? 6 : 0

        AppIcon {
            visible: root.icon.length > 0
            name: root.icon
            size: 14
            color: root.filled ? "#FFFFFF"
                               : (root.variant === "danger" ? Theme.danger : Theme.textPrimary)
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            visible: root.text.length > 0
            text: root.text
            color: root.filled ? "#FFFFFF"
                               : (root.variant === "danger" ? Theme.danger : Theme.textPrimary)
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBase
            font.weight: root.filled ? Font.Medium : Font.Normal
            verticalAlignment: Text.AlignVCenter
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
