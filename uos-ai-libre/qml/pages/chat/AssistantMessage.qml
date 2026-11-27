import QtQuick
import UosAiLibre

Item {
    id: root
    property string text: ""
    property string reasoning: ""
    property bool reasoningActive: false
    property int reasoningElapsed: 0
    property bool streaming: false
    property var tools: []
    property var timeline: []
    property string activeText: ""
    property int elapsedMs: 0
    property string model: ""
    property string error: ""
    property string notice: ""
    width: parent ? parent.width : 0
    implicitHeight: column.implicitHeight + 8

    HoverHandler { id: messageHover }

    Row {
        id: row
        width: parent.width
        spacing: 12

        Image {
            id: avatar
            y: 1
            width: 28
            height: 28
            source: "qrc:/images/logo.png"
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        Column {
            id: column
            width: root.width - avatar.width - row.spacing
            spacing: 8

            ReasoningCard {
                visible: root.reasoning.length > 0 || root.reasoningActive
                width: column.width
                reasoning: root.reasoning
                active: root.reasoningActive
                elapsedMs: root.reasoningElapsed
            }

            Repeater {
                model: root.timeline
                delegate: Loader {
                    width: column.width
                    height: item ? item.implicitHeight : 0
                    sourceComponent: modelData.kind === "tools"
                                     ? timelineToolsComponent : timelineTextComponent
                    onLoaded: {
                        item.width = Qt.binding(function () { return column.width })
                        if (modelData.kind === "tools") {
                            item.tools = Qt.binding(function () { return modelData.tools })
                            item.active = Qt.binding(function () {
                                return root.streaming && modelData.active === true
                            })
                            item.elapsedMs = Qt.binding(function () { return root.elapsedMs })
                        } else {
                            item.text = Qt.binding(function () { return modelData.text })
                            item.streaming = false
                        }
                    }
                }
            }

            MarkdownView {
                id: markdown
                visible: root.activeText.length > 0
                width: column.width
                text: root.activeText
                streaming: root.streaming
            }

            LoadingDots {
                visible: root.streaming && root.activeText.length === 0
                         && root.timeline.length === 0 && !root.reasoningActive
                width: 22
                height: 14
            }

            Text {
                visible: root.error.length > 0
                width: column.width
                text: root.error
                color: Theme.danger
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                lineHeight: 1.5
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }

            Text {
                visible: root.notice.length > 0
                width: column.width
                text: root.notice
                color: Theme.textTertiary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }

            Item {
                width: column.width
                height: root.text.length > 0 ? 24 : 0
                opacity: messageHover.hovered && !root.streaming
                         && root.text.length > 0 ? 1 : 0
                enabled: opacity > 0

                Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }

                Row {
                    spacing: 4

                    Item {
                        width: 24
                        height: 24
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
                            onClicked: app.copyText(root.text)
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.model.length > 0 ? root.model : ""
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                    }
                }
            }
        }
    }

    Component {
        id: timelineTextComponent
        MarkdownView {}
    }

    Component {
        id: timelineToolsComponent
        ToolGroupCard {}
    }
}
