import QtQuick
import UosAiLibre

Item {
    id: root
    property color color: Theme.accent
    property bool running: true
    width: 16
    height: 16

    onColorChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = root.color
            ctx.lineWidth = 1.8
            ctx.lineCap = "round"
            ctx.beginPath()
            ctx.arc(width / 2, height / 2, width / 2 - 1.4,
                    -Math.PI / 2, Math.PI * 1.05)
            ctx.stroke()
        }
        Connections {
            target: Theme
            function onIsDarkChanged() { canvas.requestPaint() }
        }
    }

    RotationAnimator {
        target: canvas
        from: 0
        to: 360
        duration: 900
        loops: Animation.Infinite
        running: root.running && root.visible
                 && (!root.window || root.window.visible)
    }
}
