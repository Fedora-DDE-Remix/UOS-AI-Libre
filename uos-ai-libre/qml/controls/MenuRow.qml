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
    property bool checkLeft: false
    property bool suppressHover: false
    signal clicked()

    width: parent ? parent.width : 0
    height: 34
    opacity: enabled ? 1 : 0.45

    readonly property bool active: enabled && (highlighted || (hover.hovered && !suppressHover))
    readonly property color activeColor: Theme.menuHighlightText
    readonly property color idleColor: danger ? Theme.danger : Theme.textPrimary

    HoverHandler { id: hover }

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        anchors.topMargin: 1
        anchors.bottomMargin: 1
        radius: Theme.radiusSmall
        color: root.active ? Theme.accent : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    }

    AppIcon {
        visible: root.icon.length > 0
        x: 10
        anchors.verticalCenter: parent.verticalCenter
        name: root.icon
        size: 16
        color: root.active ? root.activeColor
                           : (root.danger ? Theme.danger : Theme.icon)
    }

    Text {
        x: root.icon.length > 0 ? 34 : (root.checkLeft ? 34 : 14)
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - x - (root.showChevron ? 30 : (root.shortcut.length > 0 ? 58 : 12))
        text: root.label
        color: root.active ? root.activeColor : root.idleColor
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBase
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        visible: root.shortcut.length > 0
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: root.shortcut
        color: root.active ? root.activeColor : Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeTiny
    }

    AppIcon {
        visible: root.checked && root.checkLeft && !root.showChevron
        x: 12
        anchors.verticalCenter: parent.verticalCenter
        name: "check"
        size: 15
        color: root.active ? root.activeColor : Theme.textPrimary
    }

    AppIcon {
        visible: root.checked && !root.checkLeft && !root.showChevron
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        name: "check"
        size: 15
        color: root.active ? root.activeColor : Theme.textPrimary
    }

    AppIcon {
        visible: root.showChevron
        anchors.right: parent.right
        anchors.rightMargin: 9
        anchors.verticalCenter: parent.verticalCenter
        name: "chevron-right"
        size: 13
        color: root.active ? root.activeColor : Theme.iconSecondary
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
