// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import UosAiLibre

/// Compact UOS AI-style FollowAlong pill.
///
/// It starts as the 24 px AI logo. Clicking the logo opens the full pill
/// with four primary skills, more menu, Ask AI input and close button. The
/// pill is not draggable and closes when it loses keyboard focus.
Window {
    id: root
    property bool expanded: false
    property bool menuOpen: moreMenu.opened
    property string selectedText: ""
    property var actions: []

    width: 24
    height: 24
    visible: false
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    title: qsTr("UOS AI FollowAlong")

    function resizeToContent() {
        if (followAlong.cursorPositionAvailable()) {
            // On X11 the host sets position and size atomically so a window
            // manager cannot clip a resize at the old screen edge.
            if (expanded)
                followAlong.setToolbarSize(
                            Math.max(425, Math.min(760,
                                                   contentRow.implicitWidth + 18)),
                            40)
            else
                followAlong.setToolbarSize(24, 24)
            return
        }
        if (!expanded) {
            width = 24
            height = 24
            return
        }
        height = 40
        width = Math.max(425, Math.min(760, contentRow.implicitWidth + 18))
    }

    onExpandedChanged: {
        resizeToContent()
        // Row layout and Repeater items can settle one event-loop turn later.
        Qt.callLater(resizeToContent)
    }
    onActionsChanged: if (expanded) Qt.callLater(resizeToContent)
    onVisibleChanged: {
        if (visible)
            return
        if (moreMenu.opened)
            moreMenu.close()
        expanded = false
        askInput.text = ""
    }

    signal actionTriggered(int type, bool custom)
    signal dismissed()

    function submitAsk() {
        const question = askInput.text.trim()
        if (question.length === 0)
            return
        // onAccepted and the key handlers can both fire for one Return.
        // Clear first so the question is submitted exactly once.
        askInput.text = ""
        followAlong.ask(question)
        root.dismissed()
    }

    // Dismissal is driven by global pointer events (with an X11 position or
    // the Wayland local-press correlation). Do not use window activation as a
    // proxy for "clicked elsewhere" on the Treeland overlay role.

    Rectangle {
        id: pill
        anchors.fill: parent
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
                // Do not focus the Treeland overlay here: opening it must not
                // depend on the compositor granting keyboard focus.
                root.expanded = true
            }
        }

        Row {
            id: contentRow
            visible: root.expanded
            anchors.centerIn: parent
            spacing: 2
            onImplicitWidthChanged: {
                if (root.expanded && !followAlong.cursorPositionAvailable())
                    root.width = Math.min(760, implicitWidth + 18)
            }

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
                    onClicked: {
                        // On Treeland the overlay normally refuses keyboard
                        // focus; the menu needs it for arrow-key navigation.
                        followAlong.activateToolbar()
                        moreMenu.open()
                    }
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

    Menu {
        id: moreMenu
        popupType: Popup.Window
        width: 180
        x: Math.max(8, root.width - width - 8)
        y: root.height + 4
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
            model: followAlong.moreActions
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
        onActivated: root.dismissed()
    }
}
