import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root
    property var request: ({})
    property bool decisionMade: false
    signal decided(int decision)

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    width: 460
    padding: 0
    modal: true
    closePolicy: Popup.CloseOnEscape

    readonly property bool blocked: request.mode === "blocked" || request.allow === false
    readonly property string risk: request.risk ? request.risk : "MEDIUM"
    readonly property color riskColor: {
        if (risk === "HIGH" || risk === "DANGER")
            return Theme.danger
        if (risk === "LOW")
            return Theme.success
        return Theme.warning
    }

    onOpened: {
        decisionMade = false
        timeoutTimer.restart()
    }
    onClosed: {
        if (!decisionMade)
            decide(3)
    }

    function decide(decision) {
        if (root.decisionMade)
            return
        root.decisionMade = true
        timeoutTimer.stop()
        root.close()
        root.decided(decision)
    }

    Timer {
        id: timeoutTimer
        interval: 290000
        onTriggered: root.decide(3)
    }

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder
    }

    contentItem: Column {
        width: root.width
        topPadding: 24
        leftPadding: 24
        rightPadding: 24
        bottomPadding: 18
        spacing: 12

        Row {
            spacing: 12
            Rectangle {
                width: 38
                height: 38
                radius: 19
                color: root.blocked
                       ? (Theme.isDark ? "#462326" : "#FFECEC")
                       : (Theme.isDark ? "#47351D" : "#FFF4E3")
                AppIcon {
                    anchors.centerIn: parent
                    name: root.blocked ? "lock" : "warning"
                    size: 20
                    color: root.blocked ? Theme.danger : Theme.warning
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                Text {
                    text: root.blocked ? "操作已被安全策略阻止" : "需要你的确认"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }
                Text {
                    text: root.blocked
                          ? "aish 安全策略不允许执行该操作。"
                          : "AI 请求执行以下操作，请确认是否允许。"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
        }

        Row {
            spacing: 8
            Rectangle {
                height: 24
                radius: 5
                width: toolText.implicitWidth + 16
                color: Theme.codeBg
                border.width: 1
                border.color: Theme.border
                Text {
                    id: toolText
                    anchors.centerIn: parent
                    text: root.request.tool_name ? root.request.tool_name : "工具"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: Font.Medium
                }
            }
            Rectangle {
                height: 24
                radius: 5
                width: riskText.implicitWidth + 14
                color: Qt.rgba(root.riskColor.r, root.riskColor.g, root.riskColor.b, 0.12)
                Text {
                    id: riskText
                    anchors.centerIn: parent
                    text: root.risk
                    color: root.riskColor
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
            }
        }

        Rectangle {
            visible: root.request.target !== undefined && String(root.request.target).length > 0
            width: parent.width - 48
            height: Math.min(targetText.implicitHeight + 20, 128)
            radius: Theme.radiusSmall
            color: Theme.codeBg
            border.width: 1
            border.color: Theme.border

            Flickable {
                anchors.fill: parent
                anchors.margins: 10
                contentHeight: targetText.implicitHeight
                clip: true
                interactive: contentHeight > height
                Text {
                    id: targetText
                    width: parent.width
                    text: root.request.target !== undefined ? String(root.request.target) : ""
                    color: Theme.textPrimary
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: 12
                    lineHeight: 1.45
                    wrapMode: Text.WrapAnywhere
                }
            }
        }

        Text {
            visible: root.request.message !== undefined && String(root.request.message).length > 0
            width: parent.width - 48
            text: root.request.message !== undefined ? String(root.request.message) : ""
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            lineHeight: 1.5
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }

        Item { width: 1; height: 4 }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 24
            spacing: 10

            AppButton {
                text: root.blocked ? "知道了" : "拒绝"
                minWidth: 88
                onClicked: root.decide(3)
            }

            AppButton {
                visible: !root.blocked
                text: "仅本次允许"
                minWidth: 106
                variant: "primary"
                onClicked: root.decide(0)
            }

            AppButton {
                visible: !root.blocked
                text: "本会话允许"
                minWidth: 106
                variant: "primary"
                onClicked: root.decide(1)
            }
        }
    }
}
