// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import UosAiLibre

SettingsScroll {
    id: page

    Text {
        text: "AI 随航"
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: 18
        font.weight: Font.DemiBold
    }
    Text {
        width: parent.width
        text: "在系统界面中选中文字后，会在光标附近显示 AI 随航工具栏。搜索会调用系统浏览器，解释、总结和翻译会把选中文字交给当前 aish 会话处理。"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Rectangle {
        width: parent.width
        height: 58
        radius: Theme.radiusMedium
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text {
                    text: "选中文字时显示随航工具栏"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBase
                }
                Text {
                    text: "关闭后仍可通过重新开启恢复"
                    color: Theme.textTertiary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTiny
                }
            }
        }

        ToggleSwitch {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            checked: followAlong.enabled
            onToggled: (checked) => followAlong.enabled = checked
        }
    }

    Text {
        width: parent.width
        text: "随航技能"
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBody
        font.weight: Font.Medium
    }

    Rectangle {
        width: parent.width
        height: 48
        radius: Theme.radiusMedium
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10
            Repeater {
                model: followAlong.actions
                delegate: Rectangle {
                    width: 54
                    height: 28
                    radius: 6
                    color: Theme.codeBg
                    Text {
                        anchors.centerIn: parent
                        text: modelData.name
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }
        }
    }

    Text {
        width: parent.width
        text: "X11 使用 PRIMARY 选区和全局鼠标位置；Wayland/Treeland 使用 wlr-data-control 监听选区，并通过 Treeland 私有 overlay 协议把工具栏放在光标附近。"
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeTiny
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }
}
