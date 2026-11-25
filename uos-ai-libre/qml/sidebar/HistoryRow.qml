import QtQuick
import UosAiLibre

Rectangle {
    id: root
    property string title
    property bool selected: false
    property bool renaming: false

    signal activated()
    signal menuRequested(real sceneY)
    signal renameRequested()
    signal renameCommitted(string text)
    signal renameCancelled()

    width: parent ? parent.width : 0
    height: 30
    radius: Theme.radiusSmall
    color: selected
           ? Theme.selected
           : (rowHover.hovered && !renaming ? Theme.hover : "transparent")
    Behavior on color { ColorAnimation { duration: Theme.durationFast } }

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
        onDoubleClicked: root.renameRequested()
    }

    Item {
        id: rowActions
        visible: root.actionsVisible && !root.renaming
        z: 2
        width: 22
        height: 22
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.verticalCenter: parent.verticalCenter

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: moreHover.hovered ? Theme.hover : "transparent"
            border.width: 1
            border.color: Theme.borderStrong
        }

        AppIcon {
            anchors.centerIn: parent
            name: "more"
            size: 12
            color: Theme.icon
        }

        HoverHandler { id: moreHover }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.menuRequested(root.mapToItem(null, 0, 0).y)
        }
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
