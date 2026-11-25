import QtQuick
import UosAiLibre

Item {
    id: root
    property string blockText
    width: parent ? parent.width : 0
    implicitHeight: mathBox.height + 24

    Rectangle {
        id: mathBox
        y: 12
        width: parent.width
        height: mathText.implicitHeight + 24
        radius: Theme.radiusMedium
        color: Theme.codeBg
        border.width: 1
        border.color: Theme.border

        Text {
            id: mathText
            x: 14
            y: 12
            width: parent.width - 28
            text: root.blockText
            color: Theme.textPrimary
            font.family: "DejaVu Serif"
            font.pixelSize: 15
            lineHeight: 1.55
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }
}
