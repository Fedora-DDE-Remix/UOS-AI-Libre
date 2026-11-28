// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import UosAiLibre

/// Platform-neutral FollowAlong content.
///
/// This file owns only the 24 px AI mark, the 425 px toolbar and the Ask AI
/// field. The "more" menu is drawn inside the same surface instead of using a
/// separate popup window: a layer-shell popup is a second surface that Treeland
/// may refuse to focus, which would leave the menu visible but unable to
/// receive clicks. The execution backend only sees a height change and keeps
/// the pill anchored.
Item {
    id: root
    property bool expanded: false
    property string selectedText: ""
    property var actions: []
    property var moreActions: []
    property int maxContentWidth: 1920
    property bool inlineMenu: false
    property bool inlineMenuOpen: false
    property bool menuAbove: false
    readonly property bool menuOpen: inlineMenu ? inlineMenuOpen : moreMenu.opened

    signal actionTriggered(int type, bool custom)
    signal dismissed()
    signal askRequested(string question)
    signal settingsRequested()
    signal expandRequested()
    signal moreMenuRequested()

    readonly property int pillHeight: 40
    readonly property int menuGap: 6
    readonly property int menuHeight: menuColumn.implicitHeight + 10
    readonly property var hostWindow: Window.window

    implicitWidth: expanded ? toolbarWidth() : 24
    implicitHeight: !expanded ? 24
        : inlineMenu && inlineMenuOpen
            ? pillHeight + menuGap + menuHeight
            : pillHeight

    function toolbarWidth() {
        let value = Math.max(425, Math.min(760, contentRow.implicitWidth + 18))
        if (maxContentWidth > 0)
            value = Math.min(value, maxContentWidth)
        return Math.max(24, value)
    }

    function openMoreMenu() {
        // The execution backend sets menuAbove from the real anchor/screen
        // geometry before calling this function.
        if (!root.expanded)
            return
        followAlong.activateToolbar()
        if (inlineMenu)
            inlineMenuOpen = true
        else
            moreMenu.open()
    }

    function closeMoreMenu() {
        inlineMenuOpen = false
        if (moreMenu.opened)
            moreMenu.close()
    }

    function closeMenuOrDismiss() {
        if (menuOpen)
            closeMoreMenu()
        else
            dismissed()
    }

    function submitAsk() {
        const question = askInput.text.trim()
        if (question.length === 0)
            return
        // onAccepted and the key handlers can both fire for one Return.
        // Clear first so the question is submitted exactly once.
        askInput.text = ""
        root.askRequested(question)
        root.dismissed()
    }

    function resetVisualState() {
        inlineMenuOpen = false
        menuAbove = false
        if (moreMenu.opened)
            moreMenu.close()
        expanded = false
        askInput.text = ""
    }

    // Clicking the transparent area created by an inline menu dismisses the
    // toolbar instead of silently swallowing the click.
    MouseArea {
        anchors.fill: parent
        enabled: root.inlineMenu && root.inlineMenuOpen
        onClicked: root.dismissed()
    }

    Rectangle {
        id: pill
        anchors.left: parent.left
        anchors.right: parent.right
        y: root.inlineMenu && root.inlineMenuOpen && root.menuAbove
            ? root.height - root.pillHeight : 0
        height: root.expanded ? root.pillHeight : 24
        radius: root.expanded ? 10 : 12
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder

        // Collapsed state: only the AI mark.
        Image {
            visible: !root.expanded
            anchors.centerIn: parent
            width: 20
            height: 20
            source: "qrc:/images/UosAiLibre.svg"
            sourceSize: Qt.size(20, 20)
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        MouseArea {
            anchors.fill: parent
            enabled: !root.expanded
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                // The icon must be clicked explicitly; hover never expands.
                expanded = true
                root.expandRequested()
            }
        }

        Row {
            id: contentRow
            visible: root.expanded
            anchors.centerIn: parent
            spacing: 2

            Image {
                anchors.verticalCenter: parent.verticalCenter
                width: 20
                height: 20
                source: "qrc:/images/UosAiLibre.svg"
                sourceSize: Qt.size(20, 20)
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Repeater {
                model: root.actions
                delegate: Rectangle {
                    id: actionButton
                    width: actionLabel.implicitWidth + 34
                    height: 30
                    radius: 7
                    color: actionHover.hovered ? Theme.hover : "transparent"

                    Row {
                        anchors.centerIn: parent
                        spacing: 4

                        AppIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 16
                            height: 16
                            name: modelData.icon
                            color: Theme.icon
                        }
                        Text {
                            id: actionLabel
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.name
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }

                    HoverHandler { id: actionHover }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.actionTriggered(modelData.type,
                                                        modelData.custom)
                    }
                }
            }

            Rectangle {
                width: 1
                height: 18
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.border
            }

            Rectangle {
                id: moreButton
                width: 26
                height: 30
                radius: 7
                color: moreHover.hovered ? Theme.hover : "transparent"

                AppIcon {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    name: "chevron-down"
                    color: Theme.icon
                }

                HoverHandler { id: moreHover }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.moreMenuRequested()
                }
            }

            Rectangle {
                width: 86
                height: 28
                radius: 6
                color: Theme.codeBg

                TextField {
                    id: askInput
                    anchors.fill: parent
                    leftPadding: 8
                    rightPadding: 6
                    verticalAlignment: TextInput.AlignVCenter
                    color: Theme.textPrimary
                    selectionColor: Theme.accent
                    selectedTextColor: "#FFFFFF"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    clip: true
                    placeholderText: "Ask AI"
                    background: null
                    onPressed: followAlong.activateToolbar()
                    onAccepted: root.submitAsk()
                    Keys.onReturnPressed: root.submitAsk()
                    Keys.onEnterPressed: root.submitAsk()
                }
            }

            Rectangle {
                id: closeButton
                width: 26
                height: 30
                radius: 7
                color: closeHover.hovered ? Theme.hover : "transparent"

                AppIcon {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    name: "x"
                    color: Theme.icon
                }

                HoverHandler { id: closeHover }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.dismissed()
                }
            }
        }
    }

    Rectangle {
        id: inlineMoreMenu
        visible: root.inlineMenu && root.inlineMenuOpen
        width: 180
        x: Math.max(8, root.width - width - 8)
        y: root.menuAbove ? 0 : root.pillHeight + root.menuGap
        height: root.menuHeight
        color: Theme.menuBackground
        radius: 8
        border.width: 1
        border.color: Theme.menuBorder

        // Swallow clicks on the panel background, and close on empty space.
        MouseArea {
            anchors.fill: parent
            onClicked: root.closeMoreMenu()
        }

        Column {
            id: menuColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 5
            spacing: 0

            Repeater {
                model: root.moreActions
                delegate: Rectangle {
                    width: menuColumn.width
                    height: 32
                    radius: 5
                    color: itemHover.hovered ? Theme.hover : "transparent"

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 9
                        text: modelData.name
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    HoverHandler { id: itemHover }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            const type = modelData.type
                            const custom = modelData.custom
                            root.closeMoreMenu()
                            root.actionTriggered(type, custom)
                        }
                    }
                }
            }

            Rectangle {
                width: menuColumn.width
                height: 1
                color: Theme.border
            }

            Rectangle {
                width: menuColumn.width
                height: 32
                radius: 5
                color: settingsHover.hovered ? Theme.hover : "transparent"

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 9
                    text: "随航设置"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }

                HoverHandler { id: settingsHover }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.closeMoreMenu()
                        followAlong.openSettings()
                        root.dismissed()
                    }
                }
            }
        }
    }

    // X11 uses a real popup menu: it is a managed Qt popup and does not
    // enlarge the FollowAlong window or cover the desktop with a transparent
    // rectangle. Treeland uses the inline menu above because its layer-shell
    // popup cannot be reliably focused by the compositor.
    Menu {
        id: moreMenu
        popupType: Popup.Window
        width: 180
        x: Math.max(8, root.width - width - 8)
        y: {
            const menuHeight = moreMenu.implicitHeight > 0
                    ? moreMenu.implicitHeight : 200
            const host = root.hostWindow
            if (host && host.screen) {
                const screenBottom = host.screen.virtualY + host.screen.height
                if (host.y + host.height + 4 + menuHeight > screenBottom)
                    return -menuHeight - 4
            }
            return root.height + 4
        }
        palette.window: Theme.menuBackground
        palette.windowText: Theme.textPrimary
        palette.text: Theme.textPrimary
        palette.button: Theme.menuBackground
        palette.buttonText: Theme.textPrimary
        palette.highlight: Theme.hover
        palette.highlightedText: Theme.textPrimary
        background: Rectangle {
            implicitWidth: 180
            color: Theme.menuBackground
            radius: 8
            border.width: 1
            border.color: Theme.menuBorder
        }
        Repeater {
            model: root.moreActions
            MenuItem {
                height: 32
                text: modelData.name
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                onTriggered: root.actionTriggered(modelData.type,
                                                  modelData.custom)
            }
        }
        MenuSeparator {}
        MenuItem {
            height: 32
            text: "随航设置"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            onTriggered: {
                followAlong.openSettings()
                root.dismissed()
            }
        }
    }

    Shortcut {
        sequence: "Esc"
        onActivated: root.closeMenuOrDismiss()
    }
}
