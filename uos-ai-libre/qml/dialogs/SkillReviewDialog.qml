import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root
    property string skillName: ""
    readonly property var review: app.skillReview || ({})
    readonly property var verify: review.verify || ({})
    readonly property var checks: verify.checks || []
    signal trustRequested(string name)
    signal removeRequested(string name)

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    width: Math.min(680, parent ? parent.width - 40 : 680)
    height: Math.min(590, parent ? parent.height - 32 : 590)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    function openFor(name) {
        skillName = name
        app.reviewSkill(name)
        open()
    }

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder
    }

    contentItem: Column {
        width: root.width
        height: root.height
        spacing: 0

        Item {
            width: parent.width
            height: 56
            Text {
                x: 22
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 90
                text: "技能审查 · " + (root.review.name || root.skillName)
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 17
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Rectangle {
                visible: root.review.quarantined === true
                anchors.right: parent.right
                anchors.rightMargin: 48
                anchors.verticalCenter: parent.verticalCenter
                width: 54
                height: 20
                radius: 4
                color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.12)
                Text {
                    anchors.centerIn: parent
                    text: "待信任"
                    color: Theme.warning
                    font.family: Theme.fontFamily
                    font.pixelSize: 9
                }
            }
            Rectangle {
                visible: root.review.name !== undefined && root.review.name !== ""
                         && root.review.quarantined !== true
                anchors.right: parent.right
                anchors.rightMargin: 48
                anchors.verticalCenter: parent.verticalCenter
                width: 54
                height: 20
                radius: 4
                color: Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.12)
                Text {
                    anchors.centerIn: parent
                    text: "已信任"
                    color: Theme.success
                    font.family: Theme.fontFamily
                    font.pixelSize: 9
                }
            }
            AppIcon {
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                name: "x"
                size: 14
                color: Theme.icon
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -8
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
            }
        }

        Rectangle { width: parent.width; height: 1; color: Theme.border }

        Flickable {
            width: parent.width
            height: parent.height - 56 - 1 - 60
            contentWidth: width
            contentHeight: body.implicitHeight + 30
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: body
                x: 22
                y: 14
                width: parent.width - 44
                spacing: 10

                Text {
                    width: parent.width
                    text: root.review.description || ""
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    lineHeight: 1.45
                }
                Text {
                    width: parent.width
                    text: root.review.path || ""
                    color: Theme.textTertiary
                    font.family: "DejaVu Sans Mono"
                    font.pixelSize: Theme.fontSizeTiny
                    elide: Text.ElideMiddle
                }

                Rectangle {
                    width: parent.width
                    height: verifyColumn.implicitHeight + 20
                    radius: Theme.radiusSmall
                    color: Theme.codeBg
                    border.width: 1
                    border.color: Theme.border
                    Column {
                        id: verifyColumn
                        x: 12
                        y: 10
                        width: parent.width - 24
                        spacing: 5
                        Text {
                            text: "校验结果 · " + (root.verify.valid === true ? "PASS" : "FAIL")
                            color: root.verify.valid === true ? Theme.success : Theme.danger
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: Font.Medium
                        }
                        Repeater {
                            model: root.checks
                            delegate: Row {
                                width: parent.width
                                spacing: 6
                                AppIcon {
                                    anchors.verticalCenter: parent.verticalCenter
                                    name: modelData.passed ? "check" : "warning"
                                    size: 12
                                    color: modelData.passed ? Theme.success : Theme.danger
                                }
                                Text {
                                    width: parent.width - 24
                                    text: modelData.label + (modelData.detail ? "  ·  " + modelData.detail : "")
                                    color: Theme.textTertiary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeTiny
                                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                }
                            }
                        }
                    }
                }

                Text {
                    text: "SKILL.md 内容（只读预览）"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: Font.Medium
                }
                Rectangle {
                    width: parent.width
                    height: Math.min(contentText.implicitHeight + 20, 260)
                    radius: Theme.radiusSmall
                    color: Theme.codeBg
                    border.width: 1
                    border.color: Theme.border
                    clip: true
                    Flickable {
                        anchors.fill: parent
                        anchors.margins: 10
                        contentWidth: width
                        contentHeight: contentText.implicitHeight
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        Text {
                            id: contentText
                            width: parent.width
                            text: root.review.content || "（无法读取内容）"
                            color: Theme.textSecondary
                            font.family: "DejaVu Sans Mono"
                            font.pixelSize: 11
                            wrapMode: Text.WrapAnywhere
                            lineHeight: 1.45
                        }
                    }
                }
                Text {
                    width: parent.width
                    visible: root.review.truncated === true
                    text: "内容超过 256 KB，仅显示前 256 KB。"
                    color: Theme.textTertiary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTiny
                }
                Text {
                    width: parent.width
                    visible: root.review.quarantined === true
                    text: "信任后该技能内容会进入 AI 上下文，可能引导 AI 执行命令。请先确认来源和内容，不要信任未知来源的技能。"
                    color: Theme.warning
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTiny
                    lineHeight: 1.45
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
            }
        }

        Rectangle { width: parent.width; height: 1; color: Theme.border }

        Item {
            width: parent.width
            height: 60
            Row {
                anchors.left: parent.left
                anchors.leftMargin: 22
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8
                AppButton {
                    text: "打开目录"
                    onClicked: app.openPath(root.review.base_dir || "")
                }
                AppButton {
                    text: "删除技能"
                    variant: "danger"
                    onClicked: root.removeRequested(root.review.name || root.skillName)
                }
            }
            Row {
                anchors.right: parent.right
                anchors.rightMargin: 22
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8
                AppButton { text: "关闭"; onClicked: root.close() }
                AppButton {
                    text: "信任并启用"
                    variant: "primary"
                    visible: root.review.quarantined === true
                    onClicked: root.trustRequested(root.review.name || root.skillName)
                }
            }
        }
    }
}
