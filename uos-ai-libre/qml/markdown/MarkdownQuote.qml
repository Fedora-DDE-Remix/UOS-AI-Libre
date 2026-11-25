import QtQuick
import UosAiLibre

Item {
    property string blockText
    width: parent ? parent.width : 0
    implicitHeight: quoteText.implicitHeight + 16

    Rectangle {
        x: 2
        y: 8
        width: 3
        height: parent.implicitHeight - 16
        radius: 1.5
        color: Theme.borderStrong
    }

    Text {
        id: quoteText
        x: 18
        y: 8
        width: parent.width - 22
        text: blockText
        textFormat: Text.MarkdownText
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeChat
        lineHeight: 1.65
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        linkColor: Theme.accent
        onLinkActivated: (link) => app.openExternal(link)
    }
}
