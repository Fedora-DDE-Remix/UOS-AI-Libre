import QtQuick
import org.deepin.ds
import UosAiLibre

/// Wayland/Treeland quick-answer surface.
///
/// DLayerShellWindow is dde-shell's Qt binding for the generic
/// wlr-layer-shell-unstable-v1 protocol. Treeland implements that protocol.
///
/// Layer surfaces have no window-manager titlebar, so the header drives the
/// position itself: dragging updates the layer-shell margins while keeping
/// QWindow::x/y in sync for Item.mapToGlobal(). The latter is what makes the
/// drag follow the cursor instead of feeding surface-local coordinates back
/// into the move. This stays within the generic layer-shell protocol and does
/// not depend on Treeland's still-incomplete interactive-move extension.
Window {
    id: root
    property int mode: 1
    property string imagePath: ""
    property point anchorPoint: Qt.point(0, 0)
    property string externalError: ""
    property int dragCancelEpoch: 0

    // Relative to the current output. Kept separately from x/y because
    // layer-shell placement is expressed as anchors + margins.
    property real surfaceX: 0
    property real surfaceY: 0

    // Cursor position relative to the window. It changes when a screen-edge
    // clamp prevents the window from following the pointer, which is the
    // reliable "drag left the surface" signal for a layer-shell window.
    property real cursorLocalX: 0
    property real cursorLocalY: 0
    property bool dragOutside: false
    signal dragExited()

    width: 460
    height: 560
    visible: false
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint
    title: "UOS AI Libre"

    function clampX(value) {
        if (!screen)
            return Math.max(8, value)
        const max = Math.max(8, screen.width - width - 8)
        return Math.max(8, Math.min(max, value))
    }

    function clampY(value) {
        if (!screen)
            return Math.max(8, value)
        const max = Math.max(8, screen.height - height - 8)
        return Math.max(8, Math.min(max, value))
    }

    function applyPosition() {
        if (!screen)
            return
        // QWindow::x/y is not the layer-shell placement, but Qt's input code
        // uses it to compute global cursor coordinates. Keeping it in sync is
        // required for a stable drag delta.
        root.x = Math.round(surfaceX) + screen.virtualX
        root.y = Math.round(surfaceY) + screen.virtualY
    }

    function resetPosition() {
        if (!screen)
            return
        surfaceX = clampX(anchorPoint.x - screen.virtualX)
        surfaceY = clampY(anchorPoint.y - screen.virtualY)
        applyPosition()
    }

    function resetDragState() {
        dragOutside = false
        cursorLocalX = 0
        cursorLocalY = 0
    }

    function beginDrag(localX, localY) {
        resetDragState()
        applyPosition()
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
        // The pointer moved by (dx, dy), the window by (appliedX, appliedY).
        // The difference moves the cursor inside the window; when a clamp
        // stops the window, the cursor eventually leaves the surface.
        cursorLocalX += dx - appliedX
        cursorLocalY += dy - appliedY
        // On Wayland the compositor places the surface from its margins; the
        // QWindow x/y is only needed by the local-coordinate X11/fallback
        // path. Skip the geometry churn for every relative-pointer sample.
        if (!panel.relativeDrag)
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

    DLayerShellWindow.anchors: DLayerShellWindow.AnchorTop
                                 | DLayerShellWindow.AnchorLeft
    DLayerShellWindow.layer: DLayerShellWindow.LayerOverlay
    DLayerShellWindow.exclusionZone: -1
    DLayerShellWindow.keyboardInteractivity:
        DLayerShellWindow.KeyboardInteractivityOnDemand
    DLayerShellWindow.scope: "uos-ai-libre/quick-ai"
    DLayerShellWindow.closeOnDismissed: true
    DLayerShellWindow.leftMargin: Math.round(surfaceX)
    DLayerShellWindow.topMargin: Math.round(surfaceY)

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
