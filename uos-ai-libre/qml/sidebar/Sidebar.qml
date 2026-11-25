import QtQuick
import QtQuick.Controls
import UosAiLibre

Rectangle {
    id: sidebar
    property var appWindow
    property string activeSection: "chat"
    property bool collapsed: false
    property int cornerRadius: Theme.windowRadius
    property string renamingSessionId: ""
    property string renamingOriginal: ""
    signal newChatRequested()
    signal tasksRequested()
    signal knowledgeRequested()
    signal sessionActivated(string sessionId)
    signal collapseRequested()
    signal toastRequested(string message)

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
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 52
        acceptedButtons: Qt.LeftButton
        onPressed: {
            if (sidebar.appWindow)
                sidebar.appWindow.startSystemMove()
        }
    }

    Image {
        x: 18
        y: 18
        width: 28
        height: 28
        source: "qrc:/images/logo.png"
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
            onClicked: sidebar.collapseRequested()
        }
    }

    SidebarItem {
        x: 8
        y: 58
        width: sidebar.width - 16
        icon: "new-chat"
        label: "创建新对话"
        rightIcon: "panel-right"
        onClicked: sidebar.newChatRequested()
    }

    SidebarItem {
        x: 8
        y: 92
        width: sidebar.width - 16
        icon: "tasks"
        label: "自动任务"
        selected: sidebar.activeSection === "tasks"
        onClicked: sidebar.tasksRequested()
    }

    SidebarItem {
        x: 8
        y: 126
        width: sidebar.width - 16
        icon: "knowledge"
        label: "AI知识库"
        selected: sidebar.activeSection === "knowledge"
        onClicked: sidebar.knowledgeRequested()
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
            text: "历史对话"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            font.weight: Font.Medium
        }

        IconButton {
            buttonSize: 26
            icon: "history"
            iconSize: 14
            bordered: false
            iconColor: Theme.iconSecondary
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            onClicked: sidebar.toastRequested("历史对话按最近更新时间排序")
        }
    }

    ListView {
        id: historyList
        x: 8
        y: 198
        width: sidebar.width - 16
        height: Math.max(0, sidebar.height - y - 64)
        clip: true
        model: app.sessions
        spacing: 2
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        delegate: HistoryRow {
            width: historyList.width
            title: modelData.summary
            selected: modelData.session_id === app.sessionId
            renaming: sidebar.renamingSessionId === modelData.session_id
            onActivated: sidebar.sessionActivated(modelData.session_id)
            onMenuRequested: (sceneY) => sidebar.openContext(modelData, sceneY)
            onRenameRequested: sidebar.beginRename(modelData)
            onRenameCommitted: (text) => sidebar.commitRename(text)
            onRenameCancelled: sidebar.cancelRename()
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 20
            visible: historyList.count === 0
            text: "暂无历史对话"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    Item {
        id: accountRow
        x: 10
        y: sidebar.height - 48
        width: sidebar.width - 20
        height: 34
        HoverHandler { id: accountHover }
        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusSmall
            color: accountHover.hovered ? Theme.hover : "transparent"
        }
        Rectangle {
            x: 10
            anchors.verticalCenter: parent.verticalCenter
            width: 24
            height: 24
            radius: 12
            color: Theme.isDark ? "#3A3A42" : "#DADAE0"
            AppIcon {
                anchors.centerIn: parent
                name: "user"
                size: 14
                color: Theme.iconSecondary
            }
        }
        Text {
            x: 42
            anchors.verticalCenter: parent.verticalCenter
            text: "请登录"
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBase
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: sidebar.toastRequested("本地模式无需登录，所有会话保存在本机")
        }
    }

    property string contextSessionId
    property string contextSessionTitle

    // sceneY is in window/scene coordinates, so no sidebar offset is needed.
    function openContext(session, sceneY) {
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
        renamingSessionId = session.session_id
    }

    function commitRename(text) {
        if (renamingSessionId.length === 0)
            return
        const sessionId = renamingSessionId
        const value = text.trim()
        renamingSessionId = ""
        if (value.length > 0 && value !== renamingOriginal)
            app.renameSession(sessionId, value)
    }

    function cancelRename() {
        renamingSessionId = ""
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
