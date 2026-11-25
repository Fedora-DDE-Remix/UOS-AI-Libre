import QtQuick
import UosAiLibre

Item {
    id: root
    property string iconName: "info"
    property string title
    property string description
    property string bullet1
    property string bullet2

    Column {
        x: Math.max(24, (parent.width - width) / 2)
        y: 10
        width: Math.min(680, parent.width - 128)
        spacing: 18

        Row {
            spacing: 12
            Rectangle {
                width: 42
                height: 42
                radius: 21
                color: Theme.accentSoft
                AppIcon {
                    anchors.centerIn: parent
                    name: root.iconName
                    size: 20
                    color: Theme.accent
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Text {
                    text: root.title
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 19
                    font.weight: Font.DemiBold
                }
                Text {
                    width: 520
                    text: root.description
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBase
                    wrapMode: Text.Wrap
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 150
            radius: Theme.radiusMedium
            color: Theme.card
            border.width: 1
            border.color: Theme.cardBorder

            Column {
                x: 20
                y: 18
                width: parent.width - 40
                spacing: 12

                Row {
                    width: parent.width
                    spacing: 8
                    Rectangle {
                        width: 18
                        height: 18
                        radius: 9
                        color: Theme.accentSoft
                        Text {
                            anchors.centerIn: parent
                            text: "•"
                            color: Theme.accent
                            font.pixelSize: 11
                        }
                    }
                    Text {
                        width: parent.width - 30
                        text: root.bullet1
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBase
                        wrapMode: Text.Wrap
                    }
                }

                Row {
                    width: parent.width
                    spacing: 8
                    Rectangle {
                        width: 18
                        height: 18
                        radius: 9
                        color: Theme.accentSoft
                        Text {
                            anchors.centerIn: parent
                            text: "•"
                            color: Theme.accent
                            font.pixelSize: 11
                        }
                    }
                    Text {
                        width: parent.width - 30
                        text: root.bullet2
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBase
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        Text {
            text: "当前版本聚焦本地对话、工具调用与共享 aish 配置。"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }
    }
}
