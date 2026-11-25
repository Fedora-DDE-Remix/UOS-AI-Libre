import QtQuick
import UosAiLibre

Item {
    id: root
    property string icon
    property string label
    property string shortcut
    property bool checked: false
    property bool enabled: true
    property bool danger: false
    property bool showChevron: false
    property bool highlighted: false
    signal clicked()

    width: parent ? parent.width : 0
    height: 34
    opacity: enabled ? 1 : 0.45

    HoverHandler { id: hover }

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 2
        anchors.rightMargin: 2
        radius: Theme.radiusSmall
        color: root.highlighted
               ? Theme.accentSoft
               : (hover.hovered && root.enabled ? Theme.hover : "transparent")
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    }

    AppIcon {
        visible: root.icon.length > 0
        x: 10
        anchors.verticalCenter: parent.verticalCenter
        name: root.icon
        size: 15
        color: root.danger ? Theme.danger : Theme.icon
    }

    Text {
        x: root.icon.length > 0 ? 33 : 10
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - x - (root.showChevron ? 28 : (root.shortcut.length > 0 ? 56 : 12))
        text: root.label
        color: root.danger
               ? Theme.danger
               : (root.highlighted ? Theme.accent : Theme.textPrimary)
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBase
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        visible: root.shortcut.length > 0
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: root.shortcut
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeTiny
    }

    AppIcon {
        visible: root.checked && !root.showChevron
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        name: "check"
        size: 14
        color: Theme.accent
    }

    AppIcon {
        visible: root.showChevron
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        name: "chevron-right"
        size: 13
        color: Theme.iconSecondary
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
