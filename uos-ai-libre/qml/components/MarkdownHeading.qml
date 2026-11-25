import QtQuick
import UosAiLibre

Text {
    property string blockText
    property int level: 1
    width: parent ? parent.width : 0
    text: blockText
    textFormat: Text.MarkdownText
    color: Theme.textPrimary
    font.family: Theme.fontFamily
    font.pixelSize: level <= 1 ? 25 : (level === 2 ? 21 : (level === 3 ? 17 : 15))
    font.weight: level <= 4 ? Font.DemiBold : Font.Medium
    lineHeight: 1.4
    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    topPadding: level <= 1 ? 22 : (level === 2 ? 18 : 12)
    bottomPadding: level <= 1 ? 10 : (level === 2 ? 8 : 5)
    linkColor: Theme.accent
    onLinkActivated: (link) => app.openExternal(link)
}
