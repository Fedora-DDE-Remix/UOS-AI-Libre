import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root
    property string initialText
    property bool decisionMade: false
    signal accepted(string text)

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    width: 360
    padding: 0
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openWith(text) {
        initialText = text
        input.text = text
        decisionMade = false
        open()
        input.forceInputFocus()
        input.selectAll()
    }

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder
    }

    contentItem: Column {
        width: root.width
        topPadding: 22
        leftPadding: 24
        rightPadding: 24
        bottomPadding: 18
        spacing: 14

        Text {
            text: "重命名对话"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        AppTextField {
            id: input
            width: parent.width - 48
            placeholder: "输入对话名称"
            onAccepted: acceptButton.accept()
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 24
            spacing: 10

            Rectangle {
                width: 76
                height: 34
                radius: Theme.radiusSmall
                border.width: 1
                border.color: Theme.border
                color: cancelHover.hovered ? Theme.hover : "transparent"
                Text {
                    anchors.centerIn: parent
                    text: "取消"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBase
                }
                HoverHandler { id: cancelHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
            }

            Rectangle {
                id: acceptButton
                width: 76
                height: 34
                radius: Theme.radiusSmall
                color: acceptHover.hovered ? Theme.accentHover : Theme.accent
                function accept() {
                    const value = input.text.trim()
                    if (value.length === 0)
                        return
                    root.decisionMade = true
                    root.close()
                    root.accepted(value)
                }
                Text {
                    anchors.centerIn: parent
                    text: "保存"
                    color: "#FFFFFF"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBase
                    font.weight: Font.Medium
                }
                HoverHandler { id: acceptHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: acceptButton.accept()
                }
            }
        }
    }
}
