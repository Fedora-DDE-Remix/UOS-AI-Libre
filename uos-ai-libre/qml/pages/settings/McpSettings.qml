import QtQuick
import QtQuick.Controls
import UosAiLibre

SettingsScroll {
    id: page

    property string pendingRemoveName: ""
    property var expandedServers: ({})

    onVisibleChanged: {
        if (visible) {
            app.refreshMcpStatus()
            app.refreshToolCatalog()
        }
    }

    function statusColor(status) {
        if (status === "connected")
            return Theme.success
        if (status === "failed")
            return Theme.danger
        if (status === "connecting")
            return Theme.warning
        return Theme.textTertiary
    }

    function statusLabel(status) {
        if (status === "connected") return "已连接"
        if (status === "connecting") return "连接中"
        if (status === "failed") return "连接失败"
        if (status === "disabled") return "已禁用"
        if (status === "not_started") return "未启动"
        return status || "未知"
    }

    Text {
        text: "MCP 连接器"
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: 18
        font.weight: Font.DemiBold
    }
    Text {
        width: parent.width
        text: "服务器、工具、信任级别和副作用都来自 aish 的 config.yaml 与运行中的 MCP manager。这里可以启停、测试、调整信任和删除；连接参数仍建议在 YAML 中编辑。"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Rectangle {
        width: parent.width
        height: masterColumn.implicitHeight + 26
        radius: Theme.radiusMedium
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder

        Column {
            id: masterColumn
            x: 16
            y: 12
            width: parent.width - 32
            spacing: 8

            Item {
                width: parent.width
                height: 42
                Column {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3
                    Text {
                        text: "MCP 总开关"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: Font.Medium
                    }
                    Text {
                        text: app.mcpStatus.active === true
                              ? "运行中 · 已注册 mcp__ 工具"
                              : "未激活 · 不会向模型暴露 MCP 工具"
                        color: app.mcpStatus.active === true ? Theme.success : Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                    }
                }
                ToggleSwitch {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    checked: app.mcpStatus.master_enabled === true
                    onToggled: (value) => app.setMcpMasterEnabled(value)
                }
            }
            Text {
                width: parent.width
                text: app.mcpStatus.config_path || app.configPath
                color: Theme.textTertiary
                font.family: "DejaVu Sans Mono"
                font.pixelSize: Theme.fontSizeTiny
                elide: Text.ElideMiddle
            }
        }
    }

    Row {
        width: parent.width
        spacing: 8
        AppButton {
            text: "添加服务器"
            variant: "primary"
            onClicked: { addDialog.reset(); addDialog.open() }
        }
        AppButton {
            text: app.mcpBusy ? "处理中…" : "重载全部"
            enabled: !app.mcpBusy
            onClicked: app.reloadMcp()
        }
        AppButton { text: "刷新"; onClicked: app.refreshMcpStatus() }
        AppButton { text: "打开 config.yaml"; onClicked: app.openConfigFile() }
    }

    Column {
        width: parent.width
        spacing: 4
        Repeater {
            model: app.mcpStatus.warnings || []
            delegate: Text {
                width: parent.width
                text: "配置警告：" + modelData
                color: Theme.warning
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTiny
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
        }
    }

    Rectangle {
        width: parent.width
        height: emptyColumn.implicitHeight + 30
        visible: !app.mcpStatus.servers || app.mcpStatus.servers.length === 0
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
                text: "尚未配置 MCP server"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.Medium
            }
            Text {
                width: parent.width
                text: "可点击“添加服务器”，或在 config.yaml 的 mcp.servers 中添加后重载。MCP server 是可执行程序，会以你的用户权限运行。"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
        }
    }

    Repeater {
        model: app.mcpStatus.servers || []
        delegate: Rectangle {
            id: card
           width: parent.width
            height: serverColumn.implicitHeight + 28
            radius: Theme.radiusMedium
            color: Theme.card
            border.width: 1
            border.color: modelData.status === "failed" ? Theme.danger : Theme.cardBorder

            property string serverName: modelData.name || ""
            property bool showTools: page.expandedServers[serverName] === true

            Column {
                id: serverColumn
                x: 16
                y: 14
                width: parent.width - 32
                spacing: 7

                Item {
                    width: parent.width
                    height: 30
                    Row {
                        anchors.left: parent.left
                        anchors.right: enableSwitch.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.min(implicitWidth, parent.width - 260)
                            text: card.serverName
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeBody
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: statusText.implicitWidth + 14
                            height: 20
                            radius: 4
                            color: Qt.rgba(page.statusColor(modelData.status).r,
                                           page.statusColor(modelData.status).g,
                                           page.statusColor(modelData.status).b, 0.12)
                            Text {
                                id: statusText
                                anchors.centerIn: parent
                                text: page.statusLabel(modelData.status)
                                color: page.statusColor(modelData.status)
                                font.family: "DejaVu Sans Mono"
                                font.pixelSize: 9
                            }
                        }
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: transportText.implicitWidth + 14
                            height: 20
                            radius: 4
                            color: Theme.codeBg
                            Text {
                                id: transportText
                                anchors.centerIn: parent
                                text: modelData.transport || "stdio"
                                color: Theme.textSecondary
                                font.family: "DejaVu Sans Mono"
                                font.pixelSize: 9
                            }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.declared_side_effects || "unknown"
                            color: Theme.textTertiary
                            font.family: "DejaVu Sans Mono"
                            font.pixelSize: 9
                        }
                    }
                    ToggleSwitch {
                        id: enableSwitch
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        checked: modelData.enabled !== false
                        onToggled: (value) => app.setMcpEnabled(card.serverName, value)
                    }
                }

                Text {
                    width: parent.width
                    text: modelData.endpoint || ""
                    color: Theme.textTertiary
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: Theme.fontSizeTiny
                    elide: Text.ElideMiddle
                }
                Text {
                    width: parent.width
                    visible: modelData.status === "failed"
                             && modelData.status_detail
                             && modelData.status_detail.reason !== undefined
                    text: "失败原因：" + (modelData.status_detail
                          ? modelData.status_detail.reason : "")
                    color: Theme.danger
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTiny
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }

                Item {
                    width: parent.width
                    height: 30
                    Row {
                        anchors.left: parent.left
                        anchors.right: expandButton.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "服务器 trust"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTiny
                        }
                        SettingChoice {
                            anchors.verticalCenter: parent.verticalCenter
                            options: [{value: "ask", label: "ask"},
                                      {value: "allow", label: "allow"},
                                      {value: "block", label: "block"}]
                            value: modelData.trust || "ask"
                            onChanged: (value) => app.setMcpTrust(card.serverName, "*", value)
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "工具 " + (modelData.tools ? modelData.tools.length : 0)
                            color: Theme.textTertiary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTiny
                        }
                    }
                    AppButton {
                        id: expandButton
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: card.showTools ? "收起工具" : "展开工具"
                        minWidth: 70
                        horizontalPadding: 9
                        onClicked: {
                            const next = Object.assign({}, page.expandedServers)
                            next[card.serverName] = !card.showTools
                            page.expandedServers = next
                        }
                    }
                }

                Column {
                    width: parent.width
                    visible: card.showTools
                    spacing: 5
                    Repeater {
                        model: modelData.tools || []
                        delegate: Rectangle {
                            width: parent.width
                            height: 36
                            radius: Theme.radiusSmall
                            color: Theme.codeBg
                            Text {
                                x: 10
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.max(60, parent.width - 250)
                                text: modelData.name
                                color: Theme.textSecondary
                                font.family: "DejaVu Sans Mono"
                                font.pixelSize: Theme.fontSizeTiny
                                elide: Text.ElideRight
                            }
                            Text {
                                x: 10
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 4
                                width: Math.max(60, parent.width - 250)
                                text: (modelData.side_effects || "unknown")
                                      + (modelData.trust_override ? "  ·  覆盖 " + modelData.trust_override : "")
                                color: Theme.textTertiary
                                font.family: Theme.fontFamily
                                font.pixelSize: 9
                                elide: Text.ElideRight
                            }
                            SettingChoice {
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                options: [{value: "ask", label: "ask"},
                                          {value: "allow", label: "allow"},
                                          {value: "block", label: "block"}]
                                value: modelData.trust || "ask"
                                onChanged: (value) => app.setMcpTrust(
                                                         card.serverName,
                                                         modelData.name, value)
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        visible: !modelData.tools || modelData.tools.length === 0
                        text: "尚未发现工具。连接成功后 aish 会缓存工具目录。"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                    }
                }

                Row {
                    spacing: 8
                    AppButton {
                        text: app.mcpBusy ? "测试中…" : "测试连接"
                        enabled: !app.mcpBusy
                        onClicked: app.testMcpServer(card.serverName)
                    }
                    AppButton {
                        text: "设置"
                        onClicked: optionsDialog.openFor(modelData)
                    }
                    AppButton {
                        text: "删除"
                        variant: "danger"
                        onClicked: {
                            page.pendingRemoveName = card.serverName
                            removeDialog.open()
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        width: parent.width
        height: testColumn.implicitHeight + 24
        visible: app.mcpTestResult.job === "mcp_test"
        radius: Theme.radiusMedium
        color: Theme.card
        border.width: 1
        border.color: app.mcpTestResult.pending === true
                      ? Theme.border
                      : (app.mcpTestResult.ok === true ? Theme.success : Theme.danger)

        Column {
            id: testColumn
            x: 14
            y: 12
            width: parent.width - 28
            spacing: 5
            Text {
                text: app.mcpTestResult.pending === true
                      ? "正在测试连接…"
                      : (app.mcpTestResult.ok === true ? "连接测试成功" : "连接测试失败")
                color: app.mcpTestResult.pending === true
                       ? Theme.textSecondary
                       : (app.mcpTestResult.ok === true ? Theme.success : Theme.danger)
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.Medium
            }
            Text {
                width: parent.width
                visible: app.mcpTestResult.ok === true
                text: "版本 " + ((app.mcpTestResult.data || {}).version || "unknown")
                      + " · " + (((app.mcpTestResult.data || {}).tools || []).length) + " 个工具"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTiny
            }
            Text {
                width: parent.width
                visible: app.mcpTestResult.pending !== true && app.mcpTestResult.ok !== true
                text: app.mcpTestResult.error || ""
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTiny
                wrapMode: Text.WrapAnywhere
            }
            Repeater {
                model: app.mcpTestResult.ok === true
                       ? ((app.mcpTestResult.data || {}).tools || []).slice(0, 12) : []
                delegate: Text {
                    width: parent.width
                    text: "• " + modelData.name
                          + (modelData.description ? "  " + modelData.description : "")
                    color: Theme.textTertiary
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: Theme.fontSizeTiny
                    elide: Text.ElideRight
                }
            }
        }
    }

    Text {
        width: parent.width
        text: "MCP 工具调用会经过 aish 的审批和 trust 规则；trust=allow 只对声明为 read 且注解只读的工具免确认，unknown/write 仍会确认。"
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeTiny
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Timer {
        interval: 700
        repeat: true
        running: {
            const servers = app.mcpStatus.servers || []
            for (let i = 0; i < servers.length; ++i) {
                if (servers[i].status === "connecting")
                    return true
            }
            return false
        }
        onTriggered: app.refreshMcpStatus()
    }

    McpServerDialog {
        id: addDialog
        onSaved: (args) => app.addMcpServer(args)
    }

    McpServerOptionsDialog {
        id: optionsDialog
        onSaved: (args) => app.updateMcpServer(optionsDialog.serverName, args)
    }

    ConfirmDialog {
        id: removeDialog
        confirmEnabled: !app.generating
        title: "移除 MCP 服务器"
        message: "确定移除“" + page.pendingRemoveName + "”？"
        detail: "将关闭连接、从 config.yaml 中删除该 server，并从当前会话移除对应的 mcp__ 工具。"
        confirmText: "移除"
        danger: true
        onConfirmed: app.removeMcpServer(page.pendingRemoveName)
    }
}
