import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root
    property var allRules: []
    property int editIndex: -1
    property string errorText: ""
    property var selectedOperations: []
    property bool saving: false
    signal saveRequested(var rule)

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    width: Math.min(640, parent ? parent.width - 48 : 640)
    height: Math.min(590, parent ? parent.height - 32 : 590)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    function splitLines(text) {
        return String(text || "").split("\n")
            .map(function (line) { return line.trim() })
            .filter(function (line) { return line.length > 0 })
    }

    function splitCommas(text) {
        return String(text || "").split(/[,，]/)
            .map(function (item) { return item.trim() })
            .filter(function (item) { return item.length > 0 })
    }

    function joinLines(value) {
        if (!value)
            return ""
        if (Array.isArray(value))
            return value.join("\n")
        return String(value)
    }

    function joinCommas(value) {
        if (!value)
            return ""
        if (Array.isArray(value))
            return value.join(", ")
        return String(value)
    }

    function openFor(rule, index) {
        editIndex = index === undefined ? -1 : index
        const data = rule || ({})
        idField.text = data.id || ""
        nameField.text = data.name || ""
        riskChoice.value = String(data.risk || "MEDIUM").toUpperCase()
        commandField.text = joinCommas(data.command_list)
        pathArea.text = joinLines(data.path)
        excludeArea.text = joinLines(data.exclude)
        reasonArea.text = data.reason || ""
        confirmArea.text = data.confirm_message || ""
        suggestionArea.text = data.suggestion || ""
        selectedOperations = (data.operations || []).slice()
        errorText = ""
        saving = false
        previewCommand.text = ""
        previewResult.text = ""
        open()
    }

    function toggleOperation(operation) {
        const next = selectedOperations.slice()
        const at = next.indexOf(operation)
        if (at >= 0)
            next.splice(at, 1)
        else
            next.push(operation)
        selectedOperations = next
    }

    function draftRule() {
        const rule = {
            id: idField.text.trim(),
            name: nameField.text.trim(),
            risk: riskChoice.value,
            path: splitLines(pathArea.text),
            command_list: splitCommas(commandField.text),
            operations: selectedOperations,
            exclude: splitLines(excludeArea.text),
            reason: reasonArea.text.trim(),
            confirm_message: confirmArea.text.trim(),
            suggestion: suggestionArea.text.trim()
        }
        if (rule.id.length === 0)
            delete rule.id
        if (rule.name.length === 0)
            delete rule.name
        if (rule.command_list.length === 0)
            delete rule.command_list
        if (rule.operations.length === 0)
            delete rule.operations
        if (rule.exclude.length === 0)
            delete rule.exclude
        if (rule.reason.length === 0)
            delete rule.reason
        if (rule.confirm_message.length === 0)
            delete rule.confirm_message
        if (rule.suggestion.length === 0)
            delete rule.suggestion
        return rule
    }

    function preview() {
        const command = previewCommand.text.trim()
        if (command.length === 0) {
            previewResult.text = "请输入一条示例命令"
            return
        }
        const rules = JSON.parse(JSON.stringify(allRules || []))
        if (editIndex >= 0 && editIndex < rules.length)
            rules[editIndex] = draftRule()
        else
            rules.push(draftRule())
        const result = app.previewPolicy(command, rules)
        if (!result || result.ok !== true) {
            previewResult.text = result && result.error ? result.error : "预览失败"
            return
        }
        const data = result.preview || ({})
        if (data.matched === true) {
            const rule = data.rule || ({})
            previewResult.text = "命中 " + (rule.id || "规则") + " · " + (rule.name || "")
                    + " → " + String(data.risk || "").toUpperCase()
        } else {
            previewResult.text = "未命中 fallback 规则 → 默认 "
                    + String(data.default_risk || "").toUpperCase()
        }
    }

    function save() {
        const rule = draftRule()
        if (rule.path.length === 0) {
            errorText = "至少填写一个路径 glob"
            return
        }
        if (["LOW", "MEDIUM", "HIGH"].indexOf(rule.risk) < 0) {
            errorText = "风险等级无效"
            return
        }
        errorText = ""
        saving = true
        saveRequested(rule)
    }

    function saveSucceeded() {
        saving = false
        close()
    }

    function saveFailed(message) {
        saving = false
        errorText = message
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
            height: 54
            Text {
                x: 22
                anchors.verticalCenter: parent.verticalCenter
                text: root.editIndex >= 0 ? "编辑安全规则" : "新建安全规则"
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

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.border
        }

        Flickable {
            width: parent.width
            height: parent.height - 54 - 1 - 62
            contentWidth: width
            contentHeight: formColumn.implicitHeight + 32
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: formColumn
                x: 22
                y: 16
                width: parent.width - 44
                spacing: 11

                Text {
                    width: parent.width
                    text: "规则按顺序匹配，第一条命中决定 LOW / MEDIUM / HIGH。HIGH 会直接阻断。"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    lineHeight: 1.4
                }

                Row {
                    width: parent.width
                    spacing: 10
                    Column {
                        width: (parent.width - 10) / 2
                        spacing: 5
                        Text { text: "规则 ID"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                        AppTextField { id: idField; width: parent.width; placeholder: "例如 U-001" }
                    }
                    Column {
                        width: (parent.width - 10) / 2
                        spacing: 5
                        Text { text: "名称"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                        AppTextField { id: nameField; width: parent.width; placeholder: "例如 保护项目数据" }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 5
                    Text { text: "风险等级"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    SettingChoice {
                        id: riskChoice
                        options: [{value: "LOW", label: "LOW · 直接执行"},
                                  {value: "MEDIUM", label: "MEDIUM · 确认"},
                                  {value: "HIGH", label: "HIGH · 阻断"}]
                    }
                }

                Column {
                    width: parent.width
                    spacing: 5
                    Text { text: "命令（逗号分隔）"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    AppTextField { id: commandField; width: parent.width; placeholder: "rm, cp, mv" }
                    Text {
                        width: parent.width
                        text: "aish 的 fallback 只评估这里声明过的命令；留空时该规则只对其它规则已声明的命令生效。"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                    Text {
                        width: parent.width
                        visible: commandField.text.trim().length === 0
                        text: "警告：未填写命令时，这条规则在沙箱关闭时很可能不会命中任何命令。"
                        color: Theme.warning
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                }

                Column {
                    width: parent.width
                    spacing: 5
                    Text { text: "路径 glob（每行一个）"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    AppTextArea { id: pathArea; width: parent.width; minHeight: 70; placeholder: "/srv/project/**\n/data/**" }
                }

                Column {
                    width: parent.width
                    spacing: 5
                    Text { text: "排除路径（每行一个，可选）"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    AppTextArea { id: excludeArea; width: parent.width; minHeight: 52; placeholder: "/srv/project/cache/**" }
                }

                Column {
                    width: parent.width
                    spacing: 6
                    Text { text: "操作（仅沙箱评估使用）"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    Row {
                        spacing: 6
                        Repeater {
                            model: ["WRITE", "DELETE"]
                            delegate: Rectangle {
                                width: opLabel.implicitWidth + 20
                                height: 26
                                radius: Theme.radiusSmall
                                color: root.selectedOperations.indexOf(modelData) >= 0
                                       ? Theme.accentSoft : "transparent"
                                border.width: 1
                                border.color: root.selectedOperations.indexOf(modelData) >= 0
                                              ? Theme.accent : Theme.border
                                Text {
                                    id: opLabel
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: root.selectedOperations.indexOf(modelData) >= 0
                                           ? Theme.accent : Theme.textSecondary
                                    font.family: "DejaVu Sans Mono"
                                    font.pixelSize: Theme.fontSizeTiny
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.toggleOperation(modelData)
                                }
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        text: "沙箱关闭时 operations 不参与 fallback 命中，只保留给沙箱评估。"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                }

                Column {
                    width: parent.width
                    spacing: 5
                    Text { text: "风险原因"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    AppTextArea { id: reasonArea; width: parent.width; minHeight: 52; placeholder: "审批弹窗里展示给用户的原因" }
                }

                Row {
                    width: parent.width
                    spacing: 10
                    Column {
                        width: (parent.width - 10) / 2
                        spacing: 5
                        Text { text: "确认文案（可选）"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                        AppTextArea { id: confirmArea; width: parent.width; minHeight: 48 }
                    }
                    Column {
                        width: (parent.width - 10) / 2
                        spacing: 5
                        Text { text: "建议（可选）"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                        AppTextArea { id: suggestionArea; width: parent.width; minHeight: 48 }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.border
                }

                Column {
                    width: parent.width
                    spacing: 6
                    Text { text: "示例命令试运行"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    Row {
                        width: parent.width
                        spacing: 8
                        AppTextField {
                            id: previewCommand
                            width: parent.width - 86
                            placeholder: "例如 rm -rf /srv/project/data"
                            onAccepted: root.preview()
                        }
                        AppButton { text: "试运行"; onClicked: root.preview() }
                    }
                    Text {
                        id: previewResult
                        width: parent.width
                        color: Theme.textSecondary
                        font.family: "DejaVu Sans Mono"
                        font.pixelSize: Theme.fontSizeTiny
                        wrapMode: Text.WrapAnywhere
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

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.border
        }

        Item {
            width: parent.width
            height: 62
            Row {
                anchors.right: parent.right
                anchors.rightMargin: 22
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8
                AppButton { text: "取消"; onClicked: root.close() }
                AppButton {
                    text: root.saving ? "保存中…"
                                      : (root.editIndex >= 0 ? "保存修改" : "添加规则")
                    variant: "primary"
                    enabled: !root.saving && !app.generating
                    onClicked: root.save()
                }
            }
        }
    }
}
