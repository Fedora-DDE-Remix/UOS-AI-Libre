import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import UosAiLibre

Rectangle {
    id: root
    property string text: ""
    property var attachments: []
    property bool compact: false
    property bool showWorkspace: false
    property bool generating: false
    property string modelName: ""
    property string workspaceLabel: "默认工作空间"
    property bool webSearch: false
    property string placeholder: "请输入您的问题"
    property bool canSend: text.trim().length > 0 || attachments.length > 0

    signal sendRequested()
    signal stopRequested()
    signal settingsRequested()
    signal toastRequested(string message)

    function focusInput() {
        input.forceActiveFocus()
    }

    function clear() {
        text = ""
        attachments = []
    }

    function addAttachments(paths) {
        const next = attachments.slice()
        let rejected = 0
        for (const path of paths) {
            if (!path)
                continue
            if (next.indexOf(path) >= 0)
                continue
            if (next.length >= 4) {
                rejected++
                continue
            }
            next.push(path)
        }
        attachments = next
        if (rejected > 0)
            root.toastRequested("每条消息最多附加 4 张图片")
    }

    implicitHeight: column.implicitHeight
    height: implicitHeight
    radius: 14
    color: Theme.card
    border.width: 1
    border.color: input.activeFocus ? Theme.borderStrong : Theme.cardBorder

    onTextChanged: {
        if (input.text !== text)
            input.text = text
    }

    Column {
        id: column
        width: parent.width
        spacing: 0

        Item {
            width: parent.width
            height: root.attachments.length > 0 ? 62 : 0
            visible: height > 0

            Flickable {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.topMargin: 8
                contentWidth: attachmentRow.width
                contentHeight: attachmentRow.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Row {
                    id: attachmentRow
                    spacing: 8
                    Repeater {
                        model: root.attachments
                        delegate: Rectangle {
                            width: 48
                            height: 48
                            radius: Theme.radiusSmall
                            color: Theme.codeBg
                            border.width: 1
                            border.color: Theme.border

                            Image {
                                anchors.fill: parent
                                anchors.margins: 2
                                source: app.imageUrl(modelData)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: false
                                sourceSize: Qt.size(96, 96)
                                clip: true
                            }

                            Rectangle {
                                width: 16
                                height: 16
                                radius: 8
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.rightMargin: -4
                                anchors.topMargin: -4
                                color: "#B3000000"
                                AppIcon {
                                    anchors.centerIn: parent
                                    name: "x"
                                    size: 9
                                    color: "#FFFFFF"
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        const next = root.attachments.slice()
                                        next.splice(index, 1)
                                        root.attachments = next
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Item {
            id: inputArea
            width: parent.width
            height: input.height + 24

            TextArea {
                id: input
                x: 16
                y: 12
                width: parent.width - 32
                height: Math.min(root.compact ? 132 : 168,
                                  Math.max(root.compact ? 50 : 76, contentHeight))
                padding: 0
                leftPadding: 0
                rightPadding: 0
                topPadding: 0
                bottomPadding: 0
                placeholderText: root.placeholder
                placeholderTextColor: Theme.inputPlaceholder
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeChat
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                activeFocusOnTab: true
                background: null

                onTextChanged: {
                    if (root.text !== text)
                        root.text = text
                }

                Keys.onPressed: (event) => {
                    if (input.inputMethodComposing)
                        return
                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                            && !(event.modifiers & Qt.ShiftModifier)) {
                        event.accepted = true
                        root.sendRequested()
                    }
                }
            }
        }

        Item {
            id: controls
            width: parent.width
            height: 44

            Row {
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Item {
                    width: 30
                    height: 30
                    HoverHandler { id: plusHover }
                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusSmall
                        color: plusHover.hovered ? Theme.hover : "transparent"
                    }
                    AppIcon {
                        anchors.centerIn: parent
                        name: "plus"
                        size: 17
                        color: Theme.icon
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.openPlusMenu()
                    }
                }

                Item {
                    width: 30
                    height: 30
                    HoverHandler { id: globeHover }
                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusSmall
                        color: root.webSearch
                               ? Theme.accentSoft
                               : (globeHover.hovered ? Theme.hover : "transparent")
                    }
                    AppIcon {
                        anchors.centerIn: parent
                        name: "globe"
                        size: 16
                        color: root.webSearch ? Theme.accent : Theme.icon
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.webSearch = !root.webSearch
                            root.toastRequested(root.webSearch
                                ? "已开启联网检索，模型可按需调用搜索工具"
                                : "已关闭联网检索")
                        }
                    }
                }

                Rectangle {
                    visible: root.compact
                    width: visible ? permissionRow.width + 18 : 0
                    height: 26
                    radius: Theme.radiusSmall
                    color: permissionHover.hovered ? Theme.hover : "transparent"
                    border.width: 1
                    border.color: Theme.border

                    HoverHandler { id: permissionHover }

                    Row {
                        id: permissionRow
                        anchors.centerIn: parent
                        spacing: 5
                        AppIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: "shield"
                            size: 13
                            color: Theme.iconSecondary
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "默认权限"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        AppIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: "chevron-down"
                            size: 10
                            color: Theme.iconSecondary
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.openPermissionMenu()
                    }
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3

                Rectangle {
                    id: modelButton
                    width: modelRow.width + 16
                    height: 28
                    radius: Theme.radiusSmall
                    color: modelHover.hovered ? Theme.hover : "transparent"

                    HoverHandler { id: modelHover }

                    Row {
                        id: modelRow
                        anchors.centerIn: parent
                        spacing: 5
                        AppIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: "cloud"
                            size: 15
                            color: Theme.iconSecondary
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.modelName.length > 0 ? root.modelName : "选择模型"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight
                            width: Math.min(150, implicitWidth)
                        }
                        AppIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: "chevron-down"
                            size: 10
                            color: Theme.iconSecondary
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.openModelMenu()
                    }
                }

                Item {
                    width: 30
                    height: 30
                    HoverHandler { id: micHover }
                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusSmall
                        color: micHover.hovered ? Theme.hover : "transparent"
                    }
                    AppIcon {
                        anchors.centerIn: parent
                        name: "mic"
                        size: 16
                        color: Theme.iconSecondary
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.toastRequested("语音输入暂未接入")
                    }
                }

                Rectangle {
                    id: sendButton
                    width: 32
                    height: 32
                    radius: 16
                    color: root.generating
                           ? Theme.accent
                           : (root.canSend ? Theme.accent : Theme.accentDisabled)
                    Behavior on color { ColorAnimation { duration: Theme.durationFast } }

                    HoverHandler { id: sendHover }

                    AppIcon {
                        anchors.centerIn: parent
                        name: root.generating ? "stop" : "send"
                        size: root.generating ? 14 : 16
                        color: "#FFFFFF"
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.generating)
                                root.stopRequested()
                            else if (root.canSend)
                                root.sendRequested()
                        }
                    }
                }
            }
        }

        Rectangle {
            id: workspaceRow
            visible: root.showWorkspace
            width: parent.width
            height: visible ? 34 : 0
            color: Theme.isDark ? "#1B1B1F" : "#F7F7F8"
            radius: 13

            Rectangle {
                width: parent.width
                height: 13
                color: Theme.isDark ? "#1B1B1F" : "#F7F7F8"
            }

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 13
                anchors.rightMargin: 13
                height: 1
                color: Theme.border
            }

            Row {
                x: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 7
                AppIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "folder"
                    size: 15
                    color: Theme.iconSecondary
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.workspaceLabel
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
                AppIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "chevron-down"
                    size: 10
                    color: Theme.iconSecondary
                }
            }

            HoverHandler { id: workspaceHover }
            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusSmall
                color: workspaceHover.hovered ? Theme.hover : "transparent"
                opacity: 0.6
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.openWorkspaceMenu()
            }
        }
    }

    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (!drop.hasUrls)
                return
            const paths = []
            for (let i = 0; i < drop.urls.length; ++i) {
                const path = app.localFilePath(drop.urls[i])
                if (path.length > 0)
                    paths.push(path)
            }
            root.addAttachments(paths)
            drop.acceptProposedAction()
        }
    }

    function openPlusMenu() {
        plusMenu.x = Math.max(8, Math.min(Overlay.overlay.width - plusMenu.width - 8,
                                         plusButtonPoint()))
        plusMenu.y = Math.max(8, plusButtonY() - 126)
        plusMenu.open()
    }

    function plusButtonPoint() {
        const point = controls.mapToItem(Overlay.overlay, 8, 0)
        return point.x
    }

    function plusButtonY() {
        return controls.mapToItem(Overlay.overlay, 0, 0).y
    }

    function openModelMenu() {
        const point = modelButton.mapToItem(Overlay.overlay, 0, 0)
        modelMenu.open()
        modelMenu.x = Math.max(8, Math.min(Overlay.overlay.width - modelMenu.width - 8, point.x))
        modelMenu.y = Math.max(8, point.y - modelMenu.height - 6)
    }

    function openPermissionMenu() {
        const point = permissionRow.mapToItem(Overlay.overlay, 0, 0)
        permissionMenu.open()
        permissionMenu.x = Math.max(8, point.x)
        permissionMenu.y = Math.max(8, point.y - permissionMenu.height - 6)
    }

    function openWorkspaceMenu() {
        const point = workspaceRow.mapToItem(Overlay.overlay, 0, 0)
        workspaceMenu.open()
        workspaceMenu.x = Math.max(8, Math.min(Overlay.overlay.width - workspaceMenu.width - 8,
                                              point.x + 12))
        workspaceMenu.y = Math.max(8, point.y - workspaceMenu.height - 6)
    }

    Popup {
        id: plusMenu
        parent: Overlay.overlay
        width: 168
        padding: 0
        modal: false
        closePolicy: Popup.CloseOnEscape
        background: Rectangle {
            radius: Theme.radiusMedium
            color: Theme.card
            border.width: 1
            border.color: Theme.border
        }
        contentItem: Column {
            width: parent.width
            topPadding: 6
            bottomPadding: 6
            MenuRow {
                width: parent.width
                icon: "upload"
                label: "添加附件"
                showChevron: true
                highlighted: attachMenu.opened
                onClicked: {
                    attachMenu.x = plusMenu.x + plusMenu.width - 2
                    attachMenu.y = plusMenu.y + 8
                    attachMenu.open()
                }
            }
            MenuRow {
                width: parent.width
                icon: "skill"
                label: "技能"
                onClicked: {
                    plusMenu.close()
                    root.toastRequested("aish 技能会按任务自动加载")
                }
            }
            MenuRow {
                width: parent.width
                icon: "connector"
                label: "连接器"
                onClicked: {
                    plusMenu.close()
                    root.toastRequested("MCP 连接器由 aish 配置统一管理")
                }
            }
        }
    }

    Popup {
        id: attachMenu
        parent: Overlay.overlay
        width: 150
        padding: 0
        modal: false
        closePolicy: Popup.CloseOnEscape
        background: Rectangle {
            radius: Theme.radiusMedium
            color: Theme.card
            border.width: 1
            border.color: Theme.border
        }
        contentItem: Column {
            width: parent.width
            topPadding: 6
            bottomPadding: 6
            MenuRow {
                width: parent.width
                icon: "upload"
                label: "上传文件"
                onClicked: {
                    attachMenu.close()
                    plusMenu.close()
                    imageDialog.open()
                }
            }
            MenuRow {
                width: parent.width
                icon: "screenshot"
                label: "截图问答"
                onClicked: {
                    attachMenu.close()
                    plusMenu.close()
                    const path = app.takeScreenshot()
                    if (path.length > 0)
                        root.addAttachments([path])
                    else
                        root.toastRequested(app.lastError.length > 0
                                            ? app.lastError
                                            : "截图失败")
                }
            }
        }
    }

    MenuPopup {
        id: modelMenu
        parent: Overlay.overlay
        menuWidth: 190
        items: [
            { label: root.modelName.length > 0 ? root.modelName : "当前模型",
              icon: "cloud", checked: true },
            { separator: true },
            { label: "模型设置…", icon: "settings" }
        ]
        onItemClicked: (index) => {
            modelMenu.close()
            if (index === 2)
                root.settingsRequested()
        }
    }

    MenuPopup {
        id: permissionMenu
        parent: Overlay.overlay
        menuWidth: 160
        items: [
            { label: "默认权限", icon: "shield", checked: true },
            { label: "仅读取", icon: "file" },
            { label: "完全访问", icon: "lock" }
        ]
        onItemClicked: (index) => {
            permissionMenu.close()
            root.toastRequested("工具权限由 aish 安全策略与审批确认控制")
        }
    }

    MenuPopup {
        id: workspaceMenu
        parent: Overlay.overlay
        menuWidth: 190
        items: [
            { label: root.workspaceLabel, icon: "folder", checked: true },
            { separator: true },
            { label: "打开本地文件夹", icon: "workspace" },
            { label: "新建工作空间", icon: "plus" }
        ]
        onItemClicked: (index) => {
            workspaceMenu.close()
            if (index === 0)
                app.resetWorkspace()
            else if (index === 2)
                folderDialog.open()
            else if (index === 3)
                root.toastRequested("新建工作空间暂未接入")
        }
    }

    FileDialog {
        id: imageDialog
        title: "上传图片"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["图片 (*.png *.jpg *.jpeg *.webp *.gif *.bmp)"]
        onAccepted: {
            const paths = []
            for (let i = 0; i < selectedFiles.length; ++i) {
                const path = app.localFilePath(selectedFiles[i])
                if (path.length > 0)
                    paths.push(path)
            }
            root.addAttachments(paths)
        }
    }

    FolderDialog {
        id: folderDialog
        title: "选择工作空间"
        onAccepted: {
            const path = app.localDirectory(selectedFolder)
            if (path.length > 0)
                app.setWorkspace(path)
        }
    }
}
