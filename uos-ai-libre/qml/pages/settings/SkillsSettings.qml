import QtQuick
import UosAiLibre

SettingsScroll {
    id: page

    onVisibleChanged: { if (visible) app.refreshSkillCatalog() }

    function contextLabel(value) {
        return value === "subagent" ? "子代理" : "内联"
    }

    Text {
        text: "技能"
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: 18
        font.weight: Font.DemiBold
    }
    Text {
        width: parent.width
        text: "技能来自 aish 的技能目录；安装、信任和隔离状态都由 aish 管理。GUI 不维护第二份技能列表。"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Row {
        spacing: 8
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: (app.skillCatalog.skills ? app.skillCatalog.skills.length : 0) + " 个已加载技能"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }
        AppButton { text: "刷新"; onClicked: app.refreshSkillCatalog() }
        AppButton {
            text: "打开技能目录"
            onClicked: {
                const roots = app.skillCatalog.roots || []
                if (roots.length > 0)
                    app.openPath(roots[0])
            }
        }
    }

    Text {
        width: parent.width
        visible: app.skillCatalog.roots !== undefined
        text: (app.skillCatalog.roots || []).join("\n")
        color: Theme.textTertiary
        font.family: "DejaVu Sans Mono"
        font.pixelSize: Theme.fontSizeTiny
        lineHeight: 1.45
        wrapMode: Text.WrapAnywhere
    }

    Repeater {
        model: app.skillCatalog.skills || []
        delegate: Rectangle {
            width: parent.width
            height: skillColumn.implicitHeight + 28
            radius: Theme.radiusMedium
            color: Theme.card
            border.width: 1
            border.color: modelData.quarantined ? Theme.warning : Theme.cardBorder

            Column {
                id: skillColumn
                x: 16
                y: 14
                width: parent.width - 32
                spacing: 6
                Row {
                    width: parent.width
                    spacing: 8
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, skillColumn.width - 120)
                        text: modelData.name
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: contextText.implicitWidth + 14
                        height: 20
                        radius: 4
                        color: Theme.codeBg
                        Text {
                            id: contextText
                            anchors.centerIn: parent
                            text: page.contextLabel(modelData.context)
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: 9
                        }
                    }
                    Rectangle {
                        visible: modelData.quarantined === true
                        anchors.verticalCenter: parent.verticalCenter
                        width: quarantineText.implicitWidth + 14
                        height: 20
                        radius: 4
                        color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.12)
                        Text {
                            id: quarantineText
                            anchors.centerIn: parent
                            text: "待信任"
                            color: Theme.warning
                            font.family: Theme.fontFamily
                            font.pixelSize: 9
                        }
                    }
                }
                Text {
                    width: parent.width
                    text: modelData.description || ""
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
                Text {
                    width: parent.width
                    visible: modelData.agent !== undefined && modelData.agent !== null
                             && String(modelData.agent).length > 0
                    text: "agent: " + (modelData.agent || "") +
                          (modelData.allowed_tools && modelData.allowed_tools.length > 0
                           ? "   allowed-tools: " + modelData.allowed_tools.join(", ") : "")
                    color: Theme.textTertiary
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: Theme.fontSizeTiny
                    elide: Text.ElideMiddle
                }
                Text {
                    width: parent.width
                    text: modelData.base_dir || ""
                    color: Theme.textTertiary
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: Theme.fontSizeTiny
                    elide: Text.ElideMiddle
                }
                Row {
                    spacing: 8
                    AppButton {
                        text: "打开目录"
                        onClicked: app.openPath(modelData.base_dir || "")
                    }
                    AppButton {
                        visible: modelData.quarantined === true
                        text: "信任"
                        variant: "primary"
                        onClicked: app.trustSkill(modelData.name)
                    }
                }
            }
        }
    }

    Text {
        width: parent.width
        visible: !app.skillCatalog.skills || app.skillCatalog.skills.length === 0
        text: "当前没有已加载技能。可将 SKILL.md 放入 aish 技能目录，或通过 aish CLI 安装。"
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }
}
