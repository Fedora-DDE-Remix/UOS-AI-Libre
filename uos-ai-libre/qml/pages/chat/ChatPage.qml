import QtQuick
import QtQuick.Controls
import UosAiLibre

Item {
    id: page
    property string draft: ""
    readonly property string activeModel: app.model
    property bool previewOpen: false
    property real previewProgress: previewOpen ? 1 : 0
    property bool atBottom: true
    property int activeCategory: 0
    property int leftRadius: 0
    property int rightRadius: Theme.windowRadius

    signal toastRequested(string message)
    signal openSettings()

    readonly property var chatModel: app.chat
    readonly property bool hasConversation: chatModel.count > 0
    readonly property real previewWidth: Math.min(440, width * 0.38)
    Behavior on previewProgress {
        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
    }
    readonly property string workspaceLabel: {
        const current = app.workspace
        if (!current || current === app.defaultWorkspace)
            return "默认工作空间"
        const parts = current.split("/")
        return parts.length > 0 && parts[parts.length - 1].length > 0
                ? parts[parts.length - 1] : "默认工作空间"
    }
    readonly property string chatTitle: app.sessionTitle

    property var recommendedCards: [
        { title: "存储空间优化", desc: "分析磁盘空间占用分布，识别可安全清理的文件", icon: "disk",
          prompt: "请分析当前系统的磁盘空间占用分布，找出可以安全清理的文件和目录，并给出清理建议。" },
        { title: "系统健康诊断", desc: "检查处理器、内存、磁盘及异常进程状态", icon: "health",
          prompt: "请帮我检查当前系统的健康状态，包括处理器、内存、磁盘占用和异常进程，并给出结论。" },
        { title: "网络质量诊断", desc: "检测连接质量、DNS解析、丢包情况", icon: "wifi",
          prompt: "请诊断当前网络质量，检查连接质量、DNS解析和丢包情况，并说明可能的问题。" },
        { title: "启动速度优化", desc: "分析开机耗时构成及启动服务，识别可优化项", icon: "speed",
          prompt: "请分析系统开机耗时构成和启动服务，找出影响启动速度的项目并给出优化建议。" },
        { title: "硬件配置评估", desc: "读取处理器、内存、显卡及存储配置", icon: "hardware",
          prompt: "请读取并评估当前电脑的硬件配置，包括处理器、内存、显卡和存储，并说明适合的用途。" },
        { title: "资料检索与溯源", desc: "针对问题检索已有材料，给出结论和来源", icon: "search",
          prompt: "请围绕我指定的问题检索已有资料，给出有依据的结论，并标注信息来源。" },
        { title: "汇报PPT大纲生成", desc: "规划每页要点、数据、素材及讲解逻辑", icon: "presentation",
          prompt: "请为我的工作汇报生成一份 PPT 大纲，规划每页要点、数据、素材和讲解逻辑。" },
        { title: "多文件综合报告生成", desc: "合并重复信息及不同观点，整理结构", icon: "report",
          prompt: "请帮我整理多个文件中的信息，合并重复内容、对比不同观点，并生成一份结构清晰的综合报告。" },
        { title: "可行性调研报告编写", desc: "基于完整大纲、试点数据、安全边界编写", icon: "research",
          prompt: "请帮我编写一份可行性调研报告，包含完整大纲、试点数据、风险和安全边界。" },
        { title: "经营分析月报生成", desc: "汇总指标趋势及异常信号，列出需要关注的问题", icon: "chart",
          prompt: "请帮我生成一份经营分析月报，汇总关键指标趋势和异常信号，并列出需要关注的问题。" },
        { title: "自动任务创建", desc: "描述定期执行的任务内容，小U帮你创建", icon: "automation",
          prompt: "我想创建一个定期执行的自动任务，请根据我的描述补全任务内容、执行频率和检查项。" },
        { title: "AI新闻每日简报", desc: "每天自动汇总重要AI新闻、来源链接", icon: "news",
          prompt: "请每天汇总重要的 AI 新闻，按主题分类，并附上来源链接。" }
    ]
    property var computerCards: [
        recommendedCards[0], recommendedCards[1], recommendedCards[2], recommendedCards[3]
    ]
    property var officeCards: [
        recommendedCards[6], recommendedCards[7], recommendedCards[5],
        { title: "学习计划制定", desc: "根据目标和时间安排可执行的学习计划", icon: "knowledge",
          prompt: "请根据我的学习目标和可用时间，制定一份可执行的学习计划。" }
    ]
    property var documentCards: [
        recommendedCards[7], recommendedCards[5], recommendedCards[8],
        { title: "长文润色与校对", desc: "改善结构、语气和表达，检查明显错误", icon: "edit",
          prompt: "请帮我润色这篇长文，改善结构、语气和表达，并检查明显错误。" }
    ]
    property var dataCards: [
        recommendedCards[9],
        { title: "数据清洗建议", desc: "识别缺失值、重复值和异常数据", icon: "disk",
          prompt: "请检查这份数据，识别缺失值、重复值和异常数据，并给出清洗建议。" },
        { title: "图表呈现建议", desc: "根据数据关系选择更合适的图表", icon: "chart",
          prompt: "请根据这组数据的关系，建议合适的图表类型和呈现方式。" },
        { title: "异常信号检测", desc: "从趋势和分布中发现值得关注的变化", icon: "search",
          prompt: "请从这组数据中检测异常信号，说明趋势和分布中值得关注的变化。" }
    ]
    property var automationCards: [
        recommendedCards[10], recommendedCards[11],
        { title: "定时备份提醒", desc: "按计划检查备份状态并提醒异常", icon: "tasks",
          prompt: "请帮我创建一个定时备份检查任务，按计划检查备份状态并提醒异常。" },
        { title: "会议纪要整理", desc: "把会议记录整理成结论、待办和责任人", icon: "report",
          prompt: "请把这份会议记录整理成结论、待办事项和责任人清单。" }
    ]
    property var cardSets: [recommendedCards, computerCards, officeCards,
                            documentCards, dataCards, automationCards]
    property var categoryNames: ["推荐", "电脑设置", "办公学习", "文档处理", "数据分析", "任务自动化"]
    readonly property var visibleCards: cardSets[activeCategory]

    function scrollToBottom() {
        messageList.positionViewAtEnd()
    }

    function scrollToTop() {
        messageList.positionViewAtBeginning()
    }

    function scrollToLastUser() {
        const row = chatModel.lastUserRow()
        if (row >= 0)
            messageList.positionViewAtIndex(row, ListView.Beginning)
        else
            messageList.positionViewAtBeginning()
    }

    function scheduleScroll() {
        if (atBottom)
            scrollTimer.restart()
    }

    function updateAtBottom() {
        atBottom = messageList.contentY + messageList.height
                   >= messageList.contentHeight - 90
    }

    function sendFrom(composer) {
        const text = composer.text.trim()
        const images = composer.attachments.slice()
        if ((text.length === 0 && images.length === 0) || app.generating)
            return
        if (!app.sendMessage(text, images)) {
            page.toastRequested(app.lastError.length > 0
                                ? app.lastError : "发送失败")
            return
        }
        composer.clear()
        atBottom = true
        Qt.callLater(page.scrollToBottom)
    }

    function showComposerMenu() {
        if (welcomeMode.visible)
            welcomeComposer.openPlusMenu()
        else
            chatComposer.openPlusMenu()
    }

    function startNewChat() {
        app.newConversation()
        draft = ""
        previewOpen = false
        welcomeComposer.clear()
        chatComposer.clear()
        Qt.callLater(function() {
            welcomeComposer.focusInput()
        })
    }

    Rectangle {
        anchors.fill: parent
        color: page.hasConversation ? Theme.chatBg : Theme.windowBg
        topLeftRadius: page.leftRadius
        bottomLeftRadius: page.leftRadius
        topRightRadius: page.rightRadius
        bottomRightRadius: page.rightRadius
    }

    Item {
        id: contentArea
        width: parent.width - page.previewWidth * page.previewProgress
        height: parent.height

        Item {
            id: welcomeMode
            anchors.fill: parent
            visible: !page.hasConversation

            Flickable {
                id: welcomeFlick
                anchors.fill: parent
                contentHeight: welcomeColumn.implicitHeight + 48
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Column {
                    id: welcomeColumn
                    width: Math.max(320, Math.min(690, page.width - 96))
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 0

                    Item { width: 1; height: 140 }

                    Text {
                        width: parent.width
                        text: "问问小U，事事无忧"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 26
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Item { width: 1; height: 28 }

                    Rectangle {
                        visible: !app.ready && app.lastError.length > 0
                        width: parent.width
                        height: visible ? 46 : 0
                        radius: Theme.radiusMedium
                        color: Theme.isDark ? "#332323" : "#FFF1F1"
                        border.width: 1
                        border.color: Theme.danger
                        Text {
                            x: 14
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 90
                            text: "AI 服务初始化失败：" + app.lastError
                            color: Theme.danger
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            width: 56
                            height: 26
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            radius: Theme.radiusSmall
                            color: Theme.danger
                            Text {
                                anchors.centerIn: parent
                                text: "重试"
                                color: "#FFFFFF"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: app.initialize()
                            }
                        }
                    }

                    Rectangle {
                        visible: app.ready && !app.hasApiKey
                        width: parent.width
                        height: visible ? 38 : 0
                        radius: Theme.radiusMedium
                        color: Theme.accentSoft
                        Text {
                            x: 14
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 100
                            text: "尚未配置 API Key，发送消息前请先在设置中填写。"
                            color: Theme.accent
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }
                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            text: "去设置"
                            color: Theme.accent
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: Font.Medium
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: page.openSettings()
                            }
                        }
                    }

                    Item {
                        width: 1
                        height: (!app.ready && app.lastError.length > 0)
                              || (app.ready && !app.hasApiKey) ? 10 : 0
                    }

                    Composer {
                        id: welcomeComposer
                        width: parent.width
                        compact: false
                        showWorkspace: true
                        modelName: page.activeModel
                        workspaceLabel: page.workspaceLabel
                        text: page.draft
                        onTextChanged: page.draft = welcomeComposer.text
                        onSendRequested: page.sendFrom(welcomeComposer)
                        onToastRequested: (message) => page.toastRequested(message)
                        onSettingsRequested: page.openSettings()
                    }

                    Item { width: 1; height: 18 }

                    Row {
                        spacing: 12
                        Repeater {
                            model: page.categoryNames
                            delegate: Rectangle {
                                width: label.implicitWidth + 20
                                height: 28
                                radius: Theme.radiusSmall
                                color: index === page.activeCategory
                                       ? Theme.accentSoft : "transparent"
                                Text {
                                    id: label
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: index === page.activeCategory
                                           ? Theme.accent : Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeBody
                                    font.weight: index === page.activeCategory
                                                  ? Font.Medium : Font.Normal
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: page.activeCategory = index
                                }
                            }
                        }
                    }

                    Item { width: 1; height: 12 }

                    Grid {
                        id: suggestionGrid
                        width: parent.width
                        columns: 3
                        columnSpacing: 16
                        rowSpacing: 8

                        Repeater {
                            model: page.visibleCards
                            delegate: SuggestionCard {
                                width: (suggestionGrid.width - 32) / 3
                                title: modelData.title
                                description: modelData.desc
                                iconName: modelData.icon
                                onClicked: {
                                    page.draft = modelData.prompt
                                    welcomeComposer.text = modelData.prompt
                                    welcomeComposer.focusInput()
                                }
                            }
                        }
                    }

                    Item { width: 1; height: 16 }

                    Row {
                        width: parent.width
                        Text {
                            text: "AI生成的内容仅供参考，请注意甄别信息准确性。"
                            color: Theme.textTertiary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTiny
                        }
                        Item { width: Math.max(0, parent.width - 420); height: 1 }
                        Text {
                            text: "Super + space 快速唤起"
                            color: Theme.textTertiary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTiny
                        }
                    }

                    Item { width: 1; height: 24 }
                }
            }
        }

        Item {
            id: chatMode
            anchors.fill: parent
            visible: page.hasConversation

            Item {
                id: chatHeader
                y: 40
                width: parent.width
                height: 44

                Text {
                    x: 32
                    width: Math.max(100, parent.width - 220)
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.chatTitle
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    elide: Text.ElideRight
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Rectangle {
                        width: previewLabel.implicitWidth + 30
                        height: 28
                        radius: Theme.radiusSmall
                        color: previewHover.hovered ? Theme.hover : "transparent"
                        HoverHandler { id: previewHover }
                        AppIcon {
                            x: 8
                            anchors.verticalCenter: parent.verticalCenter
                            name: "panel-right"
                            size: 14
                            color: Theme.iconSecondary
                        }
                        Text {
                            id: previewLabel
                            x: 27
                            anchors.verticalCenter: parent.verticalCenter
                            text: "预览"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: page.previewOpen = !page.previewOpen
                        }
                    }
                }
            }

            ListView {
                id: messageList
                anchors.top: chatHeader.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                clip: true
                model: chatModel
                spacing: 14
                cacheBuffer: 1200
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds

                header: Item { width: messageList.width; height: 8 }
                footer: Item {
                    width: messageList.width
                    height: chatComposer.height + 46
                }

                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    id: messageRow
                    width: ListView.view.width
                    height: loader.height

                    Loader {
                        id: loader
                        property string rowKind: kind
                        property string rowText: text
                        property string rowReasoning: reasoning
                        property bool rowReasoningActive: reasoningActive
                        property int rowReasoningElapsed: reasoningElapsed
                        property bool rowStreaming: streaming
                        property var rowTools: tools
                        property int rowElapsed: elapsedMs
                        property string rowModel: modelName
                        property string rowError: error
                        property string rowNotice: notice
                        property var rowAttachments: attachments
                        width: Math.min(900, messageRow.width - 96)
                        height: implicitHeight
                        x: (messageRow.width - width) / 2
                        sourceComponent: rowKind === "user"
                                         ? userComponent : assistantComponent
                        onLoaded: {
                            if (rowKind === "user") {
                                item.text = Qt.binding(function() { return loader.rowText })
                                item.attachments = Qt.binding(function() { return loader.rowAttachments })
                            } else {
                                item.text = Qt.binding(function() { return loader.rowText })
                                item.reasoning = Qt.binding(function() { return loader.rowReasoning })
                                item.reasoningActive = Qt.binding(function() { return loader.rowReasoningActive })
                                item.reasoningElapsed = Qt.binding(function() { return loader.rowReasoningElapsed })
                                item.streaming = Qt.binding(function() { return loader.rowStreaming })
                                item.tools = Qt.binding(function() { return loader.rowTools })
                                item.elapsedMs = Qt.binding(function() { return loader.rowElapsed })
                                item.model = Qt.binding(function() { return loader.rowModel })
                                item.error = Qt.binding(function() { return loader.rowError })
                                item.notice = Qt.binding(function() { return loader.rowNotice })
                            }
                        }
                    }
                }

                onContentYChanged: page.updateAtBottom()
                onContentHeightChanged: page.scheduleScroll()
                onCountChanged: {
                    page.updateAtBottom()
                    if (page.atBottom)
                        Qt.callLater(page.scrollToBottom)
                }
            }

            Rectangle {
                width: parent.width
                height: 30
                anchors.bottom: chatComposer.top
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.rgba(Theme.chatBg.r,
                                                                  Theme.chatBg.g,
                                                                  Theme.chatBg.b, 0) }
                    GradientStop { position: 1.0; color: Theme.chatBg }
                }
                enabled: false
            }

            Rectangle {
                visible: !page.atBottom
                width: 32
                height: 32
                radius: 16
                anchors.right: parent.right
                anchors.rightMargin: 26
                anchors.bottom: chatComposer.top
                anchors.bottomMargin: 12
                color: Theme.card
                border.width: 1
                border.color: Theme.border

                AppIcon {
                    anchors.centerIn: parent
                    name: "chevron-down"
                    size: 14
                    color: Theme.icon
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        page.atBottom = true
                        page.scrollToBottom()
                    }
                }
            }

            Composer {
                id: chatComposer
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 18
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(940, parent.width - 96)
                compact: true
                showWorkspace: false
                modelName: page.activeModel
                generating: app.generating
                text: page.draft
                onTextChanged: page.draft = chatComposer.text
                onSendRequested: page.sendFrom(chatComposer)
                onStopRequested: app.stop()
                onToastRequested: (message) => page.toastRequested(message)
                onSettingsRequested: page.openSettings()
            }
        }
    }

    PreviewPanel {
        x: contentArea.width
        y: 44
        width: page.previewWidth * page.previewProgress
        height: parent.height - 44
        rightRadius: page.rightRadius
        visible: page.previewOpen || page.previewProgress > 0
        opacity: page.previewOpen ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 160 } }
        onCloseRequested: page.previewOpen = false
        onToastRequested: (message) => page.toastRequested(message)
    }

    Component { id: userComponent; UserMessage {} }
    Component { id: assistantComponent; AssistantMessage {} }

    Connections {
        target: app

        function onSessionLoaded() {
            scrollTimer.stop()
            page.draft = ""
            chatComposer.clear()
            welcomeComposer.clear()
            page.atBottom = false
            Qt.callLater(page.scrollToLastUser)
            Qt.callLater(function() { chatComposer.focusInput() })
        }

        function onSessionCleared() {
            page.draft = ""
            page.atBottom = true
            welcomeComposer.clear()
            chatComposer.clear()
            Qt.callLater(function() { welcomeComposer.focusInput() })
        }
    }

    Connections {
        target: page.chatModel
        function onContentChanged() { page.scheduleScroll() }
    }

    Timer {
        id: scrollTimer
        interval: 50
        onTriggered: {
            if (page.atBottom)
                page.scrollToBottom()
        }
    }

    Component.onCompleted: {
        app.initialize()
        if (page.hasConversation)
            Qt.callLater(page.scrollToLastUser)
        else
            Qt.callLater(function() { welcomeComposer.focusInput() })
    }
}
