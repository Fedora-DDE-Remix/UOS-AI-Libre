import QtQuick
import QtQuick.Controls
import UosAiLibre

Item {
    id: root
    property alias text: area.text
    property string placeholder
    property int minHeight: 76
    signal textEdited()

    implicitHeight: minHeight
    implicitWidth: 220

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: Theme.isDark ? "#1E1E22" : "#F7F7F8"
        border.width: 1
        border.color: area.activeFocus ? Theme.accent : Theme.border
        Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 1
        contentWidth: width
        contentHeight: area.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        TextArea {
            id: area
            width: parent.width
            padding: 10
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBase
            placeholderText: root.placeholder
            placeholderTextColor: Theme.inputPlaceholder
            selectByMouse: true
            wrapMode: TextArea.WrapAtWordBoundaryOrAnywhere
            background: null
            onTextChanged: root.textEdited()
        }
    }
}
