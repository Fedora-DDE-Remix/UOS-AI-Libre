import QtQuick
import UosAiLibre

Item {
    id: root
    property string icon
    property int buttonSize: 22
    property int iconSize: 12
    property bool bordered: true
    property int buttonRadius: Theme.radiusSmall
    property color iconColor: Theme.icon
    property color hoverBackground: Theme.hover
    property color pressedBackground: Theme.sidebarPressed
    signal clicked()

    implicitWidth: buttonSize
    implicitHeight: buttonSize
    width: buttonSize
    height: buttonSize
    opacity: enabled ? 1 : 0.5

    Rectangle {
        anchors.fill: parent
        radius: root.buttonRadius
        color: !root.enabled
               ? "transparent"
               : (mouseArea.pressed ? root.pressedBackground
                                    : (hover.hovered ? root.hoverBackground : "transparent"))
        border.width: root.bordered ? 1 : 0
        border.color: Theme.borderStrong
    }

    AppIcon {
        anchors.centerIn: parent
        name: root.icon
        size: root.iconSize
        color: root.iconColor
    }

    HoverHandler { id: hover }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
