import QtQuick
import UosAiLibre

Item {
    id: root
    property alias text: input.text
    property string placeholder
    property int echoMode: TextInput.Normal
    property bool readOnly: false
    property int inputMethodHints: Qt.ImhNone
    property bool password: false
    signal accepted()
    signal textEdited(string value)

    function forceInputFocus() { input.forceActiveFocus() }
    function selectAll() { input.selectAll() }

    implicitHeight: 38
    implicitWidth: 200
    clip: true

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: Theme.isDark ? "#1E1E22" : "#F7F7F8"
        border.width: 1
        border.color: input.activeFocus ? Theme.accent : Theme.border
        Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }
    }

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 11
        anchors.rightMargin: 11
        verticalAlignment: TextInput.AlignVCenter
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBase
        selectionColor: Theme.accent
        selectedTextColor: "#FFFFFF"
        echoMode: root.password ? TextInput.Password : root.echoMode
        readOnly: root.readOnly
        inputMethodHints: root.inputMethodHints
        clip: true
        onAccepted: root.accepted()
        onTextChanged: root.textEdited(input.text)

        Text {
            anchors.fill: parent
            visible: input.text.length === 0 && !input.activeFocus
            text: root.placeholder
            color: Theme.inputPlaceholder
            font: input.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
}
