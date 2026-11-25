import QtQuick
import UosAiLibre

Item {
    id: root
    property string icon: "window-min"
    property bool danger: false
    property bool maximized: false
    signal clicked()

    width: 32
    height: 32

    HoverHandler {
        id: hover
        cursorShape: Qt.ArrowCursor
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        radius: Theme.radiusSmall
        color: hover.hovered
               ? (root.danger ? "#E5484D" : Theme.hover)
               : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    }

    AppIcon {
        anchors.centerIn: parent
        name: root.maximized && root.icon === "window-max"
              ? "window-restore" : root.icon
        size: 16
        color: root.danger && hover.hovered ? "#FFFFFF" : Theme.icon
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
