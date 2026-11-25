import QtQuick
import UosAiLibre

Rectangle {
    id: root
    property string title
    property string description
    property string iconName
    signal clicked()

    implicitHeight: 62
    radius: Theme.radiusMedium
    color: Theme.card
    border.width: 1
    border.color: hover.hovered ? Theme.borderStrong : Theme.cardBorder
    Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }

    HoverHandler { id: hover }

    AppIcon {
        x: 14
        y: 13
        name: root.iconName
        size: 16
        color: hover.hovered ? Theme.accent : Theme.iconSecondary
    }

    Text {
        x: 38
        y: 11
        width: parent.width - 50
        text: root.title
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBase
        font.weight: Font.Medium
        elide: Text.ElideRight
    }

    Text {
        x: 38
        y: 32
        width: parent.width - 50
        text: root.description
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeTiny
        elide: Text.ElideRight
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
