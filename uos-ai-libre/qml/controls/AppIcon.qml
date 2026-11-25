import QtQuick
import UosAiLibre

Image {
    id: root
    property string name
    property int size: 16
    property color color: Theme.icon

    width: size
    height: size
    source: name.length > 0
            ? "image://appicon/" + name + "?color=" + encodeURIComponent(String(color))
            : ""
    sourceSize: Qt.size(Math.max(1, size * Screen.devicePixelRatio),
                        Math.max(1, size * Screen.devicePixelRatio))
    fillMode: Image.PreserveAspectFit
    smooth: true
    asynchronous: false
    cache: true
}
