// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import UosAiLibre

/// X11 shell for the FollowAlong content.
///
/// Tool + StaysOnTop + WindowDoesNotAcceptFocus is the X11 equivalent of the
/// semantic "overlay, not a normal window, never takes focus on show" request.
/// The execution layer sets the exact geometry; this file only hosts content.
Window {
    id: root
    property alias selectedText: content.selectedText
    property alias actions: content.actions
    property alias moreActions: content.moreActions
    property alias expanded: content.expanded
    readonly property bool menuOpen: content.menuOpen
    property alias menuAbove: content.menuAbove
    property alias menuHeight: content.menuHeight
    property int maxContentWidth: 1920

    signal actionTriggered(int type, bool custom)
    signal askRequested(string question)
    signal expandRequested()
    signal dismissRequested()
    signal settingsRequested()
    signal moreMenuRequested()

    width: content.implicitWidth
    height: content.implicitHeight
    visible: false
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
           | Qt.WindowDoesNotAcceptFocus
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
        inlineMenu: false
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
