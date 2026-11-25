import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root
    property string title
    property string message
    property string confirmText: "确定"
    property string cancelText: "取消"
    property bool danger: false
    property bool decisionMade: false
    signal accepted()
    signal cancelled()

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    width: 380
    padding: 0
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onClosed: {
        if (!root.decisionMade)
            root.cancelled()
        root.decisionMade = false
    }

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder
    }

    contentItem: Column {
        width: root.width
        topPadding: 24
        leftPadding: 24
        rightPadding: 24
        bottomPadding: 18
        spacing: 10

        Text {
            width: parent.width - 48
            text: root.title
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 16
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
        }

        Text {
            width: parent.width - 48
            text: root.message
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBase
            lineHeight: 1.5
            wrapMode: Text.Wrap
        }

        Item { width: 1; height: 8 }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 24
            spacing: 10

            Rectangle {
                width: 84
                height: 34
                radius: Theme.radiusSmall
                color: cancelHover.hovered ? Theme.hover : "transparent"
                border.width: 1
                border.color: Theme.border
                Text {
                    anchors.centerIn: parent
                    text: root.cancelText
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBase
                }
                HoverHandler { id: cancelHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.decisionMade = true
                        root.close()
                        root.cancelled()
                    }
                }
            }

            Rectangle {
                width: 84
                height: 34
                radius: Theme.radiusSmall
                color: root.danger
                       ? (confirmHover.hovered ? "#F06A6E" : Theme.danger)
                       : (confirmHover.hovered ? Theme.accentHover : Theme.accent)
                Text {
                    anchors.centerIn: parent
                    text: root.confirmText
                    color: "#FFFFFF"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBase
                    font.weight: Font.Medium
                }
                HoverHandler { id: confirmHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.decisionMade = true
                        root.close()
                        root.accepted()
                    }
                }
            }
        }
    }
}
