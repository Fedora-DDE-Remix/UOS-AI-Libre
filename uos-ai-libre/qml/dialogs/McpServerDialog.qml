import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root
    property string transport: "stdio"
    property string errorText: ""
    property string testText: ""
    signal saved(var args)

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    width: Math.min(640, parent ? parent.width - 48 : 640)
    height: Math.min(600, parent ? parent.height - 32 : 600)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    function splitArgs(text) {
        return String(text || "").split("\n")
            .map(function (line) { return line.trim() })
            .filter(function (line) { return line.length > 0 })
    }

    function parsePairs(text, label) {
        const result = []
        const lines = String(text || "").split("\n")
        for (let i = 0; i < lines.length; ++i) {
            const line = lines[i].trim()
            if (line.length === 0)
                continue
            const at = line.indexOf("=")
            if (at <= 0) {
                errorText = label + " 第 " + (i + 1) + " 行需要 KEY=VALUE 格式"
                return null
            }
            result.push({key: line.slice(0, at).trim(), value: line.slice(at + 1)})
        }
        return result
    }

    function draft() {
        errorText = ""
        const name = nameField.text.trim()
        if (name.length === 0) {
            errorText = "请填写服务器名称"
            return null
        }
        const existing = app.mcpStatus.servers || []
        for (let i = 0; i < existing.length; ++i) {
            if (String(existing[i].name || "") === name) {
                errorText = "已存在同名 MCP 服务器"
                return null
            }
        }
        const env = parsePairs(envArea.text, "环境变量")
        if (env === null)
            return null
        const headers = parsePairs(headerArea.text, "Headers")
        if (headers === null)
            return null
        const args = {
            name: name,
            transport: transport,
            trust: trustChoice.value,
            side_effects: effectsChoice.value,
            expose: exposeChoice.value,
            env: env,
            headers: headers
        }
        if (transport === "stdio") {
            const command = commandField.text.trim()
            if (command.length === 0) {
                errorText = "stdio 需要填写命令"
                return null
            }
            args.command = [command].concat(splitArgs(argsArea.text))
        } else {
            const url = urlField.text.trim()
            if (url.length === 0) {
                errorText = "http 需要填写 URL"
                return null
            }
            args.url = url
        }
        if (tokenField.text.trim().length > 0)
            args.token = tokenField.text.trim()
        return args
    }

    function reset() {
        nameField.text = ""
        transport = "stdio"
        commandField.text = ""
        argsArea.text = ""
        urlField.text = ""
        tokenField.text = ""
        envArea.text = ""
        headerArea.text = ""
        trustChoice.value = "ask"
        effectsChoice.value = "unknown"
        exposeChoice.value = "gateway"
        errorText = ""
        testText = ""
        bodyFlick.contentY = 0
    }

    Connections {
        target: app
        function onMcpTestFinished(result) {
            if (!root.visible || !result)
                return
            const data = result.data || ({})
            if (String(data.label || "") !== "draft:" + nameField.text.trim())
                return
            if (result.ok === true) {
                const tools = data.tools || []
                root.testText = "连接成功 · 版本 " + (data.version || "unknown")
                        + " · " + tools.length + " 个工具"
                if (tools.length > 0)
                    root.testText += "\n" + tools.slice(0, 10).map(function (tool) {
                        return "• " + tool.name + (tool.description ? "  " + tool.description : "")
                    }).join("\n")
            } else {
                root.testText = "连接失败：" + (result.error || "unknown error")
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
                text: "添加 MCP 服务器"
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
                    text: "MCP 服务器是以你的用户权限运行的外部进程。请只添加可信来源，并优先保持 trust=ask、side_effects=unknown。"
                    color: Theme.warning
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    lineHeight: 1.45
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }

                Column {
                    width: parent.width
                    spacing: 5
                    Text { text: "名称"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    AppTextField { id: nameField; width: parent.width; placeholder: "例如 filesystem" }
                    Text {
                        width: parent.width
                        text: "工具名前缀会变成 mcp__<name>；名称创建后不可修改。"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                    }
                }

                Column {
                    width: parent.width
                    spacing: 5
                    Text { text: "传输方式"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    SettingChoice {
                        options: [{value: "stdio", label: "stdio"},
                                  {value: "http", label: "http"}]
                        value: root.transport
                        onChanged: (value) => root.transport = value
                    }
                }

                Column {
                    width: parent.width
                    spacing: 5
                    visible: root.transport === "stdio"
                    Text { text: "命令"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    AppTextField { id: commandField; width: parent.width; placeholder: "例如 npx" }
                    Text { text: "参数（每行一个，不经过 shell）"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    AppTextArea { id: argsArea; width: parent.width; minHeight: 62; placeholder: "-y\n@modelcontextprotocol/server-filesystem\n/home/me/project" }
                }

                Column {
                    width: parent.width
                    spacing: 5
                    visible: root.transport === "http"
                    Text { text: "URL"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    AppTextField { id: urlField; width: parent.width; placeholder: "https://example.com/mcp" }
                    Text { text: "Token（可选，写入 Authorization: Bearer …；不会回显）"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    AppTextField { id: tokenField; width: parent.width; password: true; placeholder: "只保存在 config.yaml" }
                }

                Row {
                    width: parent.width
                    spacing: 10
                    Column {
                        width: (parent.width - 10) / 2
                        spacing: 5
                        Text { text: "默认 trust"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                        SettingChoice {
                            id: trustChoice
                            options: [{value: "ask", label: "ask"},
                                      {value: "allow", label: "allow"},
                                      {value: "block", label: "block"}]
                            value: "ask"
                        }
                    }
                    Column {
                        width: (parent.width - 10) / 2
                        spacing: 5
                        Text { text: "声明副作用"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                        SettingChoice {
                            id: effectsChoice
                            options: [{value: "read", label: "read"},
                                      {value: "network", label: "network"},
                                      {value: "write", label: "write"},
                                      {value: "unknown", label: "unknown"}]
                            value: "unknown"
                        }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 3
                    Text { text: "工具暴露方式"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    SettingChoice {
                        id: exposeChoice
                        options: [{value: "gateway", label: "gateway"},
                                  {value: "tools", label: "tools"}]
                        value: "gateway"
                    }
                    Text {
                        width: parent.width
                        text: "gateway 只注册一个发现工具，推荐；tools 会把每个远程工具都放进上下文。"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                }

                Column {
                    width: parent.width
                    spacing: 5
                    Text { text: "环境变量（KEY=VALUE，每行一个）"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    AppTextArea { id: envArea; width: parent.width; minHeight: 52; placeholder: "GITHUB_TOKEN=${GITHUB_TOKEN}" }
                }
                Column {
                    width: parent.width
                    spacing: 5
                    Text { text: "HTTP Headers（KEY=VALUE，每行一个）"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTiny }
                    AppTextArea { id: headerArea; width: parent.width; minHeight: 46 }
                }

                Row {
                    spacing: 8
                    AppButton {
                        text: app.mcpBusy ? "测试中…" : "测试连接"
                        enabled: !app.mcpBusy
                        onClicked: {
                            const args = root.draft()
                            if (args !== null) {
                                root.testText = "正在连接…"
                                app.testMcpDraft(args)
                            }
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "保存前先验证命令、URL 和工具列表"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                    }
                }

                Text {
                    width: parent.width
                    visible: root.testText.length > 0
                    text: root.testText
                    color: Theme.textSecondary
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: Theme.fontSizeTiny
                    wrapMode: Text.WrapAnywhere
                    lineHeight: 1.45
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
                    text: "添加"
                    variant: "primary"
                    enabled: !app.mcpBusy
                    onClicked: {
                        const args = root.draft()
                        if (args !== null) {
                            root.close()
                            root.saved(args)
                        }
                    }
                }
            }
        }
    }
}
