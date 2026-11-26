import QtQuick
import QtQuick.Controls
import UosAiLibre

Popup {
    id: root
    property var request: ({})
    property bool decisionMade: false
    property string selectedValue: ""
    property string selectedLabel: ""
    property string selectedDescription: ""
    property string freeText: ""
    signal decided(var response)

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    width: 500
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    readonly property var options: request.options || []
    readonly property bool allowFreeform: request.allow_freeform_input !== false
    readonly property bool allowCancel: request.allow_cancel !== false
    readonly property bool canConfirm: {
        if (!request.required)
            return true
        return selectedValue.length > 0 || freeText.trim().length >= Number(request.min_length || 0)
    }

    onOpened: {
        decisionMade = false
        selectedValue = ""
        selectedLabel = ""
        selectedDescription = ""
        freeText = ""
        let matchedDefault = false
        for (let i = 0; i < options.length; ++i) {
            if (request.default !== undefined && options[i].value === request.default) {
                selectedValue = options[i].value
                selectedLabel = options[i].label
                selectedDescription = options[i].description || ""
                matchedDefault = true
                break
            }
        }
        if (!matchedDefault && request.default !== undefined)
            freeText = String(request.default)
    }

    onClosed: {
        if (!decisionMade)
            decideCancelled()
    }

    function decideCancelled() {
        if (decisionMade)
            return
        decisionMade = true
        close()
        decided({cancelled: true})
    }

    function confirm() {
        if (!canConfirm || decisionMade)
            return
        decisionMade = true
        close()
        if (freeText.trim().length > 0) {
            decided({text: freeText})
        } else {
            decided({selected: {
                value: selectedValue,
                label: selectedLabel,
                description: selectedDescription
            }})
        }
    }

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.card
        border.width: 1
        border.color: Theme.cardBorder
    }

    contentItem: Column {
        width: root.width
        topPadding: 22
        leftPadding: 24
        rightPadding: 24
        bottomPadding: 18
        spacing: 12

        Text {
            width: root.width - 48
            text: root.request.title && String(root.request.title).length > 0
                  ? String(root.request.title) : "AI 需要你的确认"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 17
            font.weight: Font.DemiBold
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }

        Text {
            width: root.width - 48
            text: root.request.prompt || ""
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            lineHeight: 1.5
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }

        Column {
            width: parent.width - 48
            spacing: 6
            Repeater {
                model: root.options
                delegate: Rectangle {
                    width: parent.width
                    height: optionColumn.implicitHeight + 18
                    radius: Theme.radiusSmall
                    color: root.selectedValue === modelData.value
                           ? Theme.accentSoft
                           : (optionHover.hovered ? Theme.hover : Theme.codeBg)
                    border.width: 1
                    border.color: root.selectedValue === modelData.value
                                  ? Theme.accent : Theme.border
                    HoverHandler { id: optionHover }
                    Column {
                        id: optionColumn
                        x: 12
                        y: 9
                        width: parent.width - 24
                        spacing: 2
                        Row {
                            spacing: 6
                            Text {
                                text: modelData.label
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeBase
                                font.weight: root.selectedValue === modelData.value
                                             ? Font.Medium : Font.Normal
                            }
                            Rectangle {
                                visible: modelData.recommended === true
                                width: recommendedText.implicitWidth + 10
                                height: 18
                                radius: 3
                                color: Theme.accentSoft
                                Text {
                                    id: recommendedText
                                    anchors.centerIn: parent
                                    text: "推荐"
                                    color: Theme.accent
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 9
                                }
                            }
                        }
                        Text {
                            width: parent.width
                            visible: modelData.description !== undefined
                                     && String(modelData.description).length > 0
                            text: modelData.description || ""
                            color: Theme.textTertiary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.selectedValue = modelData.value
                            root.selectedLabel = modelData.label
                            root.selectedDescription = modelData.description || ""
                        }
                    }
                }
            }
        }

        AppTextField {
            width: parent.width - 48
            height: 38
            visible: root.allowFreeform
            text: root.freeText
            placeholder: root.request.placeholder || "输入回答…"
            onTextChanged: root.freeText = text
        }

        Item { width: 1; height: 2 }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 24
            spacing: 10

            AppButton {
                visible: root.allowCancel
                text: "取消"
                minWidth: 82
                onClicked: root.decideCancelled()
            }

            AppButton {
                text: "确定"
                minWidth: 82
                variant: "primary"
                enabled: root.canConfirm
                onClicked: root.confirm()
            }
        }
    }
}
