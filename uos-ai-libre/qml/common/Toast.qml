import QtQuick
import UosAiLibre

Item {
    id: root
    property string text
    property bool shown: false
    implicitWidth: Math.min(520, label.implicitWidth + 52)
    implicitHeight: 38
    opacity: shown ? 1 : 0
    visible: opacity > 0

    function show(message) {
        root.text = message
        root.shown = true
        hideTimer.restart()
    }

    Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusMedium
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder
    }

    AppIcon {
        x: 14
        anchors.verticalCenter: parent.verticalCenter
        name: "info"
        size: 15
        color: Theme.accent
    }

    Text {
        id: label
        anchors.left: parent.left
        anchors.leftMargin: 37
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        text: root.text
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBase
        elide: Text.ElideRight
    }

    Timer {
        id: hideTimer
        interval: 2600
        onTriggered: root.shown = false
    }
}
