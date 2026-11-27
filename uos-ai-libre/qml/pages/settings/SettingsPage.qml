import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import UosAiLibre

Rectangle {
    id: settings
    property int cornerRadius: Theme.windowRadius
    property int currentIndex: 0
    signal closeRequested()
    signal toastRequested(string message)

    color: Theme.windowBg
    radius: cornerRadius

    property var navItems: [
        { label: "模型与供应商", icon: "settings" },
        { label: "安全与权限", icon: "shield" },
        { label: "工具与子代理", icon: "tool" },
        { label: "技能", icon: "skill" },
        { label: "MCP 连接器", icon: "connector" },
        { label: "记忆", icon: "knowledge" },
        { label: "审计日志", icon: "report" },
        { label: "使用协议", icon: "file" }
    ]

    Rectangle {
        id: nav
        width: 216
        height: parent.height
        color: Theme.isDark ? "#1B1B1F" : "#FBFBFC"
        topLeftRadius: settings.cornerRadius
        bottomLeftRadius: settings.cornerRadius

        Image {
            x: 20
            y: 20
            width: 26
            height: 26
            source: "qrc:/images/logo.png"
            fillMode: Image.PreserveAspectFit
        }

        Column {
            x: 8
            y: 62
            width: nav.width - 16
            spacing: 2

            Repeater {
                model: settings.navItems
                delegate: Item {
                    width: nav.width - 16
                    height: 36
                    HoverHandler { id: navHover }
                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusSmall
                        color: settings.currentIndex === index
                               ? Theme.selected
                               : (navHover.hovered ? Theme.hover : "transparent")
                    }
                    AppIcon {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        name: modelData.icon
                        size: 16
                        color: settings.currentIndex === index
                               ? Theme.textPrimary : Theme.icon
                    }
                    Text {
                        x: 38
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: settings.currentIndex === index
                               ? Theme.textPrimary : Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: settings.currentIndex === index
                                      ? Font.Medium : Font.Normal
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settings.currentIndex = index
                    }
                }
            }
        }
    }

    Item {
        x: nav.width
        width: parent.width - nav.width
        height: parent.height

        Item {
            id: header
            width: parent.width
            height: 58

            Item {
                x: 22
                y: 14
                width: 30
                height: 30
                HoverHandler { id: backHover }
                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radiusSmall
                    color: backHover.hovered ? Theme.hover : "transparent"
                }
                AppIcon {
                    anchors.centerIn: parent
                    name: "back"
                    size: 16
                    color: Theme.icon
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settings.closeRequested()
                }
            }

            Text {
                x: 62
                anchors.verticalCenter: parent.verticalCenter
                text: settings.navItems[settings.currentIndex].label
                color: Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.Medium
            }

            Item {
                visible: false
                width: 30
                height: 30
                anchors.right: parent.right
                anchors.rightMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                HoverHandler { id: closeHover }
                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radiusSmall
                    color: closeHover.hovered ? Theme.hover : "transparent"
                }
                AppIcon {
                    anchors.centerIn: parent
                    name: "x"
                    size: 15
                    color: Theme.icon
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settings.closeRequested()
                }
            }
        }

        StackLayout {
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            currentIndex: settings.currentIndex

            ModelSettings {}

            SecuritySettings {}

            ToolsSettings {}

            SkillsSettings {}

            McpSettings {}

            MemorySettings {}

            AuditSettings {}

            // 使用协议
            Flickable {
                clip: true
                contentHeight: agreementColumn.implicitHeight + 60
                Column {
                    id: agreementColumn
                    x: Math.max(24, (parent.width - width) / 2)
                    y: 10
                    width: Math.min(680, parent.width - 128)
                    spacing: 14
                    Text {
                        text: "UOS AI Libre 使用协议"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                    }
                    Text {
                        width: parent.width
                        text: "UOS AI Libre 是一个本地优先的桌面 AI 客户端，基于 aish 与 Qt Quick 构建。会话、配置和工具调用记录保存在本机。"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBase
                        lineHeight: 1.7
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                    Text {
                        width: parent.width
                        text: "AI 生成内容仅供参考，请自行甄别信息准确性。文件读取、网络访问和命令执行均受 aish 安全策略与审批确认约束。"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBase
                        lineHeight: 1.7
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                    Text {
                        width: parent.width
                        text: "本项目不包含云端账号、语音输入或浏览器内核。第三方模型服务的使用受其各自服务条款约束。"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBase
                        lineHeight: 1.7
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    }
                }
            }
        }
    }

}
