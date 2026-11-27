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

    function normalizeBase(value) {
        return String(value || "").trim().replace(/\/+$/, "")
    }

    function builtinPresetFor(base) {
        const providers = app.builtinProviders() || []
        const wanted = normalizeBase(base)
        for (let i = 0; i < providers.length; ++i) {
            if (normalizeBase(providers[i].api_base) === wanted)
                return providers[i]
        }
        return null
    }

    function providerTypeLabel(provider) {
        return builtinPresetFor(provider.api_base) ? "内置" : "OpenAI-Compatible"
    }

    function editProvider(provider) {
        const preset = builtinPresetFor(provider.api_base)
        if (preset)
            builtinProviderDialog.openFor(provider, preset)
        else
            providerDialog.openFor(provider)
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
            editProvider(provider)
            return
        }
        const result = app.saveProvider(provider.name, provider.api_base, "",
                                        provider.model, provider.protocol, true)
        if (!result.ok)
            app.showToast(result.error || "切换供应商失败")
    }

    function commitProvider(args, activate) {
        const result = app.saveProvider(args.name, args.apiBase, args.apiKey,
                                        args.model, args.protocol, activate)
        if (result && result.ok === true) {
            app.setProviderModels(args.apiBase, args.models || [args.model])
            return true
        }
        return result || ({ ok: false, error: "供应商保存失败" })
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
        text: "内置供应商使用 aish 预设地址，只需填写 API Key；OpenAI-Compatible 单独配置地址和模型目录。切换后会重写 aish config.yaml 并重建会话。"
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
                onClicked: providerPicker.open()
            }
        }
    }

    Text {
        width: parent.width
        text: "内置供应商的协议由 aish 预设决定（Anthropic 仅支持官方 api.anthropic.com）。自定义 OpenAI-Compatible 端点需要提供 /models 模型目录。"
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
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: page.providerTypeLabel(modelData)
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
                            onClicked: page.editProvider(modelData)
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

    ProviderPickerDialog {
        id: providerPicker
        onBuiltinSelected: (preset) => {
            providerPicker.close()
            builtinProviderDialog.openNew(preset)
        }
        onCustomSelected: providerDialog.openNew()
    }

    ProviderDialog {
        id: providerDialog
        onSaveRequested: (args, activate) => {
            const result = page.commitProvider(args, activate)
            if (result && result.ok === true)
                providerDialog.saveSucceeded()
            else
                providerDialog.saveFailed(result && result.error
                                          ? result.error : "供应商保存失败")
        }
    }

    BuiltinProviderDialog {
        id: builtinProviderDialog
        onSaveRequested: (args, activate) => {
            const result = page.commitProvider(args, activate)
            if (result && result.ok === true)
                builtinProviderDialog.saveSucceeded()
            else
                builtinProviderDialog.saveFailed(result && result.error
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
