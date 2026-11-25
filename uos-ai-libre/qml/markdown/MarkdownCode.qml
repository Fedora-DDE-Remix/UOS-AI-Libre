import QtQuick
import UosAiLibre

Item {
    id: root
    property string blockText
    property string lang
    width: parent ? parent.width : 0
    implicitHeight: container.height + 24

    function copyCode() {
        app.copyText(root.blockText)
    }

    Rectangle {
        id: container
        y: 12
        width: parent.width
        height: header.height + body.height
        radius: Theme.radiusMedium
        color: Theme.codeBg
        border.width: 1
        border.color: Theme.border

        Rectangle {
            id: header
            width: parent.width
            height: 32
            color: Theme.codeHeader
            topLeftRadius: Theme.radiusMedium
            topRightRadius: Theme.radiusMedium

            Text {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                text: root.lang.length > 0 ? root.lang.toUpperCase() : "CODE"
                color: Theme.textSecondary
                font.family: "DejaVu Sans Mono"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                font.letterSpacing: 0.6
            }

            Item {
                width: 28
                height: 26
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                HoverHandler { id: copyHover }
                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radiusSmall
                    color: copyHover.hovered ? Theme.hover : "transparent"
                }
                AppIcon {
                    anchors.centerIn: parent
                    name: "copy"
                    size: 14
                    color: Theme.iconSecondary
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.copyCode()
                }
            }
        }

        Rectangle {
            id: body
            y: header.height
            width: parent.width
            height: codeText.implicitHeight + 28
            color: "transparent"
            bottomLeftRadius: Theme.radiusMedium
            bottomRightRadius: Theme.radiusMedium

            Text {
                id: codeText
                x: 14
                y: 14
                width: parent.width - 28
                text: root.blockText.length > 0 ? root.blockText : " "
                textFormat: Text.PlainText
                color: Theme.textPrimary
                font.family: "DejaVu Sans Mono"
                font.pixelSize: 12
                lineHeight: 1.55
                wrapMode: Text.WrapAnywhere
            }
        }
    }
}
