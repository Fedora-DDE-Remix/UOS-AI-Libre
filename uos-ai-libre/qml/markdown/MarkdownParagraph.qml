import QtQuick
import UosAiLibre

Text {
    property string blockText
    width: parent ? parent.width : 0
    text: blockText
    textFormat: Text.MarkdownText
    color: Theme.textPrimary
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeChat
    lineHeight: 1.68
    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    topPadding: 6
    bottomPadding: 6
    linkColor: Theme.accent
    onLinkActivated: (link) => app.openExternal(link)
}
