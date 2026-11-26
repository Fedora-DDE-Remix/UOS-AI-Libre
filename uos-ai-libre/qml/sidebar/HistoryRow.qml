import QtQuick
import UosAiLibre

Rectangle {
    id: root
    property string title
    property bool selected: false
    property bool renaming: false

    signal activated()
    signal menuRequested(real sceneY)
    signal renameTextEdited(string text)
    signal renameCommitted(string text)
    signal renameCancelled()

    width: parent ? parent.width : 0
    height: 30
    radius: Theme.radiusSmall
    color: rowMouse.pressed
           ? Theme.sidebarPressed
           : (selected
              ? Theme.sidebarSelected
              : (rowHover.hovered && !renaming ? Theme.sidebarHover : "transparent"))
    Behavior on color {
        ColorAnimation {
            duration: rowMouse.pressed ? Theme.durationFast : Theme.durationNormal
            easing.type: Easing.OutCubic
        }
    }

    readonly property bool actionsVisible: rowHover.hovered || selected

    HoverHandler { id: rowHover }

    Text {
        x: 12
        width: Math.max(0, parent.width - 12
                           - (root.actionsVisible ? 42 : 14))
        anchors.verticalCenter: parent.verticalCenter
        visible: !root.renaming
        text: root.title
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBase
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    // Keep the row mouse area below the action button so the button receives
    // its own clicks instead of the row underneath.
    MouseArea {
        id: rowMouse
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        enabled: !root.renaming
        cursorShape: Qt.PointingHandCursor
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton)
                root.menuRequested(root.mapToItem(null, 0, 0).y)
            else
                root.activated()
        }
    }

    IconButton {
        id: rowActions
        visible: root.actionsVisible && !root.renaming
        z: 2
        icon: "more"
        buttonSize: 22
        iconSize: 12
        hoverBackground: Theme.sidebarHover
        pressedBackground: Theme.sidebarPressed
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.verticalCenter: parent.verticalCenter
        onClicked: root.menuRequested(root.mapToItem(null, 0, 0).y)
    }

    Rectangle {
        visible: root.renaming
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: Theme.card
        border.width: 2
        border.color: Theme.accent
        z: 3

        TextInput {
            id: renameInput
            anchors.fill: parent
            anchors.leftMargin: 9
            anchors.rightMargin: 9
            verticalAlignment: TextInput.AlignVCenter
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBase
            color: Theme.textPrimary
            selectionColor: Theme.accent
            selectedTextColor: "#FFFFFF"
            selectByMouse: true
            clip: true
            onTextEdited: root.renameTextEdited(text)
            onAccepted: root.renameCommitted(text)
            Keys.onEscapePressed: root.renameCancelled()
            onActiveFocusChanged: {
                if (!activeFocus && root.renaming)
                    root.renameCommitted(text)
            }
        }
    }

    function startEditing() {
        renameInput.text = root.title
        Qt.callLater(function() {
            renameInput.forceActiveFocus()
            renameInput.selectAll()
        })
    }

    onRenamingChanged: {
        if (renaming)
            startEditing()
    }

    Component.onCompleted: {
        if (renaming)
            startEditing()
    }
}
