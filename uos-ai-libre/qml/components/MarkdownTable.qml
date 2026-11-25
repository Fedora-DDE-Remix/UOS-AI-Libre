import QtQuick
import UosAiLibre

Item {
    id: root
    property var rows: []
    property var alignments: []
    property var columnWidths: []
    width: parent ? parent.width : 0
    implicitHeight: rows.length > 0 ? tableItem.height + 24 : 0

    readonly property int columns: rows.length > 0 && rows[0] ? rows[0].length : 0
    readonly property real tableWidth: {
        let total = 0
        for (let i = 0; i < columns; ++i)
            total += cellWidth(i)
        return Math.max(total, 1)
    }

    function cellWidth(index) {
        const weight = columnWidths.length > index ? Number(columnWidths[index]) : (1 / Math.max(1, columns))
        return Math.max(72, width * weight)
    }

    function cellAlignment(index) {
        if (index >= alignments.length)
            return Text.AlignLeft
        const value = Number(alignments[index])
        return value === 1 ? Text.AlignHCenter : (value === 2 ? Text.AlignRight : Text.AlignLeft)
    }

    Flickable {
        id: flick
        y: 12
        width: root.width
        height: tableItem.height
        contentWidth: tableItem.width
        contentHeight: tableItem.height
        clip: true
        interactive: contentWidth > width
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds

        Item {
            id: tableItem
            width: root.tableWidth
            height: tableColumn.height

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusMedium
                color: Theme.card
                border.width: 1
                border.color: Theme.tableBorder
            }

            Rectangle {
                width: parent.width
                height: 38
                color: Theme.tableHeader
                topLeftRadius: Theme.radiusMedium
                topRightRadius: Theme.radiusMedium
            }

            Column {
                id: tableColumn
                width: parent.width

                Row {
                    width: tableColumn.width
                    height: 38
                    Repeater {
                        model: root.rows.length > 0 ? root.rows[0] : []
                        delegate: Item {
                            width: root.cellWidth(index)
                            height: 38

                            Rectangle {
                                visible: index > 0
                                x: 0
                                width: 1
                                height: parent.height
                                color: Theme.tableBorder
                            }

                            Text {
                                x: 12
                                y: 0
                                width: parent.width - 24
                                height: 38
                                text: modelData
                                textFormat: Text.MarkdownText
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                horizontalAlignment: root.cellAlignment(index)
                                verticalAlignment: Text.AlignVCenter
                                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                lineHeight: 1.35
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.tableBorder
                }

                Repeater {
                    model: root.rows.length > 1 ? root.rows.slice(1) : []
                    delegate: Row {
                        width: tableColumn.width
                        height: implicitHeight

                        Repeater {
                            model: modelData
                            delegate: Item {
                                width: root.cellWidth(index)
                                height: cellText.implicitHeight + 20

                                Rectangle {
                                    visible: index > 0
                                    x: 0
                                    width: 1
                                    height: parent.height
                                    color: Theme.tableBorder
                                }

                                Text {
                                    id: cellText
                                    x: 12
                                    y: 10
                                    width: parent.width - 24
                                    text: modelData
                                    textFormat: Text.MarkdownText
                                    color: Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 13
                                    lineHeight: 1.5
                                    horizontalAlignment: root.cellAlignment(index)
                                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                }
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: 1
                            y: parent.height - 1
                            color: Theme.tableBorder
                        }
                    }
                }
            }
        }
    }

    HoverHandler { cursorShape: Qt.ArrowCursor }
}
