import QtQuick
import UosAiLibre

Item {
    id: root
    property string text: ""
    property var attachments: []
    property bool canUndo: false
    property int sourceIndex: -1
    width: parent ? parent.width : 0
    implicitHeight: bubble.height + 8

    signal undoRequested(int sourceIndex, string text)

    readonly property real maxBubbleWidth: Math.max(240, Math.min(680, width * 0.76))
    readonly property real maxTextWidth: maxBubbleWidth - 32
    readonly property real attachmentWidth: attachments.length > 0
        ? Math.min(maxBubbleWidth,
                   attachments.length * 118 + (attachments.length - 1) * 6 + 32)
        : 0
    readonly property real desiredWidth: Math.max(72, Math.min(maxBubbleWidth,
                                                               textMetrics.width + 44))

    TextMetrics {
        id: textMetrics
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeChat
        text: root.text.length > 0 ? root.text : " "
    }

    Rectangle {
        id: bubble
        anchors.right: parent.right
        width: Math.max(root.attachmentWidth, root.desiredWidth)
        height: contentColumn.implicitHeight
                + (root.attachments.length > 0 ? 14 : 4)
        radius: 10
        color: Theme.userBubble

        Column {
            id: contentColumn
            x: 16
            y: root.attachments.length > 0 ? 8 : 3
            width: bubble.width - 32
            spacing: 8

            Row {
                visible: root.attachments.length > 0
                spacing: 6
                Repeater {
                    model: root.attachments
                    delegate: Rectangle {
                        width: 118
                        height: 82
                        radius: Theme.radiusSmall
                        color: Theme.codeBg
                        border.width: 1
                        border.color: Theme.border

                        Image {
                            anchors.fill: parent
                            anchors.margins: 1
                            source: modelData.previewUrl && modelData.previewUrl.length > 0
                                    ? modelData.previewUrl
                                    : app.imageUrl(modelData.path)
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: false
                            clip: true
                        }
                    }
                }
            }

            Text {
                id: textItem
                width: parent.width
                text: root.text
                textFormat: Text.PlainText
                color: Theme.userText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeChat
                lineHeight: 1.4
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
        }
    }

    HoverHandler { id: hover }

    Item {
        id: actions
        visible: hover.hovered
                 && (root.text.length > 0 || root.attachments.length > 0)
        width: 52
        height: 24
        anchors.right: bubble.left
        anchors.rightMargin: 8
        anchors.verticalCenter: bubble.verticalCenter

        Row {
            anchors.fill: parent
            spacing: 4

            Item {
                width: 24
                height: 24

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radiusSmall
                    color: copyHover.hovered ? Theme.hover : "transparent"
                    HoverHandler { id: copyHover }
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
                    onClicked: app.copyText(root.text)
                }
            }

            Item {
                width: 24
                height: 24
                opacity: root.canUndo ? 1.0 : 0.35

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radiusSmall
                    color: undoHover.hovered && root.canUndo ? Theme.hover : "transparent"
                    HoverHandler { id: undoHover }
                }
                AppIcon {
                    anchors.centerIn: parent
                    name: "undo"
                    size: 14
                    color: Theme.iconSecondary
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: root.canUndo
                    cursorShape: root.canUndo ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: root.undoRequested(root.sourceIndex, root.text)
                }
            }
        }
    }
}
