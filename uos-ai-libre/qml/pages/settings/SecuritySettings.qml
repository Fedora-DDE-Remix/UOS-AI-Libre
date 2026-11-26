import QtQuick
import QtQuick.Controls
import UosAiLibre

SettingsScroll {
    id: page

    property bool dirty: false
    property string defaultRisk: "low"
    property bool sandboxEnabled: false
    property string sandboxOffAction: "allow"
    property string sandboxTimeout: "10"
    property bool inputGuardEnabled: true
    property string previewCommand: ""
    property string previewText: ""
    property int pendingDeleteIndex: -1
    property string pendingDeleteName: ""

    Component.onCompleted: load()
    onVisibleChanged: {
        app.refreshPolicy()
        app.refreshSessionApprovals()
        if (!dirty)
            load()
    }

    Connections {
        target: app
        function onPolicyInfoChanged() {
            if (!page.dirty)
                page.load()
        }
    }

    function load() {
        const info = app.policyInfo
        if (!info || info.default_risk_level === undefined)
            return
        defaultRisk = info.default_risk_level || "low"
        sandboxEnabled = info.enable_sandbox === true
        sandboxOffAction = info.sandbox_off_action || "allow"
        sandboxTimeout = String(info.sandbox_timeout_seconds !== undefined
                               ? info.sandbox_timeout_seconds : 10)
        inputGuardEnabled = info.input_guard_enabled !== false
        dirty = false
    }

    function markDirty() { page.dirty = true }

    function riskColor(risk) {
        const value = String(risk || "").toUpperCase()
        if (value === "HIGH" || value === "DANGER")
            return Theme.danger
        if (value === "MEDIUM")
            return Theme.warning
        return Theme.success
    }

    function rawRules() {
        return JSON.parse(JSON.stringify(app.policyInfo.rule_groups || []))
    }

    function pathsOf(rule) {
        if (!rule)
            return []
        if (Array.isArray(rule.path))
            return rule.path
        if (rule.path)
            return [String(rule.path)]
        return []
    }

    function commandsOf(rule) {
        return (rule && rule.command_list) ? rule.command_list : []
    }

    function nextRuleId(rules) {
        let max = 0
        for (let i = 0; i < rules.length; ++i) {
            const id = String(rules[i].id || "")
            const match = id.match(/(\d+)$/)
            if (match)
                max = Math.max(max, Number(match[1]))
        }
        return "U-" + String(max + 1).padStart(3, "0")
    }

    function saveRules(next) {
        const result = app.savePolicyRules(next)
        // Do not call load() here: policyInfoChanged already refreshes the
        // rule list, and load() would silently discard unsaved global-field
        // edits (default risk, sandbox, timeout, InputGuard).
        if (result && result.ok === true && previewCommand.length > 0)
            page.runPreview()
        return result
    }

    function runPreview() {
        const command = previewCommand.trim()
        if (command.length === 0) {
            previewText = "请输入一条示例命令"
            return
        }
        const result = app.previewPolicy(command, page.rawRules())
        if (!result || result.ok !== true) {
            previewText = result && result.error ? result.error : "预览失败"
            return
        }
        const data = result.preview || ({})
        if (data.matched === true) {
            const rule = data.rule || ({})
            previewText = "命中 " + (rule.id || "规则") + " · " + (rule.name || "")
                    + " → " + String(data.risk || "").toUpperCase()
        } else {
            previewText = "未命中 fallback 规则 → 默认 "
                    + String(data.default_risk || "").toUpperCase()
        }
    }

    function moveRule(index, delta) {
        const rules = page.rawRules()
        const target = index + delta
        if (index < 0 || target < 0 || target >= rules.length)
            return
        const item = rules[index]
        rules[index] = rules[target]
        rules[target] = item
        page.saveRules(rules)
    }

    function deleteRule(index) {
        const rules = page.rawRules()
        if (index < 0 || index >= rules.length)
            return
        rules.splice(index, 1)
        page.saveRules(rules)
    }

    // ---------------------------------------------------------------- header

    Text {
        text: "安全与权限"
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: 18
        font.weight: Font.DemiBold
    }
    Text {
        width: parent.width
        text: "权限完全由 aish 的 security_policy.yaml 决定。这里不提供 GUI 自造的“只读/完全访问”模式，也不维护第二份权限状态。"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Rectangle {
        width: parent.width
        height: legendColumn.implicitHeight + 28
        radius: Theme.radiusMedium
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder

        Column {
            id: legendColumn
            x: 16
            y: 14
            width: parent.width - 32
            spacing: 9

            Row {
                width: parent.width
                spacing: 8
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "风险如何决定"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.Medium
                }
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 86
                    height: 20
                    radius: 4
                    color: Theme.codeBg
                    Text {
                        anchors.centerIn: parent
                        text: "按顺序匹配"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: 9
                    }
                }
            }
            Text {
                width: parent.width
                text: "LOW 直接执行 · MEDIUM 弹窗确认 · HIGH 阻断。规则按文件顺序匹配，第一条命中决定风险；未命中时使用默认等级。"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                lineHeight: 1.45
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Text {
                width: parent.width
                text: "提示：路径规则用于 bash 命令的 fallback 评估；原生 write_file / edit_file 不在该规则内，它们由快照/undo 保障。"
                color: Theme.textTertiary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTiny
                lineHeight: 1.45
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
        }
    }

    // --------------------------------------------------------------- policy

    Rectangle {
        width: parent.width
        height: policyColumn.implicitHeight + 30
        radius: Theme.radiusMedium
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder

        Column {
            id: policyColumn
            x: 16
            y: 14
            width: parent.width - 32
            spacing: 12

            Row {
                width: parent.width
                Text {
                    text: "策略文件"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.Medium
                }
                Item { width: Math.max(0, parent.width - 110); height: 1 }
                Text {
                    text: "打开 YAML"
                    color: Theme.accent
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: app.openPath(app.policyInfo.path || "")
                    }
                }
            }
            Text {
                width: parent.width
                text: app.policyInfo.path || "aish 默认策略文件"
                color: Theme.textTertiary
                font.family: "DejaVu Sans Mono"
                font.pixelSize: Theme.fontSizeTiny
                elide: Text.ElideMiddle
            }

            Rectangle { width: parent.width; height: 1; color: Theme.border }

            Row {
                width: parent.width
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "默认风险等级"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
                Item { width: Math.max(0, parent.width - 250); height: 1 }
                SettingChoice {
                    anchors.verticalCenter: parent.verticalCenter
                    options: [{value: "low", label: "低"},
                              {value: "medium", label: "中"},
                              {value: "high", label: "高"}]
                    value: page.defaultRisk
                    onChanged: (value) => { page.defaultRisk = value; page.markDirty() }
                }
            }

            Row {
                width: parent.width
                Column {
                    spacing: 3
                    Text {
                        text: "沙箱预跑"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }
                    Text {
                        text: app.policyInfo.sandbox_socket_available
                              ? "aish-sandbox socket 可用"
                              : "aish-sandbox socket 不可用（需要启用系统服务）"
                        color: app.policyInfo.sandbox_socket_available
                               ? Theme.success : Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                    }
                }
                Item { width: Math.max(0, parent.width - 300); height: 1 }
                ToggleSwitch {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: page.sandboxEnabled
                    onToggled: (value) => { page.sandboxEnabled = value; page.markDirty() }
                }
            }

            Row {
                width: parent.width
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "沙箱不可用时"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
                Item { width: Math.max(0, parent.width - 250); height: 1 }
                SettingChoice {
                    anchors.verticalCenter: parent.verticalCenter
                    options: [{value: "allow", label: "放行"},
                              {value: "confirm", label: "确认"},
                              {value: "block", label: "阻断"}]
                    value: page.sandboxOffAction
                    onChanged: (value) => { page.sandboxOffAction = value; page.markDirty() }
                }
            }

            Row {
                width: parent.width
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "沙箱超时（秒）"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
                Item { width: Math.max(0, parent.width - 170); height: 1 }
                AppTextField {
                    width: 90
                    height: 32
                    text: page.sandboxTimeout
                    placeholder: "10"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    onTextChanged: page.markDirty()
                }
            }

            Row {
                width: parent.width
                Column {
                    spacing: 3
                    Text {
                        text: "输入防护（InputGuard）"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }
                    Text {
                        text: "拦截破坏性 rm/dd/mkfs、sudo、ssh 等高风险输入"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                    }
                }
                Item { width: Math.max(0, parent.width - 300); height: 1 }
                ToggleSwitch {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: page.inputGuardEnabled
                    onToggled: (value) => { page.inputGuardEnabled = value; page.markDirty() }
                }
            }

            Row {
                spacing: 8
                AppButton {
                    text: "保存"
                    variant: "primary"
                    enabled: page.dirty
                    onClicked: {
                        const result = app.savePolicy({
                            default_risk_level: page.defaultRisk,
                            enable_sandbox: page.sandboxEnabled,
                            sandbox_off_action: page.sandboxOffAction,
                            sandbox_timeout_seconds: Number(page.sandboxTimeout),
                            input_guard_enabled: page.inputGuardEnabled
                        })
                        if (result && result.ok) {
                            page.dirty = false
                            page.load()
                        }
                    }
                }
                AppButton {
                    text: "刷新"
                    onClicked: app.refreshPolicy()
                }
            }
        }
    }

    // ---------------------------------------------------------------- rules

    Item {
        width: parent.width
        height: 26
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "安全规则"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.Medium
        }
        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            AppButton {
                text: "新建规则"
                variant: "primary"
                onClicked: {
                    ruleDialog.allRules = page.rawRules()
                    ruleDialog.openFor({}, -1)
                }
            }
        }
    }

    Text {
        width: parent.width
        text: "保存规则会原子重写 security_policy.yaml：注释会丢失，未知字段保留；保存前会校验，非法规则不会落盘。"
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeTiny
        lineHeight: 1.45
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Text {
        width: parent.width
        visible: (app.policyInfo.rules_error || "").length > 0
        text: "规则读取失败：" + (app.policyInfo.rules_error || "")
        color: Theme.danger
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Text {
        width: parent.width
        visible: (app.policyInfo.validation_issues || []).length > 0
              || (app.policyInfo.invalid_fallback_rules || []).length > 0
        text: {
            const invalid = (app.policyInfo.invalid_fallback_rules || []).length
            const issues = (app.policyInfo.validation_issues || []).length
            return "注意：策略中有 " + invalid + " 条无效 fallback 规则、"
                    + issues + " 个校验问题。无效规则会被 aish 忽略，请打开 YAML 检查。"
        }
        color: Theme.warning
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Text {
        width: parent.width
        text: "内置保护规则（只读，始终生效）"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        font.weight: Font.Medium
    }

    Column {
        width: parent.width
        spacing: 6
        Repeater {
            model: app.policyInfo.builtin_rules || []
            delegate: Rectangle {
                width: parent.width
                height: builtinColumn.implicitHeight + 20
                radius: Theme.radiusSmall
                color: Theme.codeBg
                border.width: 1
                border.color: Theme.border
                Column {
                    id: builtinColumn
                    x: 12
                    y: 8
                    width: parent.width - 24
                    spacing: 3
                    Row {
                        width: parent.width
                        spacing: 8
                        Text {
                            text: modelData.id || modelData.rule_id || "builtin"
                            color: Theme.textPrimary
                            font.family: "DejaVu Sans Mono"
                            font.pixelSize: Theme.fontSizeTiny
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: String(modelData.risk || "HIGH").toUpperCase()
                            color: page.riskColor(modelData.risk)
                            font.family: "DejaVu Sans Mono"
                            font.pixelSize: 9
                        }
                        Text {
                            width: parent.width - 150
                            text: modelData.name || ""
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTiny
                            elide: Text.ElideRight
                        }
                    }
                    Text {
                        width: parent.width
                        text: modelData.pattern || ""
                        color: Theme.textTertiary
                        font.family: "DejaVu Sans Mono"
                        font.pixelSize: Theme.fontSizeTiny
                        elide: Text.ElideMiddle
                    }
                    Text {
                        width: parent.width
                        visible: modelData.reason !== undefined
                                 && String(modelData.reason).length > 0
                        text: modelData.reason || ""
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                }
            }
        }
    }

    Text {
        text: "用户规则"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        font.weight: Font.Medium
    }

    Column {
        width: parent.width
        spacing: 8

        Repeater {
            model: app.policyInfo.rule_groups || []
            delegate: Rectangle {
                width: parent.width
                height: ruleColumn.implicitHeight + 26
                radius: Theme.radiusMedium
                color: Theme.card
                border.width: 1
                border.color: Theme.cardBorder

                Column {
                    id: ruleColumn
                    x: 14
                    y: 12
                    width: parent.width - 28
                    spacing: 5

                    Row {
                        width: parent.width
                        spacing: 8
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.id || "未命名规则"
                            color: Theme.textPrimary
                            font.family: "DejaVu Sans Mono"
                            font.pixelSize: Theme.fontSizeTiny
                            font.weight: Font.DemiBold
                        }
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: riskText.implicitWidth + 12
                            height: 18
                            radius: 4
                            color: Qt.rgba(page.riskColor(modelData.risk).r,
                                           page.riskColor(modelData.risk).g,
                                           page.riskColor(modelData.risk).b, 0.12)
                            Text {
                                id: riskText
                                anchors.centerIn: parent
                                text: String(modelData.risk || "LOW").toUpperCase()
                                color: page.riskColor(modelData.risk)
                                font.family: "DejaVu Sans Mono"
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                            }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.max(0, parent.width - 330)
                            text: modelData.name || "（无名称）"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }
                        Item { width: 1; height: 1 }
                        AppButton {
                            text: "编辑"
                            minWidth: 50
                            horizontalPadding: 10
                            onClicked: {
                                ruleDialog.allRules = page.rawRules()
                                ruleDialog.openFor(modelData, index)
                            }
                        }
                        AppButton {
                            text: "↑"
                            minWidth: 28
                            horizontalPadding: 6
                            enabled: index > 0
                            onClicked: page.moveRule(index, -1)
                        }
                        AppButton {
                            text: "↓"
                            minWidth: 28
                            horizontalPadding: 6
                            enabled: index < (app.policyInfo.rule_groups || []).length - 1
                            onClicked: page.moveRule(index, 1)
                        }
                        AppButton {
                            text: "删除"
                            variant: "danger"
                            minWidth: 50
                            horizontalPadding: 10
                            onClicked: {
                                page.pendingDeleteIndex = index
                                page.pendingDeleteName = modelData.name || modelData.id || "该规则"
                                deleteRuleDialog.open()
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text: page.pathsOf(modelData).join("   ·   ")
                        color: Theme.textTertiary
                        font.family: "DejaVu Sans Mono"
                        font.pixelSize: Theme.fontSizeTiny
                        elide: Text.ElideMiddle
                    }
                    Text {
                        width: parent.width
                        visible: page.commandsOf(modelData).length > 0
                        text: "命令  " + page.commandsOf(modelData).join(", ")
                              + ((modelData.operations && modelData.operations.length > 0)
                                 ? "   ·   操作 " + modelData.operations.join(", ") : "")
                        color: Theme.textTertiary
                        font.family: "DejaVu Sans Mono"
                        font.pixelSize: Theme.fontSizeTiny
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        visible: modelData.reason !== undefined && String(modelData.reason).length > 0
                        text: modelData.reason || ""
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                }
            }
        }

        Text {
            width: parent.width
            visible: !app.policyInfo.rule_groups || app.policyInfo.rule_groups.length === 0
            text: "当前没有用户规则。内置保护规则仍然生效，其余命令使用默认风险等级。"
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }
    }

    // ------------------------------------------------------------- preview

    Rectangle {
        width: parent.width
        height: previewColumn.implicitHeight + 28
        radius: Theme.radiusMedium
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder

        Column {
            id: previewColumn
            x: 14
            y: 12
            width: parent.width - 28
            spacing: 8
            Text {
                text: "示例命令试运行"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.Medium
            }
            Text {
                width: parent.width
                text: "使用 aish 的 fallback 规则引擎解释命令，不执行命令；沙箱开启时实际决策可能由沙箱评估决定。"
                color: Theme.textTertiary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTiny
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            Row {
                width: parent.width
                spacing: 8
                AppTextField {
                    id: securityPreviewField
                    width: parent.width - 86
                    placeholder: "例如 rm -rf /etc/nginx"
                    onTextEdited: (value) => page.previewCommand = value
                    onAccepted: page.runPreview()
                }
                AppButton { text: "试运行"; onClicked: page.runPreview() }
            }
            Text {
                width: parent.width
                visible: page.previewText.length > 0
                text: page.previewText
                color: Theme.textSecondary
                font.family: "DejaVu Sans Mono"
                font.pixelSize: Theme.fontSizeTiny
                wrapMode: Text.WrapAnywhere
            }
        }
    }

    // ----------------------------------------------------- session approvals

    Item {
        width: parent.width
        height: 26
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "本会话临时放行"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.Medium
        }
        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            AppButton { text: "刷新"; onClicked: app.refreshSessionApprovals() }
            AppButton {
                text: "全部清除"
                enabled: (app.sessionApprovals || []).length > 0
                onClicked: clearApprovalsDialog.open()
            }
        }
    }

    Text {
        width: parent.width
        text: "只影响当前会话，不是全局白名单；重启或策略文件变化后会自动失效。"
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeTiny
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Column {
        width: parent.width
        spacing: 6
        Repeater {
            model: app.sessionApprovals || []
            delegate: Rectangle {
                width: parent.width
                height: 44
                radius: Theme.radiusSmall
                color: Theme.codeBg
                border.width: 1
                border.color: Theme.border

                Text {
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 110
                    text: {
                        if (modelData.kind === "target")
                            return modelData.tool + "  ·  " + modelData.value
                        return (modelData.host || "localhost") + "  ·  " + modelData.value
                    }
                    color: Theme.textSecondary
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: Theme.fontSizeTiny
                    elide: Text.ElideMiddle
                }
                AppButton {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "撤销"
                    minWidth: 48
                    horizontalPadding: 9
                    onClicked: {
                        if (modelData.kind === "target")
                            app.revokeSessionApproval("target", modelData.tool, modelData.value)
                        else
                            app.revokeSessionApproval("command", modelData.host || "localhost", modelData.value)
                    }
                }
            }
        }

        Text {
            width: parent.width
            visible: (app.sessionApprovals || []).length === 0
            text: app.sessionId.length > 0
                  ? "没有临时放行。审批弹窗中的“本会话允许”会出现在这里。"
                  : "当前没有活动会话。"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }
    }

    Text {
        width: parent.width
        text: "aish 审批弹窗会在每次高风险操作前展示规则、命令和风险原因；“本会话允许”写入 aish 的会话审批记忆。"
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeTiny
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    PolicyRuleDialog {
        id: ruleDialog
        onSaveRequested: (rule) => {
            const rules = page.rawRules()
            if (ruleDialog.editIndex >= 0 && ruleDialog.editIndex < rules.length) {
                rules[ruleDialog.editIndex] = rule
            } else {
                if (!rule.id)
                    rule.id = page.nextRuleId(rules)
                rules.push(rule)
            }
            const result = page.saveRules(rules)
            if (result && result.ok === true) {
                ruleDialog.saveSucceeded()
            } else {
                ruleDialog.saveFailed(result && result.error
                                      ? result.error : "安全规则保存失败")
            }
        }
    }

    ConfirmDialog {
        id: deleteRuleDialog
        title: "删除安全规则"
        message: "确定删除“" + page.pendingDeleteName + "”？删除后该路径/命令将按后续规则或默认风险处理。"
        detail: "保存会立即重写 security_policy.yaml，并让本会话已有的临时放行失效。"
        confirmText: "删除"
        danger: true
        onConfirmed: page.deleteRule(page.pendingDeleteIndex)
    }

    ConfirmDialog {
        id: clearApprovalsDialog
        title: "清除本会话授权"
        message: "将清除当前会话记住的全部命令和工具目标授权。"
        detail: "等价于 aish CLI 的 /forget-approvals；不会修改安全策略文件。"
        confirmText: "全部清除"
        danger: true
        onConfirmed: app.clearSessionApprovals()
    }
}
