import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root

    property var preset: ({})
    property string displayName: ""
    property bool editing: false
    property bool editingPrimary: false
    property string selectedModel: ""
    property var models: []
    property var filteredModels: []
    property var enabledModels: []
    property bool saving: false
    property string errorText: ""
    property string fetchError: ""
    signal saveRequested(var args, bool activate)

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    width: Math.min(620, parent ? parent.width - 40 : 620)
    height: Math.min(620, parent ? parent.height - 24 : 620)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    function seedModels(provider) {
        const catalog = app.cachedProviderModels(provider.api_base || preset.api_base || "")
        const seeded = (provider.models || []).slice()
        const result = catalog.slice()
        for (let i = 0; i < seeded.length; ++i) {
            const id = String(seeded[i] || "").trim()
            if (id.length > 0
                && !result.some(function (item) { return item.id === id }))
                result.push({ id: id, owned_by: preset.label || "" })
        }
        if (root.selectedModel.length > 0
            && !result.some(function (item) { return item.id === root.selectedModel }))
            result.push({ id: root.selectedModel, owned_by: preset.label || "" })
        if (result.length === 0 && root.selectedModel.length > 0)
            result.push({ id: root.selectedModel, owned_by: preset.label || "" })
        return result
    }

    function openNew(nextPreset) {
        preset = nextPreset || ({})
        displayName = preset.label || preset.key || "供应商"
        editing = false
        editingPrimary = false
        selectedModel = String(preset.default_model || "")
        enabledModels = selectedModel.length > 0 ? [selectedModel] : []
        models = seedModels({})
        filteredModels = models
        keyField.text = ""
        keyField.placeholder = preset.requires_key === false
                ? "本地端点可留空" : "sk-...；保存后写入 aish config.yaml"
        modelField.text = selectedModel
        filterField.text = ""
        errorText = ""
        fetchError = ""
        saving = false
        open()
        keyField.forceInputFocus()
    }

    function openFor(provider, nextPreset) {
        preset = nextPreset || ({})
        displayName = provider.name || preset.label || preset.key || "供应商"
        editing = true
        editingPrimary = provider.account !== true
        selectedModel = String(provider.model || "")
        enabledModels = (provider.models || []).slice()
        if (selectedModel.length > 0 && enabledModels.indexOf(selectedModel) < 0)
            enabledModels = enabledModels.concat([selectedModel])
        models = seedModels(provider)
        filteredModels = models
        keyField.text = ""
        keyField.placeholder = "留空保留现有密钥"
        modelField.text = selectedModel
        filterField.text = ""
        errorText = ""
        fetchError = ""
        saving = false
        open()
        keyField.forceInputFocus()
    }

    function isEnabled(id) {
        return enabledModels.indexOf(id) >= 0
    }

    function setDefaultModel(id) {
        if (!isEnabled(id))
            enabledModels = enabledModels.concat([id])
        selectedModel = id
        modelField.text = id
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

    function fetchModels() {
        if (app.modelFetchBusy)
            return
        if (preset.requires_key !== false && keyField.text.trim().length === 0
            && !editing) {
            errorText = "请先填写 API Key"
            return
        }
        errorText = ""
        fetchError = ""
        app.fetchModels(preset.api_base, keyField.text.trim(), preset.protocol)
    }

    function draft() {
        errorText = ""
        if (!preset || !preset.api_base) {
            errorText = "内置供应商配置无效"
            return null
        }
        if (preset.requires_key !== false && keyField.text.trim().length === 0
            && !editing) {
            errorText = "请填写 API Key"
            return null
        }
        const model = modelField.text.trim()
        if (model.length === 0) {
            errorText = "请选择或输入默认模型"
            return null
        }
        if (root.enabledModels.length === 0)
            root.enabledModels = [model]
        else if (root.enabledModels.indexOf(model) < 0)
            root.enabledModels = root.enabledModels.concat([model])
        return {
            name: root.displayName || root.preset.label || root.preset.key,
            apiBase: preset.api_base,
            apiKey: keyField.text,
            model: model,
            models: root.enabledModels,
            protocol: preset.protocol || "openai"
        }
    }

    function save(activate) {
        if (saving)
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

    Connections {
        target: app
        function onModelFetchFinished(result) {
            if (!root.visible || !result)
                return
            const data = result.data || ({})
            if (String(data.base || "").replace(/\/+$/, "")
                !== String(preset.api_base || "").replace(/\/+$/, ""))
                return
            if (result.ok === true) {
                root.models = data.models || []
                app.cacheProviderModels(String(data.base || preset.api_base).trim(),
                                        root.models)
                root.rebuildFilter()
                if (root.enabledModels.length === 0 && root.models.length > 0) {
                    root.enabledModels = [root.models[0].id]
                    root.selectedModel = root.models[0].id
                    root.modelField.text = root.selectedModel
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
                text: (root.editing ? "编辑 " : "添加 ")
                      + (root.displayName || root.preset.label || "供应商")
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

                Rectangle {
                    width: parent.width
                    height: 42
                    radius: Theme.radiusSmall
                    color: Theme.codeBg
                    border.width: 1
                    border.color: Theme.border
                    Text {
                        x: 12
                        y: 6
                        width: parent.width - 24
                        text: root.preset.label || "内置供应商"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.weight: Font.Medium
                    }
                    Text {
                        x: 12
                        y: 24
                        width: parent.width - 24
                        text: root.preset.api_base || ""
                        color: Theme.textTertiary
                        font.family: "DejaVu Sans Mono"
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                    }
                }

                Column {
                    width: parent.width
                    spacing: 5
                    Text {
                        text: "API Key"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                    }
                    AppTextField {
                        id: keyField
                        width: parent.width
                        password: true
                    }
                    Text {
                        width: parent.width
                        text: "内置供应商的地址和协议由 aish 预设；只需要填写 API Key。"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
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
                            enabled: !app.modelFetchBusy
                            onClicked: root.fetchModels()
                        }
                    }
                    AppTextField {
                        id: modelField
                        width: parent.width
                        placeholder: "默认模型"
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
                    text: root.saving ? "保存中…" : "保存为备用"
                    enabled: !root.saving && !app.generating
                    onClicked: root.save(false)
                }
                AppButton {
                    text: root.saving ? "保存中…" : "保存"
                    variant: "primary"
                    enabled: !root.saving && !app.generating
                    onClicked: root.save(true)
                }
            }
        }
    }
}
