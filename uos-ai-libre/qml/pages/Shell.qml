import QtQuick
import Qt.labs.platform 1.1 as Platform

/// Persistent desktop shell that survives main-window destruction.
///
/// It owns the system tray entry and turns backend state changes into native
/// notifications while the window is hidden. It never talks to aish directly.
Item {
    id: shell
    visible: false
    width: 0
    height: 0
    property bool wasGenerating: false

    Platform.SystemTrayIcon {
        id: tray
        visible: false
        icon.source: "qrc:/images/UosAiLibre.svg"
        icon.name: "uos-ai-libre"
        tooltip: "UOS AI Libre"
        Component.onCompleted: visible = true

        menu: Platform.Menu {
            Platform.MenuItem {
                text: "显示主窗口"
                onTriggered: windowHost.showWindow()
            }
            Platform.MenuItem {
                text: "新建对话"
                onTriggered: windowHost.newChat()
            }
            Platform.MenuSeparator {}
            Platform.MenuItem {
                text: "退出 UOS AI Libre"
                onTriggered: windowHost.quitApp()
            }
        }

        onActivated: (reason) => {
            if (reason === Platform.SystemTrayIcon.Trigger
                    || reason === Platform.SystemTrayIcon.DoubleClick)
                windowHost.showWindow()
        }
    }

    function notifyApproval() {
        if (app.pendingApprovalCount <= 0 || windowHost.windowVisible)
            return
        if (!tray.available || !tray.supportsMessages)
            return
        const name = app.pendingApproval.tool_name
        tray.showMessage("需要确认",
                         name && name.length > 0 ? name : "工具操作",
                         Platform.SystemTrayIcon.Warning, 5000)
    }

    Connections {
        target: app
        function onPendingApprovalChanged() { shell.notifyApproval() }
        function onGeneratingChanged() {
            if (shell.wasGenerating && !app.generating && !windowHost.windowVisible
                    && tray.available && tray.supportsMessages)
                tray.showMessage("UOS AI Libre", "回答已完成",
                                 Platform.SystemTrayIcon.Information, 4000)
            shell.wasGenerating = app.generating
        }
    }

    Connections {
        target: windowHost
        function onWindowVisibleChanged() {
            if (!windowHost.windowVisible)
                shell.notifyApproval()
        }
    }

    Component.onCompleted: wasGenerating = app.generating
}
