import QtQuick
import UosAiLibre

SettingsScroll {
    id: page

    property bool dirty: false
    property string defaultRisk: "low"
    property bool sandboxEnabled: false
    property string sandboxOffAction: "allow"
    property string sandboxTimeout: "10"
    property bool inputGuardEnabled: true

    Component.onCompleted: load()
    onVisibleChanged: {
        app.refreshPolicy()
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

    Text {
        text: "安全与权限"
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: 18
        font.weight: Font.DemiBold
    }
    Text {
        width: parent.width
        text: "权限完全由 aish 的 security_policy.yaml 决定。这里不提供 GUI 自造的“只读/完全访问”模式。"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

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

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.border
            }

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

    Text {
        text: "规则（只读）"
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBody
        font.weight: Font.Medium
    }

    Rectangle {
        width: parent.width
        height: rulesColumn.implicitHeight + 28
        radius: Theme.radiusMedium
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder

        Column {
            id: rulesColumn
            x: 16
            y: 14
            width: parent.width - 32
            spacing: 10
            Repeater {
                model: app.policyInfo.rules || []
                delegate: Column {
                    width: rulesColumn.width
                    spacing: 3
                    Row {
                        width: parent.width
                        spacing: 8
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.id || "rule"
                            color: Theme.textPrimary
                            font.family: "DejaVu Sans Mono"
                            font.pixelSize: Theme.fontSizeTiny
                            font.weight: Font.DemiBold
                        }
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: riskLabel.implicitWidth + 12
                            height: 18
                            radius: 4
                            color: Qt.rgba(page.riskColor(modelData.risk).r,
                                           page.riskColor(modelData.risk).g,
                                           page.riskColor(modelData.risk).b, 0.12)
                            Text {
                                id: riskLabel
                                anchors.centerIn: parent
                                text: modelData.risk || "LOW"
                                color: page.riskColor(modelData.risk)
                                font.family: "DejaVu Sans Mono"
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                            }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 130
                            text: modelData.name || ""
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }
                    }
                    Text {
                        width: parent.width
                        text: (modelData.pattern || "") +
                              (modelData.command_list ? "  ·  " + modelData.command_list.join(", ") : "")
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
}
