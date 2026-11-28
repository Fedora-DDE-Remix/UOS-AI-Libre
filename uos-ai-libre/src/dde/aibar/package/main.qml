// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.deepin.ds 1.0
import org.deepin.ds.dock 1.0

AppletDockItem {
    id: root

    // Same slot as UOS AI: after the tray (25), before Show Desktop (30).
    dockOrder: 26
    visible: shouldVisible

    readonly property bool useColumnLayout: Panel.position % 2
    readonly property real dockItemMaxSize: (Panel.rootObject && Panel.rootObject.dockItemMaxSize) ? Panel.rootObject.dockItemMaxSize : 48
    readonly property real dockSizeValue: (Panel.rootObject && Panel.rootObject.dockSize) ? Panel.rootObject.dockSize : 48
    readonly property real iconSize: Math.min(36, dockItemMaxSize * 2 / 3)

    implicitWidth: !shouldVisible ? 0 : (useColumnLayout ? dockSizeValue : iconSize + 10)
    implicitHeight: !shouldVisible ? 0 : (useColumnLayout ? iconSize + 10 : dockSizeValue)

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("UOS AI Libre")

    Image {
        id: logo
        anchors.centerIn: parent
        width: root.iconSize
        height: root.iconSize
        source: "UosAiLibre.svg"
        sourceSize: Qt.size(root.iconSize, root.iconSize)
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onClicked: Applet.onClickIcon()
    }
}
