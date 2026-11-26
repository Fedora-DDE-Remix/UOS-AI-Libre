import QtQuick
import UosAiLibre

SettingsScroll {
    id: page

    onVisibleChanged: { if (visible) app.refreshAudit() }

    function decisionColor(decision) {
        if (decision === "block" || decision === "no")
            return Theme.danger
        if (decision === "confirm")
            return Theme.warning
        return Theme.success
    }

    Text {
        text: "审计日志"
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: 18
        font.weight: Font.DemiBold
    }
    Text {
        width: parent.width
        text: "来自 aish 共享 sessions.db 的 audit_events，记录工具调用、安全决策和用户选择。"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        lineHeight: 1.5
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    }

    Row {
        spacing: 8
        AppButton { text: "刷新"; onClicked: app.refreshAudit() }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: (app.auditEvents.length || 0) + " 条"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    Repeater {
        model: app.auditEvents
        delegate: Rectangle {
            width: parent.width
            height: auditColumn.implicitHeight + 22
            radius: Theme.radiusMedium
            color: Theme.card
            border.width: 1
            border.color: Theme.cardBorder

            Column {
                id: auditColumn
                x: 16
                y: 11
                width: parent.width - 32
                spacing: 4
                Row {
                    width: parent.width
                    spacing: 8
                    Text {
                        text: modelData.event_type || "event"
                        color: Theme.textPrimary
                        font.family: "DejaVu Sans Mono"
                        font.pixelSize: Theme.fontSizeTiny
                        font.weight: Font.DemiBold
                    }
                    Rectangle {
                        visible: modelData.decision !== undefined && modelData.decision !== null
                        width: decisionText.implicitWidth + 12
                        height: 18
                        radius: 4
                        color: Qt.rgba(page.decisionColor(modelData.decision).r,
                                       page.decisionColor(modelData.decision).g,
                                       page.decisionColor(modelData.decision).b, 0.12)
                        Text {
                            id: decisionText
                            anchors.centerIn: parent
                            text: modelData.decision || ""
                            color: page.decisionColor(modelData.decision)
                            font.family: "DejaVu Sans Mono"
                            font.pixelSize: 9
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 150
                        text: modelData.ai_tool || modelData.command || ""
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTiny
                        elide: Text.ElideRight
                    }
                }
                Text {
                    width: parent.width
                    visible: modelData.ai_args !== undefined && String(modelData.ai_args).length > 0
                    text: modelData.ai_args || ""
                    color: Theme.textTertiary
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: Theme.fontSizeTiny
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: (modelData.ts || "") + "   ·   " +
                          (modelData.risk_level || "") + "   ·   " +
                          (modelData.user_choice || "")
                    color: Theme.textTertiary
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: Theme.fontSizeTiny
                    elide: Text.ElideMiddle
                }
            }
        }
    }

    Text {
        width: parent.width
        visible: app.auditEvents.length === 0
        text: "当前没有审计事件。"
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
    }
}
