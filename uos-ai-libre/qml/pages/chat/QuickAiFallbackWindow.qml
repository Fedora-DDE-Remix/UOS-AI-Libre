import QtQuick
import UosAiLibre

/// X11 (and layer-shell-unavailable) fallback for the quick-answer surface.
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
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
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

    x: screen ? Math.max(screen.virtualX + 8,
                         Math.min(screen.virtualX + screen.width - width - 8,
                                  anchorPoint.x)) : 0
    y: screen ? Math.max(screen.virtualY + 8,
                         Math.min(screen.virtualY + screen.height - height - 8,
                                  anchorPoint.y)) : 0

    QuickAiPanel {
        anchors.fill: parent
        mode: root.mode
        imagePath: root.imagePath
        anchorPoint: root.anchorPoint
        externalError: root.externalError
        onCloseRequested: root.close()
    }
}
