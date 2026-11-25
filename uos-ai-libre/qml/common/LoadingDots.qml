import QtQuick
import UosAiLibre

Item {
    id: root
    property int count: 3
    property color color: Theme.textTertiary
    width: 22
    height: 12

    Row {
        anchors.centerIn: parent
        spacing: 4
        Repeater {
            model: root.count
            delegate: Rectangle {
                id: dot
                width: 4
                height: 4
                radius: 2
                color: root.color
                opacity: 0.35
                SequentialAnimation {
                    running: true
                    loops: Animation.Infinite
                    PauseAnimation { duration: index * 130 }
                    NumberAnimation { target: dot; property: "opacity"; to: 1; duration: 280; easing.type: Easing.InOutSine }
                    NumberAnimation { target: dot; property: "opacity"; to: 0.35; duration: 280; easing.type: Easing.InOutSine }
                    PauseAnimation { duration: (root.count - index - 1) * 130 }
                }
            }
        }
    }
}
