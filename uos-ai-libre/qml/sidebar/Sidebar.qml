import QtQuick
import QtQuick.Controls
import UosAiLibre

Rectangle {
    id: sidebar
    property var appWindow
    property bool collapsed: false
    property bool chatActive: true
    property int cornerRadius: Theme.windowRadius
    property string renamingSessionId: ""
    property string renamingOriginal: ""
    property string renamingDraft: ""
    property bool showAllWorkspaces: false
    signal newChatRequested()
    signal tasksRequested()
    signal knowledgeRequested()
    signal sessionActivated(string sessionId)
    signal collapseRequested()
    signal toastRequested(string message)

    function workspaceKey(path) {
        const value = String(path || "").trim()
        if (value.length === 0)
            return ""
        const trimmed = value.replace(/\/+$/, "")
        return trimmed.length === 0 ? "/" : trimmed
    }

    function filteredSessions() {
        const source = app.sessions || []
        const current = workspaceKey(app.workspace)
        const visible = []
        for (let i = 0; i < source.length; ++i) {
            const item = source[i]
            if (!sidebar.showAllWorkspaces && current.length > 0
                && workspaceKey(item.cwd) !== current)
                continue
            visible.push(item)
        }
        visible.sort(function (left, right) {
            return String(right.updated_at || "").localeCompare(
                        String(left.updated_at || ""))
        })
        return visible
    }

    readonly property var visibleSessions: filteredSessions()

    width: Theme.sidebarWidth
    height: parent ? parent.height : 0
    color: "transparent"
    topLeftRadius: sidebar.cornerRadius
    bottomLeftRadius: sidebar.cornerRadius
    clip: true

    gradient: Gradient {
        GradientStop { position: 0.0; color: Theme.sidebarTop }
        GradientStop { position: 0.20; color: Theme.sidebarMid }
        GradientStop { position: 0.56; color: Theme.isDark ? "#1E2227" : "#F2F6FA" }
        GradientStop { position: 1.0; color: Theme.sidebarBottom }
    }

    Rectangle {
        width: parent.width
        height: 260
        topLeftRadius: sidebar.cornerRadius
        opacity: Theme.isDark ? 0.75 : 0.72
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.sidebarPink }
            GradientStop { position: 1.0; color: Qt.rgba(Theme.sidebarPink.r,
                                                         Theme.sidebarPink.g,
                                                         Theme.sidebarPink.b, 0) }
        }
    }

    Rectangle {
        y: parent.height - 240
        width: parent.width
        height: 240
        bottomLeftRadius: sidebar.cornerRadius
        opacity: Theme.isDark ? 0.65 : 0.9
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(Theme.sidebarBlue.r,
                                                         Theme.sidebarBlue.g,
                                                         Theme.sidebarBlue.b, 0) }
            GradientStop { position: 1.0; color: Theme.sidebarBlue }
        }
    }

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.isDark ? "#20FFFFFF" : "#12000000"
    }

    MouseArea {
        anchors.fill: parent
        z: -1
        enabled: sidebar.renamingSessionId.length > 0
        onClicked: sidebar.finishRename()
    }

    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 52
        acceptedButtons: Qt.LeftButton
        onPressed: {
            sidebar.finishRename()
            if (sidebar.appWindow)
                sidebar.appWindow.startSystemMove()
        }
    }

    Image {
        x: 18
        y: 18
        width: 28
        height: 28
        source: "qrc:/images/UosAiLibre.svg"
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    Item {
        x: 54
        y: 17
        width: 32
        height: 32
        visible: !sidebar.collapsed
        HoverHandler { id: brandHover }
        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusSmall
            color: brandHover.hovered ? Theme.hover : "transparent"
        }
        AppIcon {
            anchors.centerIn: parent
            name: "sidebar"
            size: 18
            color: Theme.icon
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                sidebar.finishRename()
                sidebar.collapseRequested()
            }
        }
    }

    SidebarItem {
        x: 8
        y: 58
        width: sidebar.width - 16
        icon: "new-chat"
        label: "创建新对话"
        rightIcon: "panel-right"
        onClicked: {
            sidebar.finishRename()
            sidebar.newChatRequested()
        }
    }

    SidebarItem {
        x: 8
        y: 92
        width: sidebar.width - 16
        icon: "tasks"
        label: "自动任务"
        onClicked: {
            sidebar.finishRename()
            sidebar.tasksRequested()
        }
    }

    SidebarItem {
        x: 8
        y: 126
        width: sidebar.width - 16
        icon: "knowledge"
        label: "AI知识库"
        onClicked: {
            sidebar.finishRename()
            sidebar.knowledgeRequested()
        }
    }

    Item {
        id: historyHeader
        x: 8
        y: 170
        width: sidebar.width - 16
        height: 26

        Text {
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            text: sidebar.showAllWorkspaces ? "全部工作区对话" : "工作区对话"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            font.weight: Font.Medium
        }

        IconButton {
            buttonSize: 26
            icon: "globe"
            iconSize: 14
            bordered: false
            iconColor: sidebar.showAllWorkspaces ? Theme.accent : Theme.iconSecondary
            hoverBackground: Theme.sidebarHover
            pressedBackground: Theme.sidebarPressed
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            onClicked: {
                sidebar.finishRename()
                sidebar.showAllWorkspaces = !sidebar.showAllWorkspaces
                sidebar.toastRequested(sidebar.showAllWorkspaces
                                       ? "已显示全部工作区的对话"
                                       : "仅显示当前工作区的对话")
            }
        }
    }

    ListView {
        id: historyList
        x: 8
        y: 198
        width: sidebar.width - 8
        height: Math.max(0, sidebar.height - y - 16)
        clip: true
        model: sidebar.visibleSessions
        spacing: 2
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {
            width: 6
            policy: ScrollBar.AsNeeded
        }

        delegate: HistoryRow {
            width: historyList.width - 12
            title: modelData.summary
            cwd: modelData.cwd || ""
            selected: sidebar.chatActive
                      && modelData.session_id === app.sessionId
            renaming: sidebar.renamingSessionId === modelData.session_id
            onActivated: {
                sidebar.finishRename()
                sidebar.sessionActivated(modelData.session_id)
            }
            onMenuRequested: (sceneY) => {
                sidebar.finishRename()
                sidebar.openContext(modelData, sceneY)
            }
            onRenameTextEdited: (text) => sidebar.renamingDraft = text
            onRenameCommitted: (text) => sidebar.commitRename(text)
            onRenameCancelled: sidebar.cancelRename()
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 20
            visible: historyList.count === 0
            text: sidebar.showAllWorkspaces ? "暂无对话" : "当前工作区暂无对话"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    property string contextSessionId
    property string contextSessionTitle

    // sceneY is in window/scene coordinates, so no sidebar offset is needed.
    function openContext(session, sceneY) {
        finishRename()
        contextSessionId = session.session_id
        contextSessionTitle = session.summary
        const anchorX = sidebar.mapToItem(Overlay.overlay,
                                          sidebar.width - 34, 0).x
        sessionMenu.x = Math.max(8, Math.min(Overlay.overlay.width - sessionMenu.width - 8,
                                               anchorX))
        sessionMenu.y = Math.max(8, Math.min(Overlay.overlay.height - sessionMenu.height - 8,
                                               sceneY - 6))
        sessionMenu.open()
    }

    function beginRename(session) {
        contextSessionId = session.session_id
        contextSessionTitle = session.summary
        renamingOriginal = session.summary
        renamingDraft = session.summary
        renamingSessionId = session.session_id
    }

    function finishRename() {
        if (renamingSessionId.length > 0)
            commitRename(renamingDraft)
    }

    function commitRename(text) {
        if (renamingSessionId.length === 0)
            return
        const sessionId = renamingSessionId
        const value = text.trim()
        renamingSessionId = ""
        renamingDraft = ""
        if (value.length > 0 && value !== renamingOriginal)
            app.renameSession(sessionId, value)
    }

    function cancelRename() {
        renamingSessionId = ""
        renamingDraft = ""
    }

    SessionMenu {
        id: sessionMenu
        parent: Overlay.overlay
        onRenameRequested: sidebar.beginRename({
            session_id: sidebar.contextSessionId,
            summary: sidebar.contextSessionTitle
        })
        onDeleteRequested: {
            deleteConfirm.message = "确定删除“" + sidebar.contextSessionTitle + "”吗？删除后无法恢复。"
            deleteConfirm.open()
        }
    }

    ConfirmDialog {
        id: deleteConfirm
        title: "删除对话"
        confirmText: "删除"
        danger: true
        onAccepted: app.deleteSession(sidebar.contextSessionId)
    }
}
