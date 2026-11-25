import QtQuick
import UosAiLibre

Rectangle {
    id: root
    property int leftRadius: 0
    property int rightRadius: Theme.windowRadius
    property string pageTitle
    property string pageSubtitle
    property string iconName
    property string emptyTitle
    property string emptyDescription
    property string actionText
    signal actionRequested()

    color: Theme.windowBg
    topLeftRadius: leftRadius
    bottomLeftRadius: leftRadius
    topRightRadius: rightRadius
    bottomRightRadius: rightRadius

    Column {
        anchors.centerIn: parent
        width: Math.min(560, parent.width - 80)
        spacing: 18

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 72
            height: 72
            radius: 36
            color: Theme.accentSoft
            AppIcon {
                anchors.centerIn: parent
                name: root.iconName
                size: 30
                color: Theme.accent
            }
        }

        Text {
            width: parent.width
            text: root.pageTitle
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 26
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            width: parent.width
            text: root.pageSubtitle
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Rectangle {
            width: parent.width
            height: 148
            radius: Theme.radiusLarge
            color: Theme.card
            border.width: 1
            border.color: Theme.cardBorder

            Column {
                anchors.centerIn: parent
                width: parent.width - 48
                spacing: 8
                Text {
                    width: parent.width
                    text: root.emptyTitle
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                }
                Text {
                    width: parent.width
                    text: root.emptyDescription
                    color: Theme.textTertiary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
                Item { width: 1; height: 4 }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 112
                    height: 34
                    radius: Theme.radiusSmall
                    color: actionHover.hovered ? Theme.accentHover : Theme.accent
                    Text {
                        anchors.centerIn: parent
                        text: root.actionText
                        color: "#FFFFFF"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBase
                        font.weight: Font.Medium
                    }
                    HoverHandler { id: actionHover }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.actionRequested()
                    }
                }
            }
        }
    }
}
