import QtQuick
import UosAiLibre

SettingsScroll {
    id: page

    onVisibleChanged: { if (visible) app.refreshMemory() }

    Text {
        text: "长期记忆"
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: 18
        font.weight: Font.DemiBold
    }
    Text {
        width: parent.width
        text: "记忆条目由 aish 的 memory 工具写入共享的长期记忆文件；这里只做展示，不在 GUI 中复制一份记忆库。"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Row {
        spacing: 8
        AppButton { text: "刷新"; onClicked: app.refreshMemory() }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: (app.memoryEntries.length || 0) + " 条"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    Repeater {
        model: app.memoryEntries
        delegate: Rectangle {
            width: parent.width
            height: memoryColumn.implicitHeight + 24
            radius: Theme.radiusMedium
            color: Theme.card
            border.width: 1
            border.color: Theme.cardBorder

            Column {
                id: memoryColumn
                x: 16
                y: 12
                width: parent.width - 32
                spacing: 5
                Text {
                    width: parent.width
                    text: modelData.content || ""
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
                Text {
                    width: parent.width
                    text: (modelData.category || "") + "  ·  " +
                          (modelData.scope || "") + "  ·  " +
                          (modelData.source || "") + "  ·  #" + (modelData.id || "")
                    color: Theme.textTertiary
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: Theme.fontSizeTiny
                    elide: Text.ElideMiddle
                }
            }
        }
    }

    Text {
        width: parent.width
        visible: app.memoryEntries.length === 0
        text: "当前没有长期记忆。"
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
    }
}
