import QtQuick
import QtQuick.Controls
import UosAiLibre

SettingsScroll {
    id: page

    property int tab: 0
    property string query: ""
    property string pendingInstallId: ""
    property string pendingInstallName: ""
    property string pendingInstallTarget: ""
    property string pendingTrustName: ""
    property string pendingRemoveName: ""

    onVisibleChanged: {
        if (visible) {
            app.refreshSkillCatalog()
            app.refreshSessionApprovals()
        }
    }

    function contextLabel(value) {
        return value === "subagent" ? "子代理" : "内联"
    }

    function isUserSkill(skill) {
        const root = String(app.skillCatalog.install_root || "")
        return root.length > 0 && String(skill.base_dir || "").startsWith(root)
    }

    function slugOf(id) {
        const parts = String(id || "").split("/")
        return parts.length > 0 ? parts[parts.length - 1] : id
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
        text: "技能来自 aish 的技能目录和注册表；安装后先隔离，审查并信任后才会进入 AI 上下文。"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Row {
        spacing: 6
        Repeater {
            model: [{label: "已安装"}, {label: "浏览注册表"}]
            delegate: Rectangle {
                width: tabLabel.implicitWidth + 24
                height: 30
                radius: Theme.radiusSmall
                color: page.tab === index ? Theme.accentSoft
                                          : (tabHover.hovered ? Theme.hover : "transparent")
                border.width: 1
                border.color: page.tab === index ? Theme.accent : Theme.border
                HoverHandler { id: tabHover }
                Text {
                    id: tabLabel
                    anchors.centerIn: parent
                    text: modelData.label
                    color: page.tab === index ? Theme.accent : Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: page.tab === index ? Font.Medium : Font.Normal
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: page.tab = index
                }
            }
        }
    }

    // ------------------------------------------------------------- installed

    Column {
        width: parent.width
        spacing: 10
        visible: page.tab === 0

        Row {
            width: parent.width
            spacing: 8
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: (app.skillCatalog.skills ? app.skillCatalog.skills.length : 0)
                      + " 个已加载技能"
                color: Theme.textTertiary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }
            AppButton { text: "刷新"; onClicked: app.refreshSkillCatalog() }
            AppButton {
                text: "打开技能目录"
                onClicked: app.openPath(app.skillCatalog.install_root || "")
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
                            width: Math.max(80, Math.min(implicitWidth, skillColumn.width - 150))
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
                        AppButton { text: "审查"; onClicked: reviewDialog.openFor(modelData.name) }
                        AppButton { text: "打开目录"; onClicked: app.openPath(modelData.base_dir || "") }
                        AppButton {
                            visible: modelData.quarantined === true
                            text: "信任"
                            variant: "primary"
                            onClicked: {
                                page.pendingTrustName = modelData.name
                                trustDialog.open()
                            }
                        }
                        AppButton {
                            visible: page.isUserSkill(modelData)
                            text: "删除"
                            variant: "danger"
                            onClicked: {
                                page.pendingRemoveName = modelData.name
                                removeDialog.open()
                            }
                        }
                    }
                }
            }
        }

        Text {
            width: parent.width
            visible: !app.skillCatalog.skills || app.skillCatalog.skills.length === 0
            text: "当前没有已加载技能。可在“浏览注册表”中搜索安装，或把 SKILL.md 放入 aish 技能目录。"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }
    }

    // ---------------------------------------------------------------- browse

    Column {
        width: parent.width
        spacing: 10
        visible: page.tab === 1

        Row {
            width: parent.width
            spacing: 8
            AppTextField {
                id: skillSearchField
                width: parent.width - 96
                placeholder: "搜索技能，例如 pdf、rust、git"
                onTextEdited: (value) => page.query = value
                onAccepted: {
                    if (page.query.trim().length > 0)
                        app.searchSkills(page.query.trim())
                }
            }
            AppButton {
                text: app.skillSearchBusy ? "搜索中…" : "搜索"
                variant: "primary"
                enabled: !app.skillSearchBusy && page.query.trim().length > 0
                onClicked: app.searchSkills(page.query.trim())
            }
        }

        Row {
            width: parent.width
            spacing: 6
            Repeater {
                model: app.skillCatalog.registries || []
                delegate: Rectangle {
                    height: 22
                    width: registryText.implicitWidth + 18
                    radius: 4
                    color: modelData.enabled ? Theme.codeBg : "transparent"
                    border.width: 1
                    border.color: Theme.border
                    opacity: modelData.enabled ? 1 : 0.5
                    Text {
                        id: registryText
                        anchors.centerIn: parent
                        text: modelData.name + (modelData.default ? " · 默认" : "")
                        color: Theme.textTertiary
                        font.family: "DejaVu Sans Mono"
                        font.pixelSize: 9
                    }
                }
            }
        }

        Text {
            width: parent.width
            visible: (app.skillCatalog.registries || []).length === 0
            text: "没有配置技能注册表。可在 aish config.yaml 的 skills.registries 中添加。"
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }

        Text {
            width: parent.width
            visible: app.skillSearchBusy
            text: "正在搜索注册表…"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }

        Column {
            width: parent.width
            spacing: 4
            Repeater {
                model: (app.skillSearchResult.errors || [])
                delegate: Text {
                    width: parent.width
                    text: modelData.registry + "：" + modelData.error
                    color: Theme.danger
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTiny
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
            }
        }

        Repeater {
            model: (app.skillSearchResult.results || [])
            delegate: Rectangle {
                width: parent.width
                height: resultColumn.implicitHeight + 26
                radius: Theme.radiusMedium
                color: Theme.card
                border.width: 1
                border.color: Theme.cardBorder

                Column {
                    id: resultColumn
                    x: 14
                    y: 12
                    width: parent.width - 28
                    spacing: 5
                    Item {
                        width: parent.width
                        height: 30
                        Row {
                            anchors.left: parent.left
                            anchors.right: installButton.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.max(80, parent.width - 150)
                                text: modelData.name
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeBody
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: registryBadge.implicitWidth + 14
                                height: 20
                                radius: 4
                                color: Theme.accentSoft
                                Text {
                                    id: registryBadge
                                    anchors.centerIn: parent
                                    text: modelData.registry
                                    color: Theme.accent
                                    font.family: "DejaVu Sans Mono"
                                    font.pixelSize: 9
                                }
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.installs > 0 ? "↓ " + modelData.installs : ""
                                color: Theme.textTertiary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeTiny
                            }
                        }
                        AppButton {
                            id: installButton
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: app.skillInstallBusy ? "安装中…" : "安装"
                            minWidth: 54
                            horizontalPadding: 10
                            enabled: !app.skillInstallBusy
                            onClicked: {
                                page.pendingInstallId = modelData.id
                                page.pendingInstallName = modelData.name
                                page.pendingInstallTarget =
                                        String(app.skillCatalog.install_root || "") + "/"
                                        + page.slugOf(modelData.id)
                                installDialog.open()
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
                        text: modelData.id
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
            visible: !app.skillSearchBusy
                     && page.query.length > 0
                     && (app.skillSearchResult.results || []).length === 0
                     && (app.skillSearchResult.errors || []).length === 0
                     && app.skillSearchResult.query !== undefined
            text: "没有找到匹配的技能。"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }

        Text {
            width: parent.width
            text: "注册表的启用/默认设置仍由 aish config.yaml 管理；安装会写入 "
                  + (app.skillCatalog.install_root || "~/.config/aish/skills")
                  + "，并保持隔离直到审查信任。"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeTiny
            lineHeight: 1.45
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }
    }

    Connections {
        target: app
        function onSkillInstallFinished(result) {
            if (!result || result.ok !== true)
                return
            const data = result.data || ({})
            const meta = data.meta || ({})
            if (meta && (meta.display_name || meta.skill_name)) {
                reviewDialog.openFor(meta.display_name || meta.skill_name)
            }
        }
    }

    SkillReviewDialog {
        id: reviewDialog
        onTrustRequested: (name) => {
            page.pendingTrustName = name
            trustDialog.open()
        }
        onRemoveRequested: (name) => {
            page.pendingRemoveName = name
            removeDialog.open()
        }
    }

    ConfirmDialog {
        id: trustDialog
        title: "信任并启用技能"
        message: "确定信任“" + page.pendingTrustName + "”？"
        detail: "信任会移除 .untrusted 隔离标记，技能内容将进入 AI 上下文，并可能引导 AI 使用工具。请确认来源和内容可信。"
        confirmText: "信任并启用"
        danger: true
        onConfirmed: app.trustSkill(page.pendingTrustName)
    }

    ConfirmDialog {
        id: removeDialog
        title: "删除技能"
        message: "确定删除“" + page.pendingRemoveName + "”？"
        detail: "将删除 aish 用户技能目录中的该技能；此操作不可撤销。"
        confirmText: "删除"
        danger: true
        onConfirmed: {
            app.removeSkill(page.pendingRemoveName)
            reviewDialog.close()
        }
    }

    ConfirmDialog {
        id: installDialog
        title: "安装技能"
        message: "从注册表安装“" + page.pendingInstallName + "”？"
        detail: "来源 ID： " + page.pendingInstallId + "\n目标目录： " + page.pendingInstallTarget
                + "\n\n安装后技能处于隔离状态，不会进入 AI 上下文；需要审查并信任。"
        confirmText: "安装"
        onConfirmed: app.installSkill(page.pendingInstallId)
    }
}
