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

            AppButton {
                text: root.cancelText
                minWidth: 84
                onClicked: {
                    root.decisionMade = true
                    root.close()
                    root.cancelled()
                }
            }

            AppButton {
                text: root.confirmText
                minWidth: 84
                variant: root.danger ? "danger" : "primary"
                onClicked: {
                    root.decisionMade = true
                    root.close()
                    root.accepted()
                }
            }
        }
    }
}
