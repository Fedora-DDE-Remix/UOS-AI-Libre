import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    width: 380
    padding: 0
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder
    }

    contentItem: Column {
        width: root.width
        topPadding: 28
        leftPadding: 28
        rightPadding: 28
        bottomPadding: 20
        spacing: 10

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 56
            height: 56
            source: "qrc:/images/logo.png"
            fillMode: Image.PreserveAspectFit
        }

        Text {
            width: parent.width - 56
            text: "UOS AI Libre"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 20
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            width: parent.width - 56
            text: "版本 " + appVersion
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            width: parent.width - 40
            text: "完全开源,完全可靠的桌面AI助手"
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBase
            lineHeight: 1.55
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            topPadding: 6
        }

        AppButton {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "关闭"
            minWidth: 90
            variant: "primary"
            onClicked: root.close()
        }
    }
}
