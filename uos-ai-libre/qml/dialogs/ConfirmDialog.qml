import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root
    property string title
    property string message
    property string detail: ""
    property string confirmText: "确定"
    property string cancelText: "取消"
    property bool danger: false
    property bool decisionMade: false
    signal accepted()
    signal confirmed()
    signal cancelled()

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    width: Math.min(460, parent ? parent.width - 48 : 460)
    padding: 0
    modal: true
    focus: true
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
        topPadding: 22
        leftPadding: 22
        rightPadding: 22
        bottomPadding: 18
        spacing: 10

        Text {
            width: parent.width - 44
            text: root.title
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 16
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
        }

        Text {
            width: parent.width - 44
            text: root.message
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBase
            lineHeight: 1.5
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }

        Rectangle {
            visible: root.detail.length > 0
            width: parent.width - 44
            height: Math.min(detailText.implicitHeight + 20, 150)
            radius: Theme.radiusSmall
            color: Theme.codeBg
            border.width: 1
            border.color: Theme.border
            clip: true

            Flickable {
                anchors.fill: parent
                anchors.margins: 10
                contentHeight: detailText.implicitHeight
                contentWidth: width
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                Text {
                    id: detailText
                    width: parent.width
                    text: root.detail
                    color: Theme.textSecondary
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: Theme.fontSizeTiny
                    wrapMode: Text.WrapAnywhere
                    lineHeight: 1.4
                }
            }
        }

        Item { width: 1; height: 4 }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 22
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
                    root.confirmed()
                }
            }
        }
    }
}
