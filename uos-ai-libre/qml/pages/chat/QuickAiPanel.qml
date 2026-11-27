import QtQuick
import QtQuick.Controls
import UosAiLibre

/// Compact surface used by screen-recorder AI actions.
///
/// The panel deliberately binds to the existing ChatModel's last assistant
/// row. It does not own an agent/session; AppController starts a normal
/// conversation, so reasoning/tool/text ordering stays identical to the main
/// chat and the result remains available there after the popup closes.
Item {
    id: root
    property int mode: 1
    property string imagePath: ""
    property point anchorPoint: Qt.point(0, 0)
    property string externalError: ""
    property var snapshot: ({})
    signal closeRequested()

    readonly property bool hasAnswer: snapshot
        && snapshot.row !== undefined
    readonly property string modeTitle: {
        switch (mode) {
        case 1: return "解释截图"
        case 2: return "总结截图"
        case 3: return "翻译截图"
        default: return "截图问答"
        }
    }

    function refresh() {
        snapshot = app.chat.lastAssistantSnapshot()
    }

    Component.onCompleted: refresh()

    Connections {
        target: app.chat
        function onContentChanged() { root.refresh() }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLarge
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder

        Column {
            id: content
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            Item {
                width: parent.width
                height: 38

                Image {
                    id: logo
                    width: 24
                    height: 24
                    anchors.verticalCenter: parent.verticalCenter
                    source: "qrc:/images/logo.png"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                Column {
                    anchors.left: logo.right
                    anchors.leftMargin: 9
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1
                    Text {
                        text: root.modeTitle
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: root.hasAnswer && root.snapshot.model
                              ? root.snapshot.model : "AI"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                        elide: Text.ElideRight
                    }
                }

                Item {
                    width: 28
                    height: 28
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    HoverHandler { id: closeHover }
                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusSmall
                        color: closeHover.hovered ? Theme.hover : "transparent"
                    }
                    AppIcon {
                        anchors.centerIn: parent
                        name: "x"
                        size: 13
                        color: Theme.icon
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.closeRequested()
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.border
            }

            Row {
                width: parent.width
                spacing: 10
                visible: root.imagePath.length > 0

                Rectangle {
                    width: 92
                    height: 64
                    radius: Theme.radiusSmall
                    color: Theme.codeBg
                    clip: true
                    Image {
                        anchors.fill: parent
                        anchors.margins: 2
                        source: root.imagePath.length > 0
                                ? app.imageUrl(root.imagePath) : ""
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        cache: false
                    }
                }

                Column {
                    width: parent.width - 102
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3
                    Text {
                        width: parent.width
                        text: "来自截图"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }
                    Text {
                        width: parent.width
                        text: "结果也会保留在聊天记录中"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                }
            }

            Flickable {
                width: parent.width
                height: parent.height - y
                contentWidth: width
                contentHeight: resultColumn.implicitHeight + 8
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Column {
                    id: resultColumn
                    width: parent.width
                    spacing: 8

                    Item {
                        width: parent.width
                        height: !root.hasAnswer && root.externalError.length === 0
                                ? 54 : 0
                        visible: height > 0
                        Row {
                            anchors.centerIn: parent
                            spacing: 8
                            Spinner {
                                width: 14
                                height: 14
                                anchors.verticalCenter: parent.verticalCenter
                                color: Theme.accent
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "正在分析截图…"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                            }
                        }
                    }

                    AssistantMessage {
                        width: parent.width
                        visible: root.hasAnswer
                        text: root.hasAnswer ? (root.snapshot.text || "") : ""
                        reasoning: root.hasAnswer ? (root.snapshot.reasoning || "") : ""
                        reasoningActive: root.hasAnswer
                                         && root.snapshot.reasoningActive === true
                        reasoningElapsed: root.hasAnswer
                                          ? Number(root.snapshot.reasoningElapsed || 0) : 0
                        streaming: root.hasAnswer
                                   && root.snapshot.streaming === true
                        tools: []
                        timeline: root.hasAnswer ? (root.snapshot.timeline || []) : []
                        activeText: root.hasAnswer ? (root.snapshot.activeText || "") : ""
                        elapsedMs: root.hasAnswer ? Number(root.snapshot.elapsedMs || 0) : 0
                        model: root.hasAnswer ? (root.snapshot.model || "") : ""
                        error: root.hasAnswer ? (root.snapshot.error || "") : ""
                        notice: root.hasAnswer ? (root.snapshot.notice || "") : ""
                    }

                    Text {
                        width: parent.width
                        visible: root.externalError.length > 0
                        text: root.externalError
                        color: Theme.danger
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                }
            }
        }
    }
}
