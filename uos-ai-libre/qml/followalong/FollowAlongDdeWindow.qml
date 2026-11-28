// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import UosAiLibre

/// Treeland dde-shell overlay shell.
///
/// The window is an ordinary xdg-toplevel at the Qt/Wayland layer; Treeland's
/// dde-shell protocol adds overlay/skip/auto-placement metadata to it. This is
/// the same model UOS AI 3.3 uses, and it lets the compositor position the
/// surface relative to the real cursor even when the pointer is over a native
/// Wayland client (where XWayland coordinates are stale).
Window {
    id: root
    property alias selectedText: content.selectedText
    property alias actions: content.actions
    property alias moreActions: content.moreActions
    property alias expanded: content.expanded
    readonly property bool menuOpen: content.menuOpen
    readonly property var inputMaskRects: content.inputMaskRects
    property alias menuAbove: content.menuAbove
    property alias menuHeight: content.menuHeight
    property int maxContentWidth: 1920

    signal actionTriggered(int type, bool custom)
    signal askRequested(string question)
    signal expandRequested()
    signal dismissRequested()
    signal settingsRequested()
    signal moreMenuRequested()

    width: content.surfaceWidth
    height: content.surfaceHeight
    visible: false
    color: "transparent"
    // Keyboard focus is rejected by Treeland's dde-shell
    // accept_keyboard_focus=false in icon state; do not set
    // Qt::WindowDoesNotAcceptFocus here because it would also block the Ask AI
    // field after the toolbar expands.
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    title: qsTr("UOS AI FollowAlong")

    function openMoreMenu() {
        content.openMoreMenu()
    }

    function closeMoreMenu() {
        content.closeMoreMenu()
    }

    onVisibleChanged: {
        if (!visible)
            content.resetVisualState()
    }

    FollowAlongContent {
        id: content
        anchors.fill: parent
        inlineMenu: true
        reserveMenuSpace: true
        maxContentWidth: root.maxContentWidth
        onActionTriggered: function(type, custom) {
            root.actionTriggered(type, custom)
        }
        onAskRequested: function(question) {
            root.askRequested(question)
        }
        onExpandRequested: root.expandRequested()
        onDismissed: root.dismissRequested()
        onSettingsRequested: root.settingsRequested()
        onMoreMenuRequested: root.moreMenuRequested()
    }
}
