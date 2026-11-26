import QtQuick
import UosAiLibre

Item {
    id: root
    property var options: []
    property string value
    signal changed(string value)

    implicitWidth: row.implicitWidth
    implicitHeight: 30
    width: implicitWidth
    height: implicitHeight

    Row {
        id: row
        spacing: 4
        Repeater {
            model: root.options
            delegate: Rectangle {
                width: label.implicitWidth + 20
                height: 26
                radius: Theme.radiusSmall
                color: root.value === modelData.value
                       ? Theme.accentSoft
                       : (choiceHover.hovered ? Theme.hover : "transparent")
                border.width: 1
                border.color: root.value === modelData.value
                              ? Theme.accent
                              : Theme.border
                HoverHandler { id: choiceHover }
                Text {
                    id: label
                    anchors.centerIn: parent
                    text: modelData.label
                    color: root.value === modelData.value
                           ? Theme.accent : Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.value !== modelData.value) {
                            root.value = modelData.value
                            root.changed(modelData.value)
                        }
                    }
                }
            }
        }
    }
}
