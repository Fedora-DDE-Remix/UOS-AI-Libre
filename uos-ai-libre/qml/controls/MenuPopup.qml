import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root
    property var items: []
    property int menuWidth: 190
    property bool showIcons: true
    property bool checkLeft: false
    property int highlightedIndex: -1
    signal itemClicked(int index)

    width: menuWidth
    padding: 0
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.margins: -9
            radius: Theme.radiusLarge + 9
            color: Theme.isDark ? "#40000000" : "#0D000000"
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: Theme.radiusLarge + 4
            color: Theme.isDark ? "#4D000000" : "#16000000"
        }
        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusLarge
            color: Theme.menuBackground
            border.width: 1
            border.color: Theme.menuBorder
        }
    }

    contentItem: Column {
        id: menuColumn
        width: root.width
        topPadding: 6
        bottomPadding: 6
        spacing: 0

        Repeater {
            model: root.items
            delegate: Item {
                width: menuColumn.width
                height: modelData && modelData.separator ? 9 : 34

                Rectangle {
                    visible: modelData && modelData.separator === true
                    anchors.centerIn: parent
                    width: parent.width - 24
                    height: 1
                    color: Theme.menuBorder
                }

                MenuRow {
                    visible: !(modelData && modelData.separator === true)
                    width: parent.width
                    icon: root.showIcons && modelData && modelData.icon ? modelData.icon : ""
                    label: modelData && modelData.label ? modelData.label : ""
                    shortcut: modelData && modelData.shortcut ? modelData.shortcut : ""
                    checked: modelData && modelData.checked === true
                    enabled: !(modelData && modelData.enabled === false)
                    danger: modelData && modelData.danger === true
                    showChevron: modelData && modelData.submenu === true
                    checkLeft: root.checkLeft
                    highlighted: root.highlightedIndex === index
                    suppressHover: root.highlightedIndex >= 0 && root.highlightedIndex !== index
                    onClicked: root.itemClicked(index)
                }
            }
        }
    }

    onClosed: highlightedIndex = -1

    Connections {
        target: Window.window
        function onActiveChanged() {
            if (Window.window && !Window.window.active)
                root.close()
        }
    }
}
