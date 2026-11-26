import QtQuick
import UosAiLibre

Item {
    id: root
    property bool checked: false
    signal toggled(bool checked)

    implicitWidth: 38
    implicitHeight: 22
    width: implicitWidth
    height: implicitHeight
    opacity: enabled ? 1 : 0.45

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? Theme.accent : Theme.borderStrong
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    }

    Rectangle {
        width: 18
        height: 18
        radius: 9
        y: 2
        x: root.checked ? parent.width - width - 2 : 2
        color: "#FFFFFF"
        Behavior on x { NumberAnimation { duration: Theme.durationFast; easing.type: Easing.OutCubic } }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.checked = !root.checked
            root.toggled(root.checked)
        }
    }
}
