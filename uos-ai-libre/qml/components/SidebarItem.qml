import QtQuick
import UosAiLibre

Item {
    id: root
    property string icon
    property string label
    property string rightIcon
    property bool selected: false
    property bool compact: false
    signal clicked()

    width: parent ? parent.width : 0
    height: 34

    HoverHandler { id: hover }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: root.selected
               ? Theme.selected
               : (hover.hovered ? Theme.hover : "transparent")
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    }

    AppIcon {
        id: iconItem
        x: 12
        anchors.verticalCenter: parent.verticalCenter
        name: root.icon
        size: 16
        color: root.selected ? Theme.textPrimary : Theme.icon
    }

    Text {
        x: 38
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - 38 - (root.rightIcon.length > 0 ? 34 : 12)
        text: root.label
        color: root.selected ? Theme.textPrimary : Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBase
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    AppIcon {
        visible: root.rightIcon.length > 0
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        name: root.rightIcon
        size: 15
        color: Theme.iconSecondary
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
