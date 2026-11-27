import QtQuick
import QtQml.Models
import UosAiLibre

Item {
    id: root
    property string text: ""
    property bool streaming: false
    width: parent ? parent.width : 0
    implicitHeight: contentColumn.implicitHeight + (markdownModel.truncated ? 26 : 0)

    // Vertical center of the first rendered text line, in this item's
    // coordinates. AssistantMessage uses it to optically align its avatar with
    // a leading heading/paragraph instead of the block's top padding.
    readonly property real firstTextLineCenter: {
        if (contentColumn.children.length === 0)
            return -1
        const first = contentColumn.children[0]
        const text = first ? first.item : null
        if (!text || text.font === undefined
            || text.font.pixelSize === undefined)
            return -1
        const lineHeight = text.lineHeight !== undefined ? text.lineHeight : 1
        return (text.topPadding || 0) + text.font.pixelSize * lineHeight / 2
    }

    MarkdownModel {
        id: markdownModel
        text: root.text
    }

    Component { id: paragraphComponent; MarkdownParagraph {} }
    Component { id: headingComponent; MarkdownHeading {} }
    Component { id: listComponent; MarkdownListItem {} }
    Component { id: codeComponent; MarkdownCode {} }
    Component { id: tableComponent; MarkdownTable {} }
    Component { id: quoteComponent; MarkdownQuote {} }
    Component { id: mathComponent; MarkdownMath {} }
    Component { id: ruleComponent; MarkdownRule {} }

    Column {
        id: contentColumn
        width: parent.width
        spacing: 0

        Repeater {
            model: markdownModel
            delegate: Loader {
                id: blockLoader
                width: contentColumn.width
                height: implicitHeight

                sourceComponent: {
                    switch (model.type) {
                    case "heading": return headingComponent
                    case "bullet":
                    case "ordered": return listComponent
                    case "code": return codeComponent
                    case "table": return tableComponent
                    case "quote": return quoteComponent
                    case "math": return mathComponent
                    case "hr": return ruleComponent
                    default: return paragraphComponent
                    }
                }

                function bindIfPresent(property, value) {
                    if (item && item[property] !== undefined)
                        item[property] = value
                }

                onLoaded: {
                    if (!item)
                        return
                    bindIfPresent("blockText", Qt.binding(function() { return model.text }))
                    bindIfPresent("level", Qt.binding(function() { return model.level }))
                    bindIfPresent("marker", Qt.binding(function() { return model.marker }))
                    bindIfPresent("indent", Qt.binding(function() { return model.indent }))
                    bindIfPresent("checked", Qt.binding(function() { return model.checked }))
                    bindIfPresent("lang", Qt.binding(function() { return model.lang }))
                    bindIfPresent("closed", Qt.binding(function() { return model.closed }))
                    bindIfPresent("rows", Qt.binding(function() { return model.rows }))
                    bindIfPresent("alignments", Qt.binding(function() { return model.alignments }))
                    bindIfPresent("columnWidths", Qt.binding(function() { return model.columnWidths }))
                }
            }
        }
    }

    Text {
        visible: markdownModel.truncated
        y: contentColumn.implicitHeight + 4
        width: parent.width
        text: "内容过长，已截断显示"
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        topPadding: 4
    }
}
