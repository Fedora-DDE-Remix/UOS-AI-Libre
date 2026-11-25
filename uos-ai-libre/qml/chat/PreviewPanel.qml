import QtQuick
import UosAiLibre

Rectangle {
    id: root
    signal closeRequested()
    signal toastRequested(string message)
    property int rightRadius: Theme.windowRadius

    color: Theme.card
    border.width: 1
    border.color: Theme.border
    topRightRadius: rightRadius
    bottomRightRadius: rightRadius

    Rectangle {
        width: 1
        height: parent.height
        color: Theme.border
    }

    Item {
        id: header
        width: parent.width
        height: 52

        Item {
            x: 12
            width: 30
            height: 30
            anchors.verticalCenter: parent.verticalCenter
            HoverHandler { id: plusHover }
            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusSmall
                color: plusHover.hovered ? Theme.hover : "transparent"
            }
            AppIcon {
                anchors.centerIn: parent
                name: "plus"
                size: 16
                color: Theme.icon
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toastRequested("预览面板暂未接入新建标签页")
            }
        }

        Item {
            width: 30
            height: 30
            anchors.right: closeButton.left
            anchors.rightMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            HoverHandler { id: expandHover }
            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusSmall
                color: expandHover.hovered ? Theme.hover : "transparent"
            }
            AppIcon {
                anchors.centerIn: parent
                name: "expand"
                size: 14
                color: Theme.iconSecondary
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toastRequested("预览面板已在当前窗口展开")
            }
        }

        Item {
            id: closeButton
            width: 30
            height: 30
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            HoverHandler { id: closeHover }
            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusSmall
                color: closeHover.hovered ? Theme.hover : "transparent"
            }
            AppIcon {
                anchors.centerIn: parent
                name: "panel-right"
                size: 15
                color: Theme.icon
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.closeRequested()
            }
        }
    }

    Column {
        id: content
        y: 76
        x: 20
        width: parent.width - 40
        spacing: 18

        Rectangle {
            width: parent.width
            height: 54
            radius: Theme.radiusMedium
            color: Theme.codeBg
            border.width: 1
            border.color: Theme.border

            AppIcon {
                x: 14
                anchors.verticalCenter: parent.verticalCenter
                name: "browser-open"
                size: 17
                color: Theme.accent
            }
            Column {
                x: 42
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text {
                    text: "浏览器打开"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBase
                }
                Text {
                    text: "浏览网页"
                    color: Theme.textTertiary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTiny
                }
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toastRequested("内置浏览器面板未启用（本项目不依赖 WebEngine）")
            }
        }

        Item {
            width: parent.width
            height: 54

            AppIcon {
                x: 8
                anchors.verticalCenter: parent.verticalCenter
                name: "folder"
                size: 32
                color: Theme.accent
            }
            Column {
                x: 50
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text {
                    text: "工作区文件"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBase
                }
                Text {
                    text: "浏览会话工作区的文件"
                    color: Theme.textTertiary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTiny
                }
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: app.openWorkspace()
            }
        }
    }
}
