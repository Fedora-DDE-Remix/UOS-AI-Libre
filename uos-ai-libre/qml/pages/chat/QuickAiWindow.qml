import QtQuick
import org.deepin.ds
import UosAiLibre

/// Wayland/Treeland quick-answer surface.
///
/// DLayerShellWindow is dde-shell's Qt binding for the generic
/// wlr-layer-shell-unstable-v1 protocol. Treeland implements that protocol;
/// its treeland-layer-shell-extension is only needed for compositor-driven
/// interactive resize/move, not for a fixed-size popup.
Window {
    id: root
    property int mode: 1
    property string imagePath: ""
    property point anchorPoint: Qt.point(0, 0)
    property string externalError: ""

    width: 460
    height: 560
    visible: false
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint
    title: "UOS AI Libre"

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
    DLayerShellWindow.leftMargin: {
        if (!screen)
            return 0
        const max = Math.max(8, screen.width - width - 8)
        return Math.max(8, Math.min(max,
                                  anchorPoint.x - screen.virtualX))
    }
    DLayerShellWindow.topMargin: {
        if (!screen)
            return 0
        const max = Math.max(8, screen.height - height - 8)
        return Math.max(8, Math.min(max,
                                  anchorPoint.y - screen.virtualY))
    }

    QuickAiPanel {
        anchors.fill: parent
        mode: root.mode
        imagePath: root.imagePath
        anchorPoint: root.anchorPoint
        externalError: root.externalError
        onCloseRequested: root.close()
    }
}
