import QtQuick
import QtQuick.Controls
import UosAiLibre

SettingsScroll {
    id: page

    property string pendingDeleteName: ""

    onVisibleChanged: {
        if (visible)
            app.refreshProviders()
    }

    function protocolLabel(value) {
        return value === "anthropic" ? "Anthropic Messages" : "OpenAI 兼容"
    }

    function keyLabel(provider) {
        return provider.has_key === true ? "密钥已配置" : "未配置密钥"
    }

    function modelLabel(provider) {
        return provider.model !== undefined && String(provider.model).length > 0
                ? provider.model : "未选择模型"
    }

    function providerKind(provider) {
        return provider.account === true ? "备用账户" : "主供应商"
    }

    function activate(provider) {
        if (String(provider.model || "").length === 0) {
            providerDialog.openFor(provider)
            return
        }
        const result = app.saveProvider(provider.name, provider.api_base, "",
                                        provider.model, provider.protocol, true)
        if (!result.ok)
            app.showToast(result.error || "切换供应商失败")
    }

    Text {
        text: "模型与供应商"
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: 18
        font.weight: Font.DemiBold
    }
    Text {
        width: parent.width
        text: "供应商来自 aish 的主端点和 api_accounts。每个供应商有自己的 API 地址、协议和模型；切换后会重写 aish config.yaml 并重建会话。"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Item {
        width: parent.width
        height: 30
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "已配置供应商"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.Medium
        }
        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            AppButton {
                text: "添加供应商"
                variant: "primary"
                onClicked: providerDialog.openNew()
            }
        }
    }

    Text {
        width: parent.width
        text: "协议检查：aish 只对 api.anthropic.com 使用 Anthropic Messages，其它地址使用 OpenAI 兼容协议。自定义 Anthropic 兼容代理目前需要 aish 后端支持。"
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeTiny
        lineHeight: 1.45
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Repeater {
        model: app.providers || []
        delegate: Rectangle {
            width: parent.width
            height: providerColumn.implicitHeight + 28
            radius: Theme.radiusMedium
            color: Theme.card
            border.width: 1
            border.color: modelData.active === true ? Theme.accent : Theme.cardBorder

            Column {
                id: providerColumn
                x: 16
                y: 14
                width: parent.width - 32
                spacing: 7

                Item {
                    width: parent.width
                    height: 30
                    Row {
                        anchors.left: parent.left
                        anchors.right: providerActions.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.max(80, Math.min(implicitWidth, providerColumn.width - 390))
                            text: modelData.name || modelData.host || "供应商"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeBody
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            visible: modelData.active === true
                            anchors.verticalCenter: parent.verticalCenter
                            width: 42
                            height: 20
                            radius: 4
                            color: Theme.accentSoft
                            Text {
                                anchors.centerIn: parent
                                text: "当前"
                                color: Theme.accent
                                font.family: Theme.fontFamily
                                font.pixelSize: 9
                            }
                        }
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: protocolText.implicitWidth + 14
                            height: 20
                            radius: 4
                            color: Theme.codeBg
                            Text {
                                id: protocolText
                                anchors.centerIn: parent
                                text: page.protocolLabel(modelData.protocol)
                                color: Theme.textSecondary
                                font.family: "DejaVu Sans Mono"
                                font.pixelSize: 9
                            }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: page.providerKind(modelData)
                            color: Theme.textTertiary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTiny
                        }
                    }
                    Row {
                        id: providerActions
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6
                        AppButton {
                            visible: modelData.active !== true
                            text: "切换"
                            minWidth: 48
                            horizontalPadding: 9
                            onClicked: page.activate(modelData)
                        }
                        AppButton {
                            text: "编辑"
                            minWidth: 48
                            horizontalPadding: 9
                            onClicked: providerDialog.openFor(modelData)
                        }
                        AppButton {
                            visible: modelData.account === true
                            text: "删除"
                            variant: "danger"
                            minWidth: 48
                            horizontalPadding: 9
                            onClicked: {
                                page.pendingDeleteName = modelData.name
                                deleteProviderDialog.open()
                            }
                        }
                    }
                }

                Text {
                    width: parent.width
                    text: modelData.api_base || ""
                    color: Theme.textTertiary
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: Theme.fontSizeTiny
                    elide: Text.ElideMiddle
                }
                Item {
                    width: parent.width
                    height: 22
                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 220
                        text: "当前模型  " + page.modelLabel(modelData)
                        color: Theme.textSecondary
                        font.family: "DejaVu Sans Mono"
                        font.pixelSize: Theme.fontSizeTiny
                        elide: Text.ElideRight
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: page.keyLabel(modelData)
                        color: modelData.has_key === true ? Theme.success : Theme.warning
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                    }
                }
                Text {
                    width: parent.width
                    visible: (modelData.models || []).length > 0
                    text: "已启用 " + (modelData.models || []).length + " 个模型  ·  "
                          + (modelData.models || []).join("  ·  ")
                    color: Theme.textTertiary
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: Theme.fontSizeTiny
                    elide: Text.ElideRight
                }
            }
        }
    }

    Rectangle {
        width: parent.width
        height: emptyColumn.implicitHeight + 30
        visible: !app.providers || app.providers.length === 0
        radius: Theme.radiusMedium
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder
        Column {
            id: emptyColumn
            x: 16
            y: 15
            width: parent.width - 32
            spacing: 8
            Text {
                text: "尚未配置供应商"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.Medium
            }
            Text {
                width: parent.width
                text: "添加供应商后可以先获取模型列表，再保存或切换。配置写入 aish config.yaml。"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
        }
    }

    Text {
        width: parent.width
        text: "切换供应商会重置当前会话；对话记录仍保存在共享的 sessions.db。API Key 不会显示在列表中。"
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeTiny
        lineHeight: 1.4
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    ProviderDialog {
        id: providerDialog
        onSaveRequested: (args, activate) => {
            const result = app.saveProvider(args.name, args.apiBase, args.apiKey,
                                            args.model, args.protocol, activate)
            if (result && result.ok === true) {
                app.setProviderModels(args.apiBase, args.models || [args.model])
                providerDialog.saveSucceeded()
            }
            else
                providerDialog.saveFailed(result && result.error
                                          ? result.error : "供应商保存失败")
        }
    }

    ConfirmDialog {
        id: deleteProviderDialog
        title: "删除供应商"
        message: "确定删除“" + page.pendingDeleteName + "”？"
        detail: "只删除 api_accounts 中的备用账户，不会改动当前主供应商。"
        confirmText: "删除"
        danger: true
        confirmEnabled: !app.generating
        onConfirmed: app.deleteProvider(page.pendingDeleteName)
    }
}
