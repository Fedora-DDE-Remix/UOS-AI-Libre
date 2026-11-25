import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root
    property var items: []
    property int menuWidth: 190
    signal itemClicked(int index)

    width: menuWidth
    padding: 0
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        radius: Theme.radiusMedium
        color: Theme.card
        border.width: 1
        border.color: Theme.border
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
                    width: parent.width - 16
                    height: 1
                    color: Theme.border
                }

                MenuRow {
                    visible: !(modelData && modelData.separator === true)
                    width: parent.width
                    icon: modelData && modelData.icon ? modelData.icon : ""
                    label: modelData && modelData.label ? modelData.label : ""
                    shortcut: modelData && modelData.shortcut ? modelData.shortcut : ""
                    checked: modelData && modelData.checked === true
                    enabled: !(modelData && modelData.enabled === false)
                    danger: modelData && modelData.danger === true
                    showChevron: modelData && modelData.submenu === true
                    onClicked: root.itemClicked(index)
                }
            }
        }
    }
}
