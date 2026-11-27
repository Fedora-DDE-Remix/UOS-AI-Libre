import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root

    property bool editing: false
    property bool editingPrimary: false
    property string selectedModel: ""
    property string errorText: ""
    property string fetchError: ""
    property var models: []
    property var filteredModels: []
    property var enabledModels: []
    property bool saving: false
    property bool unsupported: false
    signal saveRequested(var args, bool activate)

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    width: Math.min(680, parent ? parent.width - 40 : 680)
    height: Math.min(650, parent ? parent.height - 24 : 650)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    function hostOf(value) {
        const match = String(value || "").trim().match(/^https?:\/\/([^\/:?#]+)/i)
        return match ? match[1].toLowerCase() : ""
    }

    function protocolError() {
        const host = hostOf(baseField.text)
        if (host.length === 0)
            return "请输入 http(s) API 地址"
        if (host === "api.anthropic.com")
            return "第三方接入仅支持 OpenAI 兼容协议，不支持 Anthropic"
        return ""
    }

    function rebuildFilter() {
        const query = filterField.text.trim().toLowerCase()
        if (query.length === 0) {
            filteredModels = models
            return
        }
        const result = []
        for (let i = 0; i < models.length; ++i) {
            const item = models[i]
            if (String(item.id || "").toLowerCase().indexOf(query) >= 0
                || String(item.owned_by || "").toLowerCase().indexOf(query) >= 0)
                result.push(item)
        }
        filteredModels = result
    }

    function openNew() {
        editing = false
        editingPrimary = false
        nameField.text = ""
        baseField.text = ""
        root.unsupported = false
        keyField.text = ""
        keyField.placeholder = "sk-...；新建供应商时填写"
        modelField.text = ""
        selectedModel = ""
        models = []
        filteredModels = []
        enabledModels = []
        filterField.text = ""
        errorText = ""
        fetchError = ""
        saving = false
        open()
        nameField.forceInputFocus()
    }

    function openFor(provider) {
        editing = true
        editingPrimary = provider.account !== true
        nameField.text = provider.name || ""
        baseField.text = provider.api_base || ""
        root.unsupported = String(provider.protocol || "") === "anthropic"
        keyField.text = ""
        keyField.placeholder = "留空保留现有密钥"
        modelField.text = provider.model || ""
        selectedModel = provider.model || ""
        enabledModels = (provider.models || []).slice()
        if (selectedModel.length > 0 && enabledModels.indexOf(selectedModel) < 0)
            enabledModels = enabledModels.concat([selectedModel])
        models = app.cachedProviderModels(provider.api_base || "")
        if (enabledModels.length > 0) {
            const seeded = models.slice()
            for (let i = 0; i < enabledModels.length; ++i) {
                const id = enabledModels[i]
                if (!seeded.some(function (item) { return item.id === id }))
                    seeded.push({ id: id, owned_by: "" })
            }
            models = seeded
        }
        filteredModels = models
        filterField.text = ""
        errorText = root.unsupported
                ? "该供应商使用 Anthropic 协议，不在 GUI 支持范围内；请直接在 aish config.yaml 中维护。"
                : ""
        fetchError = ""
        saving = false
        open()
        baseField.forceInputFocus()
    }

    function applyPreset(name, base) {
        if (nameField.text.trim().length === 0)
            nameField.text = name
        baseField.text = base
        models = []
        filteredModels = []
        fetchError = ""
        errorText = ""
    }

    function isEnabled(id) {
        return enabledModels.indexOf(id) >= 0
    }

    function toggleModel(id) {
        if (isEnabled(id)) {
            const next = enabledModels.slice()
            next.splice(next.indexOf(id), 1)
            enabledModels = next
            if (selectedModel === id)
                selectedModel = enabledModels.length > 0 ? enabledModels[0] : ""
        } else {
            enabledModels = enabledModels.concat([id])
            if (selectedModel.length === 0)
                selectedModel = id
        }
        if (selectedModel.length > 0)
            modelField.text = selectedModel
    }

    function setDefaultModel(id) {
        if (!isEnabled(id))
            enabledModels = enabledModels.concat([id])
        selectedModel = id
        modelField.text = id
    }

    function selectAllModels() {
        const next = []
        for (let i = 0; i < models.length; ++i)
            next.push(models[i].id)
        enabledModels = next
        if (selectedModel.length === 0 && next.length > 0)
            selectedModel = next[0]
        if (selectedModel.length > 0)
            modelField.text = selectedModel
    }

    function clearEnabledModels() {
        enabledModels = []
        selectedModel = ""
        modelField.text = ""
    }

    function fetchModels() {
        errorText = ""
        fetchError = ""
        const invalid = protocolError()
        if (invalid.length > 0) {
            errorText = invalid
            return
        }
        if (app.modelFetchBusy || root.unsupported)
            return
        app.fetchModels(baseField.text.trim(), keyField.text.trim(), "openai")
    }

    function draft() {
        errorText = ""
        const name = nameField.text.trim()
        const base = baseField.text.trim()
        const model = modelField.text.trim()
        if (root.unsupported) {
            errorText = "该供应商使用 Anthropic 协议，不在 GUI 支持范围内"
            return null
        }
        if (name.length === 0) {
            errorText = "请填写供应商显示名称"
            return null
        }
        if (base.length === 0) {
            errorText = "请填写 API 地址"
            return null
        }
        const invalid = protocolError()
        if (invalid.length > 0) {
            errorText = invalid
            return null
        }
        if (model.length === 0) {
            errorText = "请选择或输入默认模型"
            return null
        }
        if (root.enabledModels.length === 0)
            root.enabledModels = [model]
        else if (root.enabledModels.indexOf(model) < 0)
            root.enabledModels = root.enabledModels.concat([model])
        return {
            name: name,
            apiBase: base,
            apiKey: keyField.text,
            model: model,
            models: root.enabledModels,
            protocol: "openai"
        }
    }

    function save(activate) {
        if (saving || root.unsupported)
            return
        const args = draft()
        if (args === null)
            return
        saving = true
        saveRequested(args, activate)
    }

    function saveSucceeded() {
        saving = false
        close()
    }

    function saveFailed(message) {
        saving = false
        errorText = message
    }

    function showLatestError() {
        if (root.errorText.length === 0 && root.fetchError.length === 0)
            return
        Qt.callLater(function () {
            bodyFlick.contentY = Math.max(0, bodyFlick.contentHeight - bodyFlick.height)
        })
    }

    onErrorTextChanged: root.showLatestError()
    onFetchErrorChanged: root.showLatestError()

    Connections {
        target: app
        function onModelFetchFinished(result) {
            if (!root.visible || !result)
                return
            const data = result.data || ({})
            if (String(data.base || "").replace(/\/+$/, "")
                !== baseField.text.trim().replace(/\/+$/, ""))
                return
            if (result.ok === true) {
                root.models = data.models || []
                app.cacheProviderModels(String(data.base || baseField.text).trim(),
                                        root.models)
                root.rebuildFilter()
                if (root.enabledModels.length === 0 && root.models.length > 0) {
                    root.enabledModels = [root.models[0].id]
                    root.selectedModel = root.models[0].id
                    modelField.text = root.selectedModel
                } else if (root.selectedModel.length === 0
                           || !root.models.some(function (item) { return item.id === root.selectedModel })) {
                    if (root.enabledModels.length > 0) {
                        root.selectedModel = root.enabledModels[0]
                        modelField.text = root.selectedModel
                    }
                }
                root.fetchError = root.models.length === 0 ? "端点没有返回模型" : ""
            } else {
                root.fetchError = result.error || "获取模型失败"
            }
        }
    }

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder
    }

    contentItem: Column {
        width: root.width
        height: root.height
        spacing: 0

        Item {
            width: parent.width
            height: 56
            Text {
                x: 22
                anchors.verticalCenter: parent.verticalCenter
                text: root.editing ? "编辑 OpenAI-Compatible 供应商"
                                   : "添加 OpenAI-Compatible 供应商"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
            AppIcon {
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                name: "x"
                size: 14
                color: Theme.icon
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -8
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
            }
        }

        Rectangle { width: parent.width; height: 1; color: Theme.border }

        Flickable {
            id: bodyFlick
            width: parent.width
            height: parent.height - 56 - 1 - 64
            contentWidth: width
            contentHeight: form.implicitHeight + 30
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: form
                x: 22
                y: 14
                width: parent.width - 44
                spacing: 10

                Text {
                    width: parent.width
                    text: "自定义端点写入 aish config.yaml 的主端点或 api_accounts。这里只支持 OpenAI 兼容协议和 /models 模型目录；an​​thropic 请使用“添加供应商”中的内置 Anthropic。"
                    color: Theme.textTertiary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTiny
                    lineHeight: 1.45
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }

                Row {
                    spacing: 6
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "快捷填入"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                    }
                    AppButton { text: "OpenAI"; minWidth: 58; horizontalPadding: 9; onClicked: root.applyPreset("OpenAI", "https://api.openai.com/v1") }
                    AppButton { text: "DeepSeek"; minWidth: 68; horizontalPadding: 9; onClicked: root.applyPreset("DeepSeek", "https://api.deepseek.com") }
                }

                Column {
                    width: parent.width
                    spacing: 5
                    Text { text: "供应商显示名称"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    AppTextField { id: nameField; width: parent.width; placeholder: "例如 DeepSeek 生产环境" }
                    Text {
                        width: parent.width
                        text: "只影响 GUI 显示，不改变 aish 的协议或模型语义。"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                    }
                }

                Column {
                    width: parent.width
                    spacing: 5
                    Text { text: "API 地址"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    AppTextField {
                        id: baseField
                        width: parent.width
                        placeholder: "https://api.deepseek.com 或 https://api.openai.com/v1"
                        onTextEdited: {
                            root.models = []
                            root.filteredModels = []
                            root.fetchError = ""
                            root.errorText = ""
                        }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 5
                    Text { text: "API Key"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    AppTextField { id: keyField; width: parent.width; password: true; placeholder: "sk-...；编辑时留空保留现有密钥" }
                    Text {
                        width: parent.width
                        text: "密钥只写入本机 aish config.yaml，不会显示在日志或模型列表中。"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                    }
                }

                Column {
                    width: parent.width
                    spacing: 5
                    Row {
                        width: parent.width
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "模型"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTiny
                        }
                        Item { width: Math.max(0, parent.width - 130); height: 1 }
                        AppButton {
                            text: app.modelFetchBusy ? "获取中…" : "获取模型列表"
                            minWidth: 100
                            horizontalPadding: 9
                            enabled: !app.modelFetchBusy && !root.unsupported
                                     && baseField.text.trim().length > 0
                            onClicked: root.fetchModels()
                        }
                    }
                    AppTextField {
                        id: modelField
                        width: parent.width
                        placeholder: "先获取模型列表，或手动输入模型 ID"
                        onTextEdited: (value) => root.selectedModel = value
                    }
                    Text {
                        width: parent.width
                        visible: root.fetchError.length > 0
                        text: root.fetchError
                        color: Theme.danger
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                }

                Column {
                    width: parent.width
                    spacing: 5
                    visible: root.models.length > 0
                    Item {
                        width: parent.width
                        height: 24
                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            text: "已启用 " + root.enabledModels.length + " / "
                                  + root.models.length + " 个模型"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTiny
                        }
                        Row {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 6
                            AppButton {
                                text: "全选"
                                minWidth: 44
                                horizontalPadding: 8
                                onClicked: root.selectAllModels()
                            }
                            AppButton {
                                text: "清空"
                                minWidth: 44
                                horizontalPadding: 8
                                onClicked: root.clearEnabledModels()
                            }
                        }
                    }
                    AppTextField {
                        id: filterField
                        width: parent.width
                        placeholder: "筛选模型"
                        onTextEdited: (value) => root.rebuildFilter()
                    }
                    Rectangle {
                        width: parent.width
                        height: Math.min(modelList.implicitHeight + 16, 230)
                        radius: Theme.radiusSmall
                        color: Theme.codeBg
                        border.width: 1
                        border.color: Theme.border
                        clip: true
                        Flickable {
                            anchors.fill: parent
                            anchors.margins: 8
                            contentWidth: width
                            contentHeight: modelList.implicitHeight
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                            Column {
                                id: modelList
                                width: parent.width
                                spacing: 3
                                Repeater {
                                    model: root.filteredModels
                                    delegate: Rectangle {
                                        width: parent.width
                                        height: 34
                                        radius: Theme.radiusSmall
                                        color: root.isEnabled(modelData.id)
                                               ? Theme.accentSoft
                                               : (modelHover.hovered ? Theme.hover : "transparent")
                                        HoverHandler { id: modelHover }

                                        Rectangle {
                                            x: 8
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 16
                                            height: 16
                                            radius: 4
                                            color: root.isEnabled(modelData.id)
                                                   ? Theme.accent : "transparent"
                                            border.width: 1
                                            border.color: root.isEnabled(modelData.id)
                                                          ? Theme.accent : Theme.borderStrong
                                            AppIcon {
                                                visible: root.isEnabled(modelData.id)
                                                anchors.centerIn: parent
                                                name: "check"
                                                size: 10
                                                color: "#FFFFFF"
                                            }
                                            MouseArea {
                                                anchors.fill: parent
                                                anchors.margins: -4
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.toggleModel(modelData.id)
                                            }
                                        }

                                        Text {
                                            x: 34
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: parent.width - 170
                                            text: modelData.id
                                            color: root.isEnabled(modelData.id)
                                                   ? Theme.textPrimary : Theme.textSecondary
                                            font.family: "DejaVu Sans Mono"
                                            font.pixelSize: Theme.fontSizeTiny
                                            elide: Text.ElideRight
                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.setDefaultModel(modelData.id)
                                            }
                                        }

                                        Text {
                                            visible: root.selectedModel === modelData.id
                                            anchors.right: ownedText.left
                                            anchors.rightMargin: 6
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: "默认"
                                            color: Theme.accent
                                            font.family: Theme.fontFamily
                                            font.pixelSize: 9
                                        }
                                        Text {
                                            id: ownedText
                                            anchors.right: parent.right
                                            anchors.rightMargin: 8
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 80
                                            horizontalAlignment: Text.AlignRight
                                            text: modelData.owned_by || ""
                                            color: Theme.textTertiary
                                            font.family: Theme.fontFamily
                                            font.pixelSize: 9
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        text: root.filteredModels.length + " 个匹配 · 点击复选框启用，点击模型名设为默认"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                    }
                }

                Text {
                    width: parent.width
                    text: "切换供应商会写入 aish 配置并重建当前后端会话；对话记录仍保存在 sessions.db。"
                    color: Theme.textTertiary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTiny
                    lineHeight: 1.4
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }

                Text {
                    width: parent.width
                    visible: root.errorText.length > 0
                    text: root.errorText
                    color: Theme.danger
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
            }
        }

        Rectangle { width: parent.width; height: 1; color: Theme.border }

        Item {
            width: parent.width
            height: 64
            Row {
                anchors.right: parent.right
                anchors.rightMargin: 22
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8
                AppButton { text: "取消"; onClicked: root.close() }
                AppButton {
                    visible: !root.editingPrimary
                    text: root.saving ? "保存中…" : "仅保存"
                    enabled: !root.saving && !app.generating && !root.unsupported
                    onClicked: root.save(false)
                }
                AppButton {
                    text: root.saving ? "保存中…" : "保存"
                    variant: "primary"
                    enabled: !root.saving && !app.generating && !root.unsupported
                    onClicked: root.save(true)
                }
            }
        }
    }

}
