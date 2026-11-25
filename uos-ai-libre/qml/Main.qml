import QtQuick
import QtQuick.Controls
import UosAiLibre

ApplicationWindow {
    id: window
    visible: false
    width: 1290
    height: 800
    minimumWidth: 980
    minimumHeight: 640
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    title: "UOS AI Libre"

    property string activePage: "chat"
    property bool sidebarCollapsed: false
    readonly property int themeMode: app.themeMode
    property bool systemDark: app.systemDark
    readonly property bool maximized: visibility === Window.Maximized
    readonly property int frameMargin: maximized ? 0 : 14
    readonly property int corner: maximized ? 0 : Theme.windowRadius


    function applyTheme() {
        Theme.isDark = themeMode === 1 || (themeMode === 2 && systemDark)
    }

    onThemeModeChanged: applyTheme()
    onSystemDarkChanged: applyTheme()

    function showToast(message) {
        if (message && message.length > 0)
            toast.show(message)
    }

    function newChatFromTray() {
        window.activePage = "chat"
        chatPage.startNewChat()
    }

    function showDemoApproval(request) {
        approvalDialog.request = request
        approvalDialog.open()
    }

    function syncApproval() {
        const request = app.pendingApproval
        if (request && request.request_id) {
            approvalDialog.request = request
            if (!approvalDialog.opened)
                approvalDialog.open()
        } else if (approvalDialog.opened) {
            approvalDialog.close()
        }
    }

    Connections {
        target: app
        function onPendingApprovalChanged() { window.syncApproval() }
        function onToastRequested(message) { window.showToast(message) }
    }

    function showSettings(index) {
        window.activePage = "settings"
        settingsPage.currentIndex = index
    }

    function showComposerMenu() {
        window.activePage = "chat"
        chatPage.showComposerMenu()
    }

    function openAppMenu() {
        mainMenu.openMenu()
    }

    function openThemeMenu() {
        mainMenu.openThemeSubmenu()
    }

    function openHistoryContext() {
        if (app.sessions.length > 0)
            sidebar.openContext(app.sessions[0], 212)
    }

    function startHistoryRename() {
        if (app.sessions.length > 0)
            sidebar.beginRename(app.sessions[0])
    }

    function toggleMaximize() {
        if (window.visibility === Window.Maximized)
            window.showNormal()
        else
            window.showMaximized()
    }

    Shortcut {
        sequence: "Ctrl+N"
        onActivated: {
            window.activePage = "chat"
            chatPage.startNewChat()
        }
    }

    Shortcut {
        sequence: "Ctrl+,"
        onActivated: window.activePage = "settings"
    }

    Shortcut {
        sequence: StandardKey.Quit
        onActivated: windowHost.quitApp()
    }

    Shortcut {
        sequence: StandardKey.Cancel
        onActivated: {
            if (window.activePage === "settings")
                window.activePage = "chat"
        }
    }

    Item {
        id: outer
        anchors.fill: parent
        anchors.margins: window.frameMargin

        Rectangle {
            visible: !window.maximized
            anchors.fill: parent
            anchors.margins: -2
            radius: window.corner + 2
            color: Theme.isDark ? "#40000000" : "#16000000"
        }

        Rectangle {
            visible: !window.maximized
            anchors.fill: parent
            anchors.margins: -7
            radius: window.corner + 7
            color: Theme.isDark ? "#30000000" : "#0D000000"
        }

        Rectangle {
            id: frame
            anchors.fill: parent
            radius: window.corner
            color: Theme.windowBg
            border.width: 1
            border.color: Theme.isDark ? "#3A3A40" : "#33000000"
            clip: true

            Item {
                id: frameContent
                anchors.fill: parent

                Sidebar {
                    id: sidebar
                    width: window.sidebarCollapsed ? 0 : Theme.sidebarWidth
                    height: parent.height
                    appWindow: window
                    activeSection: window.activePage === "settings" ? "" : window.activePage
                    collapsed: window.sidebarCollapsed
                    cornerRadius: window.corner
                    opacity: window.sidebarCollapsed ? 0 : 1
                    Behavior on width {
                        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                    }
                    Behavior on opacity {
                        NumberAnimation { duration: 160 }
                    }
                    onNewChatRequested: {
                        window.activePage = "chat"
                        chatPage.startNewChat()
                    }
                    onSessionActivated: (sessionId) => {
                        window.activePage = "chat"
                        app.loadSession(sessionId)
                    }
                    onTasksRequested: window.activePage = "tasks"
                    onKnowledgeRequested: window.activePage = "knowledge"
                    onCollapseRequested: window.sidebarCollapsed = true
                    onToastRequested: (message) => window.showToast(message)
                }

                Item {
                    id: mainArea
                    anchors.left: sidebar.right
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom

                    ChatPage {
                        id: chatPage
                        anchors.fill: parent
                        visible: window.activePage === "chat"
                        leftRadius: window.sidebarCollapsed ? window.corner : 0
                        rightRadius: window.corner
                        onToastRequested: (message) => window.showToast(message)
                        onOpenSettings: window.showSettings(0)
                    }

                    PlaceholderPage {
                        anchors.fill: parent
                        visible: window.activePage === "tasks"
                        leftRadius: window.sidebarCollapsed ? window.corner : 0
                        rightRadius: window.corner
                        pageTitle: "自动任务"
                        pageSubtitle: "让 AI 按计划自动执行任务，并把结果带回对话。"
                        iconName: "tasks"
                        emptyTitle: "还没有自动任务"
                        emptyDescription: "描述你希望定期执行的事情，AI 会补全执行步骤和时间安排。"
                        actionText: "创建任务"
                        onActionRequested: window.showToast("自动任务后端将在后续版本接入")
                    }

                    PlaceholderPage {
                        anchors.fill: parent
                        visible: window.activePage === "knowledge"
                        leftRadius: window.sidebarCollapsed ? window.corner : 0
                        rightRadius: window.corner
                        pageTitle: "AI 知识库"
                        pageSubtitle: "把本地资料整理成可检索的知识，让回答有据可查。"
                        iconName: "knowledge"
                        emptyTitle: "知识库为空"
                        emptyDescription: "导入文档后，AI 可以在回答时引用本地资料并标注来源。"
                        actionText: "导入资料"
                        onActionRequested: window.showToast("知识库索引将在后续版本接入")
                    }
                }

                SettingsPage {
                    id: settingsPage
                    anchors.fill: parent
                    visible: window.activePage === "settings"
                    z: 200
                    cornerRadius: window.corner
                    onCloseRequested: window.activePage = "chat"
                    onToastRequested: (message) => window.showToast(message)
                }

                Item {
                    id: collapsedExpand
                    visible: window.sidebarCollapsed
                    x: 12
                    y: 10
                    width: 32
                    height: 32
                    z: 60
                    HoverHandler { id: expandHover }
                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusSmall
                        color: expandHover.hovered ? Theme.hover : "transparent"
                    }
                    AppIcon {
                        anchors.centerIn: parent
                        name: "panel-right"
                        size: 16
                        color: Theme.icon
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: window.sidebarCollapsed = false
                    }
                }

                MouseArea {
                    id: titleDrag
                    anchors.top: parent.top
                    anchors.left: sidebar.right
                    anchors.right: windowControls.left
                    height: 38
                    z: 10
                    acceptedButtons: Qt.LeftButton
                    onPressed: {
                        if (!window.maximized)
                            window.startSystemMove()
                    }
                    onDoubleClicked: window.toggleMaximize()
                }

                Item {
                    id: windowControls
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: 8
                    anchors.rightMargin: 10
                    width: controlsRow.width
                    height: 34
                    z: 300

                    Row {
                        id: controlsRow
                        spacing: 2

                        Item {
                            width: 30
                            height: 32
                            AppIcon {
                                anchors.centerIn: parent
                                name: "signal"
                                size: 15
                                color: Theme.textSecondary
                            }
                        }

                        WindowButton {
                            id: menuButton
                            icon: "menu"
                            onClicked: window.openAppMenu()
                        }

                        Item { width: 6; height: 1 }

                        WindowButton {
                            icon: "window-min"
                            onClicked: window.showMinimized()
                        }
                        WindowButton {
                            icon: "window-max"
                            maximized: window.maximized
                            onClicked: window.toggleMaximize()
                        }
                        WindowButton {
                            icon: "x"
                            danger: true
                            onClicked: window.close()
                        }
                    }
                }

                Toast {
                    id: toast
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 18
                    z: 400
                }

                // Edge resize handles (hidden while maximized).
                MouseArea {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 5
                    z: 120
                    enabled: !window.maximized
                    cursorShape: Qt.SizeHorCursor
                    onPressed: window.startSystemResize(Qt.LeftEdge)
                }
                MouseArea {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 5
                    z: 120
                    enabled: !window.maximized
                    cursorShape: Qt.SizeHorCursor
                    onPressed: window.startSystemResize(Qt.RightEdge)
                }
                MouseArea {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 5
                    z: 120
                    enabled: !window.maximized
                    cursorShape: Qt.SizeVerCursor
                    onPressed: window.startSystemResize(Qt.TopEdge)
                }
                MouseArea {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 5
                    z: 120
                    enabled: !window.maximized
                    cursorShape: Qt.SizeVerCursor
                    onPressed: window.startSystemResize(Qt.BottomEdge)
                }
                MouseArea {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    width: 8
                    height: 8
                    z: 121
                    enabled: !window.maximized
                    cursorShape: Qt.SizeFDiagCursor
                    onPressed: window.startSystemResize(Qt.LeftEdge | Qt.TopEdge)
                }
                MouseArea {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    width: 8
                    height: 8
                    z: 121
                    enabled: !window.maximized
                    cursorShape: Qt.SizeBDiagCursor
                    onPressed: window.startSystemResize(Qt.RightEdge | Qt.TopEdge)
                }
                MouseArea {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    width: 8
                    height: 8
                    z: 121
                    enabled: !window.maximized
                    cursorShape: Qt.SizeBDiagCursor
                    onPressed: window.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
                }
                MouseArea {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    width: 8
                    height: 8
                    z: 121
                    enabled: !window.maximized
                    cursorShape: Qt.SizeFDiagCursor
                    onPressed: window.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
                }
            }
        }
    }

    AppMenu {
        id: mainMenu
        anchorItem: menuButton
        onUpdatesRequested: updateDialog.open()
        onSettingsRequested: window.activePage = "settings"
        onHelpRequested: helpDialog.open()
        onAboutRequested: aboutDialog.open()
    }

    ConfirmDialog {
        id: updateDialog
        title: "更新记录"
        message: "0.1.0\n\n• 首个纯 QML 预览版，无 WebEngine\n• 共享 aish 配置、sessions.db、工具与安全策略\n• 流式文本、深度思考、工具执行与审批卡片\n• 亮色 / 深色 / 跟随系统主题"
        confirmText: "知道了"
        cancelText: "关闭"
    }

    ConfirmDialog {
        id: helpDialog
        title: "帮助"
        message: "Enter 发送消息，Shift+Enter 换行。\nCtrl+N 新建对话，Ctrl+, 打开设置，Ctrl+Q 退出。\n点击工具卡片可查看参数与结果；工具执行前会请求审批。"
        confirmText: "知道了"
        cancelText: "关闭"
    }

    AboutDialog {
        id: aboutDialog
    }

    ApprovalDialog {
        id: approvalDialog
        onDecided: (decision) => app.resolveApproval(decision)
    }

    Component.onCompleted: {
        window.applyTheme()
        app.initialize()
        window.syncApproval()
    }
}
