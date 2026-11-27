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
    property bool dragging: false
    property bool relativeDrag: false
    property bool leftDuringDrag: false
    property int dragCancelEpoch: 0
    signal closeRequested()
    signal dragRequested(real dx, real dy)
    signal dragStarted(real localX, real localY)

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
    onDragCancelEpochChanged: {
        if (root.dragging)
            root.stopDrag()
    }

    Connections {
        target: app.chat
        function onContentChanged() { root.refresh() }
    }

    Connections {
        target: quickAiDrag
        function onMoved(dx, dy) {
            if (root.dragging && root.relativeDrag && !root.leftDuringDrag)
                root.dragRequested(dx, dy)
        }
        function onReleased() {
            root.stopDrag()
        }
    }

    function stopDrag(resetMouse) {
        if (!root.dragging && !root.relativeDrag) {
            dragLeaveTimer.stop()
            return
        }
        if (root.relativeDrag)
            quickAiDrag.end()
        root.relativeDrag = false
        root.dragging = false
        root.leftDuringDrag = false
        dragLeaveTimer.stop()

        // A release delivered to another surface never reaches this
        // MouseArea, leaving QQuickMouseArea::pressed stuck. Toggling
        // visibility synchronously triggers its itemChange -> ungrabMouse()
        // path, which clears the stale press and emits canceled().
        if (resetMouse !== false) {
            dragArea.visible = false
            dragArea.visible = true
        }
    }

    // Layer-shell cannot grab the pointer globally: if the button is released
    // outside the window, no release event reaches the MouseArea. Once a drag
    // leaves the panel, never resume it on re-entry; a short timer is only a
    // fallback for the case where the pointer never comes back.
    Timer {
        id: dragLeaveTimer
        interval: 160
        repeat: false
        onTriggered: root.stopDrag()
    }

    HoverHandler {
        id: panelHover
        onHoveredChanged: {
            if (!root.dragging)
                return
            if (!hovered) {
                root.leftDuringDrag = true
                dragLeaveTimer.start()
            } else if (root.leftDuringDrag) {
                // The pointer was released outside. Do not let the next
                // relative-pointer event move the window before we notice.
                root.stopDrag()
            }
        }
    }

    Component.onDestruction: root.stopDrag(false)

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
                id: header
                width: parent.width
                height: 42

                // Keep the logo and the title on one optical center line. The
                // model subtitle sits under the title instead of pulling the
                // title upwards as a vertically-centered Column would.
                Row {
                    id: titleRow
                    anchors.left: parent.left
                    anchors.top: parent.top
                    spacing: 9

                    Image {
                        id: logo
                        width: 24
                        height: 24
                        source: "qrc:/images/logo.png"
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }

                    Text {
                        id: titleText
                        width: Math.max(0, header.width - 33 - 40)
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.modeTitle
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                }

                Text {
                    anchors.left: titleRow.left
                    anchors.leftMargin: 33
                    anchors.top: titleRow.bottom
                    anchors.topMargin: 1
                    width: Math.max(0, header.width - 33 - 40)
                    text: root.hasAnswer && root.snapshot.model
                          ? root.snapshot.model : "AI"
                    color: Theme.textTertiary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTiny
                    elide: Text.ElideRight
                }

                // Layer-shell surfaces do not get window-manager titlebar
                // dragging. The host translates this drag into margin updates
                // on Wayland and x/y updates on X11.
                MouseArea {
                    id: dragArea
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: pressed ? Qt.ClosedHandCursor
                                         : Qt.SizeAllCursor
                    property point lastGlobal: Qt.point(0, 0)

                    onPressed: function(mouse) {
                        root.leftDuringDrag = false
                        root.dragging = true
                        dragLeaveTimer.stop()
                        // On Wayland the raw relative deltas are independent
                        // of the surface position. X11 (or a compositor
                        // without relative-pointer) keeps the local fallback.
                        root.relativeDrag = quickAiDrag.begin()
                        if (!root.relativeDrag)
                            lastGlobal = root.mapToGlobal(mouse.x, mouse.y)
                        // The MouseArea lives in the header, not at the panel
                        // origin. Convert to panel coordinates so the
                        // screen-edge clamp check measures the whole window.
                        const local = root.mapFromItem(dragArea, mouse.x,
                                                       mouse.y)
                        root.dragStarted(local.x, local.y)
                    }
                    onPositionChanged: function(mouse) {
                        if (!root.dragging)
                            return
                        // If the release happened while the pointer was
                        // outside, the button mask tells us before we move
                        // again.
                        if (!(mouse.buttons & Qt.LeftButton)) {
                            root.stopDrag()
                            return
                        }
                        if (root.leftDuringDrag)
                            return
                        if (root.relativeDrag)
                            return
                        const global = root.mapToGlobal(mouse.x, mouse.y)
                        const dx = global.x - lastGlobal.x
                        const dy = global.y - lastGlobal.y
                        lastGlobal = global
                        if (dx !== 0 || dy !== 0)
                            root.dragRequested(dx, dy)
                    }
                    onReleased: root.stopDrag(false)
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
                id: quickFlick
                width: parent.width
                height: parent.height - y
                contentWidth: width
                contentHeight: resultColumn.implicitHeight + 8
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                // Reparent the attached scroll bar to the panel and anchor it
                // in the 14px content margin, so it cannot overlap the answer
                // text and sits closer to the rounded window edge.
                ScrollBar.vertical: ScrollBar {
                    parent: root
                    z: 10
                    y: quickFlick.mapToItem(root, 0, 0).y
                    height: quickFlick.height
                    width: 8
                    x: root.width - width - 2
                    policy: ScrollBar.AsNeeded
                }

                Column {
                    id: resultColumn
                    width: parent.width
                    spacing: 8

                    Item {
                        id: loadingState
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
                                running: loadingState.visible
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
