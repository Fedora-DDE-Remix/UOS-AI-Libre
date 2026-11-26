import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UosAiLibre

Rectangle {
    id: settings
    property int cornerRadius: Theme.windowRadius
    property int currentIndex: 0
    signal closeRequested()
    signal toastRequested(string message)

    color: Theme.windowBg
    radius: cornerRadius

    property var navItems: [
        { label: "模型配置", icon: "settings" },
        { label: "AI随航", icon: "skill" },
        { label: "知识库管理", icon: "knowledge" },
        { label: "IM 接入", icon: "connector" },
        { label: "拓展能力", icon: "expand" },
        { label: "浏览器", icon: "browser-open" },
        { label: "使用协议", icon: "file" }
    ]

    Rectangle {
        id: nav
        width: 216
        height: parent.height
        color: Theme.isDark ? "#1B1B1F" : "#FBFBFC"
        topLeftRadius: settings.cornerRadius
        bottomLeftRadius: settings.cornerRadius

        Image {
            x: 20
            y: 20
            width: 26
            height: 26
            source: "qrc:/images/logo.png"
            fillMode: Image.PreserveAspectFit
        }

        Column {
            x: 8
            y: 62
            width: nav.width - 16
            spacing: 2

            Repeater {
                model: settings.navItems
                delegate: Item {
                    width: nav.width - 16
                    height: 36
                    HoverHandler { id: navHover }
                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusSmall
                        color: settings.currentIndex === index
                               ? Theme.selected
                               : (navHover.hovered ? Theme.hover : "transparent")
                    }
                    AppIcon {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        name: modelData.icon
                        size: 16
                        color: settings.currentIndex === index
                               ? Theme.textPrimary : Theme.icon
                    }
                    Text {
                        x: 38
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: settings.currentIndex === index
                               ? Theme.textPrimary : Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: settings.currentIndex === index
                                      ? Font.Medium : Font.Normal
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settings.currentIndex = index
                    }
                }
            }
        }
    }

    Item {
        x: nav.width
        width: parent.width - nav.width
        height: parent.height

        Item {
            id: header
            width: parent.width
            height: 58

            Item {
                x: 22
                y: 14
                width: 30
                height: 30
                HoverHandler { id: backHover }
                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radiusSmall
                    color: backHover.hovered ? Theme.hover : "transparent"
                }
                AppIcon {
                    anchors.centerIn: parent
                    name: "back"
                    size: 16
                    color: Theme.icon
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settings.closeRequested()
                }
            }

            Text {
                x: 62
                anchors.verticalCenter: parent.verticalCenter
                text: settings.navItems[settings.currentIndex].label
                color: Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.Medium
            }

            Item {
                visible: false
                width: 30
                height: 30
                anchors.right: parent.right
                anchors.rightMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                HoverHandler { id: closeHover }
                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radiusSmall
                    color: closeHover.hovered ? Theme.hover : "transparent"
                }
                AppIcon {
                    anchors.centerIn: parent
                    name: "x"
                    size: 15
                    color: Theme.icon
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settings.closeRequested()
                }
            }
        }

        StackLayout {
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            currentIndex: settings.currentIndex

            // 模型配置
            Flickable {
                clip: true
                contentHeight: modelContent.implicitHeight + 60
                boundsBehavior: Flickable.StopAtBounds

                Column {
                    id: modelContent
                    x: Math.max(24, (parent.width - width) / 2)
                    y: 10
                    width: Math.min(680, parent.width - 128)
                    spacing: 22

                    Item {
                        width: parent.width
                        height: 24
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "在线模型"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }
                        Text {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: "添加模型"
                            color: Theme.accent
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            verticalAlignment: Text.AlignVCenter
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: modelDialog.openWith(app.model, app.apiBase)
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: onlineColumn.implicitHeight
                        radius: Theme.radiusMedium
                        color: Theme.card
                        border.width: 1
                        border.color: Theme.cardBorder

                        Column {
                            id: onlineColumn
                            width: parent.width

                            Item {
                                width: parent.width
                                height: 58
                                AppIcon {
                                    x: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    name: "cloud"
                                    size: 18
                                    color: Theme.accent
                                }
                                Column {
                                    x: 44
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2
                                    Row {
                                        spacing: 8
                                        Text {
                                            text: "UOS-AI 本地配置"
                                            color: Theme.textPrimary
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeBody
                                            font.weight: Font.Medium
                                        }
                                        Rectangle {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 38
                                            height: 18
                                            radius: 3
                                            color: Theme.accentSoft
                                            Text {
                                                anchors.centerIn: parent
                                                text: "服务商"
                                                color: Theme.accent
                                                font.family: Theme.fontFamily
                                                font.pixelSize: 9
                                            }
                                        }
                                    }
                                    Text {
                                        width: parent.width
                                        text: app.apiBase
                                        color: Theme.textTertiary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeTiny
                                        elide: Text.ElideRight
                                    }
                                }
                                AppIcon {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    name: "chevron-right"
                                    size: 13
                                    color: Theme.iconSecondary
                                }
                            }

                            Rectangle {
                                x: 16
                                width: parent.width - 32
                                height: 1
                                color: Theme.border
                            }

                            Item {
                                width: parent.width
                                height: 52
                                HoverHandler { id: modelHover }
                                Rectangle {
                                    anchors.fill: parent
                                    radius: Theme.radiusSmall
                                    color: modelHover.hovered ? Theme.hover : "transparent"
                                }
                                Row {
                                    x: 24
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 8
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: app.model
                                        color: Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeBody
                                    }
                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 34
                                        height: 18
                                        radius: 3
                                        color: Theme.codeBg
                                        Text {
                                            anchors.centerIn: parent
                                            text: "模型"
                                            color: Theme.textSecondary
                                            font.family: Theme.fontFamily
                                            font.pixelSize: 9
                                        }
                                    }
                                }
                                Text {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 18
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "编辑"
                                    color: Theme.accent
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSmall
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: modelDialog.openWith(app.model, app.apiBase)
                                }
                            }
                        }
                    }

                    Text {
                        text: "本地模型"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }

                    Rectangle {
                        width: parent.width
                        height: localColumn.implicitHeight
                        radius: Theme.radiusMedium
                        color: Theme.card
                        border.width: 1
                        border.color: Theme.cardBorder

                        Column {
                            id: localColumn
                            width: parent.width

                            Item {
                                width: parent.width
                                height: 66
                                Column {
                                    x: 16
                                    width: parent.width - 100
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 3
                                    Text {
                                        text: "向量化模型插件"
                                        color: Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeBody
                                    }
                                    Text {
                                        text: "用于小U知识库。"
                                        color: Theme.textTertiary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeTiny
                                    }
                                }
                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 14
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 48
                                    height: 28
                                    radius: Theme.radiusSmall
                                    color: Theme.codeBg
                                    border.width: 1
                                    border.color: Theme.border
                                    Text {
                                        anchors.centerIn: parent
                                        text: "卸载"
                                        color: Theme.textTertiary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                    }
                                }
                            }

                            Rectangle {
                                x: 16
                                width: parent.width - 32
                                height: 1
                                color: Theme.border
                            }

                            Item {
                                width: parent.width
                                height: 66
                                Column {
                                    x: 16
                                    width: parent.width - 100
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 3
                                    Text {
                                        text: "DeepSeek-R1-1.5B"
                                        color: Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeBody
                                    }
                                    Text {
                                        text: "从开源社区下载，请谨慎操作。安装后在小U同学中使用。"
                                        color: Theme.textTertiary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeTiny
                                        elide: Text.ElideRight
                                        width: parent.width - 20
                                    }
                                }
                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 14
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 48
                                    height: 28
                                    radius: Theme.radiusSmall
                                    color: Theme.codeBg
                                    border.width: 1
                                    border.color: Theme.border
                                    Text {
                                        anchors.centerIn: parent
                                        text: "下载"
                                        color: Theme.textTertiary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                    }
                                }
                            }
                        }
                    }

                    Row {
                        width: parent.width
                        Text {
                            text: "私有模型"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }
                        Item { width: parent.width - 210; height: 1 }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "添加模型"
                            color: Theme.accent
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            verticalAlignment: Text.AlignVCenter
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settings.toastRequested("私有模型配置将在后续版本接入")
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 52
                        radius: Theme.radiusMedium
                        color: Theme.card
                        border.width: 1
                        border.color: Theme.cardBorder
                        Text {
                            x: 16
                            anchors.verticalCenter: parent.verticalCenter
                            text: "暂无"
                            color: Theme.textTertiary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeBase
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: configColumn.implicitHeight + 28
                        radius: Theme.radiusMedium
                        color: Theme.card
                        border.width: 1
                        border.color: Theme.cardBorder

                        Column {
                            id: configColumn
                            x: 16
                            y: 14
                            width: parent.width - 32
                            spacing: 8
                            Text {
                                text: "aish 配置"
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeBody
                                font.weight: Font.Medium
                            }
                            Text {
                                width: parent.width
                                text: app.configPath
                                color: Theme.textTertiary
                                font.family: "DejaVu Sans Mono"
                                font.pixelSize: Theme.fontSizeTiny
                                elide: Text.ElideMiddle
                            }
                            Text {
                                text: app.hasApiKey ? "已配置 API Key" : "尚未配置 API Key"
                                color: app.hasApiKey ? Theme.success : Theme.danger
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                            }
                            Text {
                                text: "打开配置文件"
                                color: Theme.accent
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: app.openConfigFile()
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text: "修改模型配置后，当前会话会重置。对话记录仍保存在共享的 sessions.db 中。"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                        lineHeight: 1.4
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }

                    Item { width: 1; height: 20 }
                }
            }

            // AI随航
            Flickable {
                clip: true
                contentHeight: copilotColumn.implicitHeight + 60
                boundsBehavior: Flickable.StopAtBounds

                Column {
                    id: copilotColumn
                    x: Math.max(24, (parent.width - width) / 2)
                    y: 10
                    width: Math.min(680, parent.width - 128)
                    spacing: 18

                    Rectangle {
                        width: parent.width
                        height: 150
                        radius: Theme.radiusMedium
                        color: Theme.card
                        border.width: 1
                        border.color: Theme.cardBorder

                        Row {
                            anchors.centerIn: parent
                            spacing: 4
                            Rectangle {
                                width: 300
                                height: 38
                                radius: 10
                                color: Theme.isDark ? "#26262B" : "#F7F7F9"
                                border.width: 1
                                border.color: Theme.border
                                Row {
                                    anchors.centerIn: parent
                                    spacing: 10
                                    Image {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 20
                                        height: 20
                                        source: "qrc:/images/logo.png"
                                        fillMode: Image.PreserveAspectFit
                                    }
                                    Repeater {
                                        model: ["搜索", "解释", "总结", "翻译"]
                                        delegate: Row {
                                            anchors.verticalCenter: parent.verticalCenter
                                            spacing: 3
                                            AppIcon {
                                                anchors.verticalCenter: parent.verticalCenter
                                                name: index === 0 ? "search" : (index === 1 ? "eye" : (index === 2 ? "report" : "globe"))
                                                size: 12
                                                color: Theme.iconSecondary
                                            }
                                            Text {
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: modelData
                                                color: Theme.textSecondary
                                                font.family: Theme.fontFamily
                                                font.pixelSize: 10
                                            }
                                        }
                                    }
                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 52
                                        height: 24
                                        radius: 6
                                        color: Theme.accentSoft
                                        Text {
                                            anchors.centerIn: parent
                                            text: "问问AI"
                                            color: Theme.accent
                                            font.family: Theme.fontFamily
                                            font.pixelSize: 10
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Row {
                        width: parent.width
                        Column {
                            spacing: 4
                            Text {
                                text: "随航关闭"
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeBody
                                font.weight: Font.Medium
                            }
                            Text {
                                text: "自动显示随航工具栏，可按 Super + R 唤醒。"
                                color: Theme.textTertiary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeTiny
                            }
                        }
                        Item { width: parent.width - 460; height: 1 }
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 38
                            height: 22
                            radius: 11
                            color: Theme.borderStrong
                            Rectangle {
                                x: 2
                                y: 2
                                width: 18
                                height: 18
                                radius: 9
                                color: "#FFFFFF"
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 256
                        radius: Theme.radiusMedium
                        color: Theme.card
                        border.width: 1
                        border.color: Theme.cardBorder

                        Item {
                            x: 16
                            y: 10
                            width: parent.width - 32
                            height: 28

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "技能管理"
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeBody
                                font.weight: Font.Medium
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: "添加技能"
                                color: Theme.accent
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: settings.toastRequested("技能由 aish 配置统一管理")
                                }
                            }
                        }
                        Row {
                            x: 16
                            y: 48
                            spacing: 26
                            Column {
                                spacing: 8
                                Text {
                                    text: "显示在工具栏"
                                    color: Theme.textTertiary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeTiny
                                }
                                Repeater {
                                    model: [
                                        { label: "搜索", icon: "search" },
                                        { label: "解释", icon: "eye" },
                                        { label: "总结", icon: "report" },
                                        { label: "翻译", icon: "globe" }
                                    ]
                                    delegate: Rectangle {
                                        width: 230
                                        height: 30
                                        radius: Theme.radiusSmall
                                        color: Theme.isDark ? "#232327" : "#F7F7F8"
                                        border.width: 1
                                        border.color: Theme.border
                                        AppIcon {
                                            x: 10
                                            anchors.verticalCenter: parent.verticalCenter
                                            name: modelData.icon
                                            size: 13
                                            color: Theme.iconSecondary
                                        }
                                        Text {
                                            x: 30
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: modelData.label
                                            color: Theme.textSecondary
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeSmall
                                        }
                                    }
                                }
                            }
                            Column {
                                spacing: 8
                                Text {
                                    text: "隐藏在更多菜单中"
                                    color: Theme.textTertiary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeTiny
                                }
                                Repeater {
                                    model: ["添加到AI知识库", "续写", "扩写", "纠错", "润色"]
                                    delegate: Rectangle {
                                        width: 230
                                        height: 30
                                        radius: Theme.radiusSmall
                                        color: Theme.isDark ? "#232327" : "#F7F7F8"
                                        border.width: 1
                                        border.color: Theme.border
                                        Text {
                                            x: 30
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: modelData
                                            color: Theme.textSecondary
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeSmall
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text: "AI随航与 DDE 托盘集成将在后续版本接入，当前版本保留界面预览。"
                        color: Theme.accent
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                }
            }

            // 知识库管理
            SettingsPlaceholder {
                iconName: "knowledge"
                title: "知识库管理"
                description: "接入本地文档与知识库，让回答有据可查。"
                bullet1: "文档索引、向量检索与引用溯源将在后续版本接入。"
                bullet2: "当前会话可直接读取工作区文件。"
            }

            // IM 接入
            SettingsPlaceholder {
                iconName: "connector"
                title: "IM 接入"
                description: "将 AI 助手接入即时通讯工具。"
                bullet1: "IM 接入属于后续版本范围。"
                bullet2: "当前版本优先完善本地桌面体验。"
            }

            // 拓展能力
            SettingsPlaceholder {
                iconName: "expand"
                title: "拓展能力"
                description: "MCP 服务与技能扩展由 aish 统一管理。"
                bullet1: "编辑 aish 配置即可启用 MCP 服务器。"
                bullet2: "工具执行受安全策略和审批确认约束。"
            }

            // 浏览器
            SettingsPlaceholder {
                iconName: "browser-open"
                title: "浏览器"
                description: "本项目坚持无 WebEngine 的纯 QML 前端。"
                bullet1: "内置浏览器面板暂未启用。"
                bullet2: "外部链接会交给系统默认浏览器打开。"
            }

            // 使用协议
            Flickable {
                clip: true
                contentHeight: agreementColumn.implicitHeight + 60
                Column {
                    id: agreementColumn
                    x: Math.max(24, (parent.width - width) / 2)
                    y: 10
                    width: Math.min(680, parent.width - 128)
                    spacing: 14
                    Text {
                        text: "UOS AI Libre 使用协议"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                    }
                    Text {
                        width: parent.width
                        text: "UOS AI Libre 是一个本地优先的桌面 AI 客户端，基于 aish 与 Qt Quick 构建。会话、配置和工具调用记录保存在本机。"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBase
                        lineHeight: 1.7
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                    Text {
                        width: parent.width
                        text: "AI 生成内容仅供参考，请自行甄别信息准确性。文件读取、网络访问和命令执行均受 aish 安全策略与审批确认约束。"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBase
                        lineHeight: 1.7
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                    Text {
                        width: parent.width
                        text: "本项目不包含云端账号、语音输入或浏览器内核。第三方模型服务的使用受其各自服务条款约束。"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBase
                        lineHeight: 1.7
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                }
            }
        }
    }

    ModelDialog {
        id: modelDialog
        onSaveRequested: (model, apiBase, apiKey) => {
            app.saveModelConfig(model, apiBase, apiKey)
        }
    }

}
