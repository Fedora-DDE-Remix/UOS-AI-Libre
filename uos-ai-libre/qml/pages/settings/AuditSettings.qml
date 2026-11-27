import QtQuick
import UosAiLibre

SettingsScroll {
    id: page

    property string auditQuery: ""
    property var filteredEvents: []

    onVisibleChanged: {
        if (visible) {
            app.refreshAudit()
            rebuild()
        }
    }

    Component.onCompleted: {
        if (visible) {
            app.refreshAudit()
            rebuild()
        }
    }

    function rebuild() {
        const source = app.auditEvents || []
        const query = auditQuery.trim().toLowerCase()
        if (query.length === 0) {
            filteredEvents = source
            return
        }
        const result = []
        for (let i = 0; i < source.length; ++i) {
            const item = source[i]
            const haystack = [
                item.event_type, item.command, item.ai_tool, item.ai_args,
                item.ai_result, item.decision, item.user_choice,
                item.matched_rule, item.risk_level, item.session_uuid,
                item.ts, item.outcome, item.user, item.host, item.source
            ].map(function (value) {
                return value === undefined || value === null ? "" : String(value)
            }).join(" ").toLowerCase()
            if (haystack.indexOf(query) >= 0)
                result.push(item)
        }
        filteredEvents = result
    }

    function decisionColor(decision) {
        if (decision === "block" || decision === "no")
            return Theme.danger
        if (decision === "confirm")
            return Theme.warning
        return Theme.success
    }

    Connections {
        target: app
        function onAuditEventsChanged() { page.rebuild() }
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
        width: parent.width
        spacing: 8

        AppTextField {
            width: Math.min(360, parent.width - 190)
            height: 32
            placeholder: "搜索事件、工具、命令、参数、决策…"
            onTextEdited: (value) => {
                page.auditQuery = value
                page.rebuild()
            }
        }
        AppButton {
            text: "刷新"
            minWidth: 76
            onClicked: app.refreshAudit()
        }
        AppButton {
            text: "清空"
            variant: "danger"
            minWidth: 76
            enabled: (app.auditEvents.length || 0) > 0
            onClicked: clearAuditDialog.open()
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: page.filteredEvents.length + " / "
                  + (app.auditEvents.length || 0) + " 条"
            color: Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    Repeater {
        model: page.filteredEvents
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
        visible: page.filteredEvents.length === 0
        text: page.auditQuery.length > 0
              ? "没有匹配的审计事件。"
              : "当前没有审计事件。"
        color: Theme.textTertiary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
    }

    ConfirmDialog {
        id: clearAuditDialog
        title: "清空审计日志"
        message: "确定清空全部审计记录？"
        detail: "将删除 aish sessions.db 中全部 audit_events，包括其它会话产生的记录。此操作无法恢复。"
        confirmText: "清空"
        danger: true
        confirmEnabled: !app.generating
        onConfirmed: app.clearAudit()
    }
}
