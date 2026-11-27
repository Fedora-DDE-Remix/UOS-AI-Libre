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
    property string workspaceLabel: "当前目录"
    property string placeholder: "请输入您的问题"
    property bool canSend: text.trim().length > 0 || attachments.length > 0

    signal sendRequested()
    signal stopRequested()
    signal settingsRequested()
    signal securityRequested()
    signal skillsRequested()
    signal mcpRequested()
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
                    width: policyPill.width
                    height: 30

                    Rectangle {
                        id: policyPill
                        anchors.verticalCenter: parent.verticalCenter
                        width: policyRow.width + 18
                        height: 26
                        radius: Theme.radiusSmall
                        color: policyHover.hovered ? Theme.hover : "transparent"
                        border.width: 1
                        border.color: Theme.border

                        HoverHandler { id: policyHover }

                        Row {
                            id: policyRow
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
                                text: String(app.policyInfo.default_risk_level || "low").toUpperCase()
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.securityRequested()
                        }
                    }
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3

                Item {
                    // The send button is 32px; center the shorter model control
                    // instead of letting Row top-align it.
                    width: modelButton.width
                    height: 32

                    Rectangle {
                        id: modelButton
                        anchors.verticalCenter: parent.verticalCenter
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
                    width: Math.min(implicitWidth, root.width - 70)
                    text: root.workspaceLabel
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    elide: Text.ElideMiddle
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
        attachMenu.close()
        plusMenu.x = Math.max(8, Math.min(Overlay.overlay.width - plusMenu.width - 8,
                                         plusButtonPoint()))
        plusMenu.y = Math.max(8, plusButtonY() - 126)
        plusMenu.open()
    }

    function openAttachSubmenu() {
        if (!plusMenu.opened || attachMenu.opened)
            return
        // Keep the parent menu open while the pointer is over the submenu.
        plusMenu.closePolicy = Popup.CloseOnEscape
        attachMenu.closingForHover = false
        let x = plusMenu.x + plusMenu.width - 2
        if (x + attachMenu.width > Overlay.overlay.width - 8)
            x = Math.max(8, plusMenu.x - attachMenu.width + 2)
        attachMenu.x = x
        attachMenu.y = Math.max(8, plusMenu.y + 8)
        attachMenu.open()
    }

    function closeAttachSubmenuForHover() {
        if (!attachMenu.opened || attachMenu.closingForHover)
            return
        // Hovering another row retracts the submenu but keeps the parent open.
        attachMenu.closingForHover = true
        attachMenu.close()
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
        modelMenu.width = Math.min(380, Overlay.overlay.width - 16)
        const available = Overlay.overlay.height - (point.y + modelButton.height + 14)
        modelMenu.height = Math.min(420, Math.max(190, available))
        modelMenu.x = Math.max(8, Math.min(Overlay.overlay.width - modelMenu.width - 8,
                                         point.x + modelButton.width - modelMenu.width))
        let menuY = point.y + modelButton.height + 6
        if (menuY + modelMenu.height > Overlay.overlay.height - 8)
            menuY = Math.max(8, Overlay.overlay.height - modelMenu.height - 8)
        modelMenu.y = menuY
        modelMenu.query = ""
        modelMenu.rebuild()
        modelMenu.open()
        modelMenu.focusSearch()
    }

    function selectChatModel(item) {
        if (!item)
            return
        modelMenu.close()
        if (item.sessionOnly === true)
            return
        const result = app.activateModel(item.apiBase, item.provider, item.model)
        if (!result || result.ok !== true)
            root.toastRequested(result && result.error ? result.error : "切换模型失败")
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
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: attachMenu.close()
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
                onHovered: root.openAttachSubmenu()
                onClicked: root.openAttachSubmenu()
            }
            MenuRow {
                width: parent.width
                icon: "skill"
                label: "技能"
                onHovered: root.closeAttachSubmenuForHover()
                onClicked: {
                    plusMenu.close()
                    root.skillsRequested()
                }
            }
            MenuRow {
                width: parent.width
                icon: "connector"
                label: "连接器"
                onHovered: root.closeAttachSubmenuForHover()
                onClicked: {
                    plusMenu.close()
                    root.mcpRequested()
                }
            }
        }
    }

    Popup {
        id: attachMenu
        parent: Overlay.overlay
        property bool closingForHover: false
        width: 150
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: {
            plusMenu.closePolicy = Popup.CloseOnEscape | Popup.CloseOnPressOutside
            if (closingForHover) {
                closingForHover = false
                return
            }
            if (plusMenu.opened)
                plusMenu.close()
        }
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

    Connections {
        target: Window.window
        function onActiveChanged() {
            if (Window.window && !Window.window.active) {
                plusMenu.close()
                attachMenu.close()
            }
        }
    }

    Popup {
        id: modelMenu
        parent: Overlay.overlay
        property string query: ""
        property var entries: []
        width: 380
        height: 420
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        function rebuild() {
            const q = query.trim().toLowerCase()
            const source = app.chatModels || []
            const result = []
            for (let i = 0; i < source.length; ++i) {
                const item = source[i]
                if (q.length > 0
                    && String(item.model || "").toLowerCase().indexOf(q) < 0
                    && String(item.provider || "").toLowerCase().indexOf(q) < 0)
                    continue
                result.push(item)
            }
            let hasCurrent = false
            for (let i = 0; i < result.length; ++i) {
                if (result[i].model === root.modelName)
                    hasCurrent = true
            }
            if (!hasCurrent && root.modelName.length > 0) {
                result.unshift({
                    model: root.modelName,
                    provider: "当前会话",
                    apiBase: "",
                    protocol: "",
                    sessionOnly: true
                })
            }
            entries = result
        }

        function focusSearch() {
            Qt.callLater(function () { modelSearch.forceInputFocus() })
        }

        Connections {
            target: app
            function onChatModelsChanged() {
                if (modelMenu.opened)
                    modelMenu.rebuild()
            }
        }

        background: Rectangle {
            radius: Theme.radiusMedium
            color: Theme.card
            border.width: 1
            border.color: Theme.border
        }

        contentItem: Column {
            width: modelMenu.width
            height: modelMenu.height
            spacing: 0

            Item {
                width: parent.width
                height: 46
                AppTextField {
                    id: modelSearch
                    x: 10
                    y: 8
                    width: parent.width - 20
                    height: 32
                    placeholder: "搜索模型或供应商"
                    onTextEdited: (value) => { modelMenu.query = value; modelMenu.rebuild() }
                    onAccepted: {
                        if (modelMenu.entries.length > 0)
                            root.selectChatModel(modelMenu.entries[0])
                    }
                }
            }

            Rectangle { width: parent.width; height: 1; color: Theme.border }

            Flickable {
                width: parent.width
                height: parent.height - 46 - 1 - 42
                contentWidth: width
                contentHeight: modelColumn.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Column {
                    id: modelColumn
                    width: parent.width
                    spacing: 2
                    Repeater {
                        model: modelMenu.entries
                        delegate: Rectangle {
                            width: parent.width
                            height: 38
                            color: root.modelName === modelData.model
                                   ? Theme.accentSoft
                                   : (modelRowHover.hovered ? Theme.hover : "transparent")
                            HoverHandler { id: modelRowHover }

                            Text {
                                x: 12
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 150
                                text: modelData.model
                                color: root.modelName === modelData.model
                                       ? Theme.accent : Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                                elide: Text.ElideRight
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.rightMargin: 30
                                anchors.verticalCenter: parent.verticalCenter
                                width: 120
                                horizontalAlignment: Text.AlignRight
                                text: modelData.provider || ""
                                color: Theme.textTertiary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeTiny
                                elide: Text.ElideRight
                            }
                            AppIcon {
                                visible: root.modelName === modelData.model
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                name: "check"
                                size: 12
                                color: Theme.accent
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.selectChatModel(modelData)
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        visible: modelMenu.entries.length === 0
                        text: modelMenu.query.length > 0 ? "没有匹配的模型" : "尚未启用模型"
                        color: Theme.textTertiary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        horizontalAlignment: Text.AlignHCenter
                        topPadding: 24
                    }
                }
            }

            Rectangle { width: parent.width; height: 1; color: Theme.border }

            Item {
                width: parent.width
                height: 42
                Text {
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "管理模型与供应商…"
                    color: Theme.accent
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            modelMenu.close()
                            root.settingsRequested()
                        }
                    }
                }
            }
        }
    }

    MenuPopup {
        id: workspaceMenu
        parent: Overlay.overlay
        menuWidth: 210
        items: [
            { label: app.workspace && app.workspace.length > 0 ? app.workspace : root.workspaceLabel,
              icon: "folder", checked: true },
            { separator: true },
            { label: "选择目录…", icon: "folder" },
            { label: "打开当前目录", icon: "workspace" },
            { label: "复制目录路径", icon: "copy" }
        ]
        onItemClicked: (index) => {
            workspaceMenu.close()
            if (index === 2)
                folderDialog.open()
            else if (index === 3)
                app.openPath(app.workspace)
            else if (index === 4)
                app.copyText(app.workspace)
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
