// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.deepin.ds
import UosAiLibre

/// Treeland shell for the FollowAlong content.
///
/// DLayerShellWindow is dde-shell's Qt binding for wlr-layer-shell. The layer
/// role is installed by Qt's Wayland shell integration before the first map,
/// so this surface is never an xdg-toplevel and dde-shell never treats it as
/// an application window. Icon state uses KeyboardInteractivityNone; only the
/// expanded toolbar requests OnDemand focus.
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

    // Layer-shell placement is expressed as output-relative margins.
    property int surfaceX: 0
    property int surfaceY: 0
    property bool keyboardInteractive: false

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
    title: qsTr("UOS AI FollowAlong")

    // Keep QWindow x/y in sync with the layer margins so Qt maps local input
    // coordinates and popup positions correctly.
    x: screen ? screen.virtualX + surfaceX : surfaceX
    y: screen ? screen.virtualY + surfaceY : surfaceY

    DLayerShellWindow.anchors: DLayerShellWindow.AnchorTop
                                 | DLayerShellWindow.AnchorLeft
    DLayerShellWindow.layer: DLayerShellWindow.LayerOverlay
    DLayerShellWindow.exclusionZone: -1
    DLayerShellWindow.keyboardInteractivity: root.keyboardInteractive
        ? DLayerShellWindow.KeyboardInteractivityOnDemand
        : DLayerShellWindow.KeyboardInteractivityNone
    DLayerShellWindow.scope: "uos-ai-libre/followalong"
    DLayerShellWindow.closeOnDismissed: true
    DLayerShellWindow.leftMargin: root.surfaceX
    DLayerShellWindow.topMargin: root.surfaceY

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
