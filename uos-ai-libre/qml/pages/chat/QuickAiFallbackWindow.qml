import QtQuick
import UosAiLibre

/// X11 (and layer-shell-unavailable) fallback for the quick-answer surface.
Window {
    id: root
    property int mode: 1
    property string imagePath: ""
    property point anchorPoint: Qt.point(0, 0)
    property string externalError: ""
    property int dragCancelEpoch: 0

    property real surfaceX: 0
    property real surfaceY: 0

    property real cursorLocalX: 0
    property real cursorLocalY: 0
    property bool dragOutside: false
    signal dragExited()

    width: 460
    height: 560
    visible: false
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    title: "UOS AI Libre"

    function clampX(value) {
        if (!screen)
            return Math.max(0, value)
        const min = screen.virtualX + 8
        const max = Math.max(min,
                             screen.virtualX + screen.width - width - 8)
        return Math.max(min, Math.min(max, value))
    }

    function clampY(value) {
        if (!screen)
            return Math.max(0, value)
        const min = screen.virtualY + 8
        const max = Math.max(min,
                             screen.virtualY + screen.height - height - 8)
        return Math.max(min, Math.min(max, value))
    }

    function applyPosition() {
        root.x = Math.round(surfaceX)
        root.y = Math.round(surfaceY)
    }

    function resetPosition() {
        surfaceX = clampX(anchorPoint.x)
        surfaceY = clampY(anchorPoint.y)
        applyPosition()
    }

    function resetDragState() {
        dragOutside = false
        cursorLocalX = 0
        cursorLocalY = 0
    }

    function beginDrag(localX, localY) {
        resetDragState()
        cursorLocalX = localX
        cursorLocalY = localY
    }

    function moveBy(dx, dy) {
        if (dragOutside)
            return
        const oldX = surfaceX
        const oldY = surfaceY
        surfaceX = clampX(surfaceX + dx)
        surfaceY = clampY(surfaceY + dy)
        const appliedX = surfaceX - oldX
        const appliedY = surfaceY - oldY
        cursorLocalX += dx - appliedX
        cursorLocalY += dy - appliedY
        applyPosition()
        if (cursorLocalX < 0 || cursorLocalX > width
            || cursorLocalY < 0 || cursorLocalY > height) {
            dragOutside = true
            dragExited()
        }
    }

    onDragExited: panel.stopDrag()
    onVisibleChanged: {
        if (visible)
            resetDragState()
    }
    onAnchorPointChanged: resetPosition()
    onScreenChanged: resetPosition()
    Component.onCompleted: resetPosition()

    screen: {
        const screens = Qt.application.screens
        for (let i = 0; i < screens.length; ++i) {
            const gx = screens[i].virtualX
            const gy = screens[i].virtualY
            const gw = screens[i].width
            const gh = screens[i].height
            if (anchorPoint.x >= gx && anchorPoint.x < gx + gw
                && anchorPoint.y >= gy && anchorPoint.y < gy + gh)
                return screens[i]
        }
        return screens.length > 0 ? screens[0] : null
    }

    QuickAiPanel {
        id: panel
        anchors.fill: parent
        mode: root.mode
        imagePath: root.imagePath
        anchorPoint: root.anchorPoint
        externalError: root.externalError
        dragCancelEpoch: root.dragCancelEpoch
        onCloseRequested: root.close()
        onDragRequested: function(dx, dy) { root.moveBy(dx, dy) }
        onDragStarted: function(localX, localY) {
            root.beginDrag(localX, localY)
        }
    }
}
