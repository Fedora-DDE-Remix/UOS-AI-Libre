import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root

    property var providers: []

    signal builtinSelected(var preset)
    signal customSelected()

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    width: Math.min(560, parent ? parent.width - 40 : 560)
    height: Math.min(560, parent ? parent.height - 24 : 560)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: providers = app.builtinProviders() || []

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder
    }

    contentItem: Column {
        width: root.width
        height: root.height
        spacing: 0

        Item {
            width: parent.width
            height: 56
            Text {
                x: 22
                anchors.verticalCenter: parent.verticalCenter
                text: "添加供应商"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
            AppIcon {
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                name: "x"
                size: 14
                color: Theme.icon
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -8
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
            }
        }

        Rectangle { width: parent.width; height: 1; color: Theme.border }

        Item {
            width: parent.width
            height: 58
            Text {
                x: 22
                y: 14
                width: parent.width - 44
                text: "aish 内置供应商只需填写 API Key；自定义 OpenAI 兼容端点在下方单独配置。"
                color: Theme.textTertiary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTiny
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
        }

        Flickable {
            width: parent.width
            height: parent.height - 56 - 1 - 58 - 64
            contentWidth: width
            contentHeight: providerColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: providerColumn
                width: parent.width
                spacing: 2

                Repeater {
                    model: root.providers
                    delegate: Rectangle {
                        width: providerColumn.width
                        height: 46
                        color: providerHover.hovered ? Theme.hover : "transparent"
                        HoverHandler { id: providerHover }

                        Text {
                            x: 22
                            y: 8
                            width: parent.width - 44
                            text: modelData.label || modelData.key || "供应商"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeBase
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
                        Text {
                            x: 22
                            y: 26
                            width: parent.width - 44
                            text: modelData.api_base || ""
                            color: Theme.textTertiary
                            font.family: "DejaVu Sans Mono"
                            font.pixelSize: 10
                            elide: Text.ElideMiddle
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.builtinSelected(modelData)
                        }
                    }
                }
            }
        }

        Rectangle { width: parent.width; height: 1; color: Theme.border }

        Item {
            width: parent.width
            height: 64
            Rectangle {
                anchors.fill: parent
                anchors.margins: 10
                radius: Theme.radiusSmall
                color: customHover.hovered ? Theme.hover : "transparent"
                border.width: 1
                border.color: Theme.border
                HoverHandler { id: customHover }
                Text {
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "OpenAI-Compatible（自定义地址 / 模型目录）"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.close()
                        root.customSelected()
                    }
                }
            }
        }
    }
}
