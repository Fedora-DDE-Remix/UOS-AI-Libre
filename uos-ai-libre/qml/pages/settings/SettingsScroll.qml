import QtQuick
import QtQuick.Controls
import UosAiLibre

Flickable {
    id: root
    default property alias contentItems: column.data
    clip: true
    contentHeight: column.implicitHeight + 60
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    Column {
        id: column
        x: Math.max(24, (parent.width - width) / 2)
        y: 10
        width: Math.min(680, parent.width - 128)
        spacing: 18
    }
}
