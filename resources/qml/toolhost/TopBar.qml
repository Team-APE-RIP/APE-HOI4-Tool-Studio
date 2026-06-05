//-------------------------------------------------------------------------------------
// TopBar.qml -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import APE.ToolHost 1.0

Item {
    id: root

    property var hostState: ({})

    signal actionInvoked(string actionId)

    readonly property bool visibleState: hostState && hostState.visible !== undefined ? !!hostState.visible : true
    readonly property bool windowDragEnabled: hostState && hostState.windowDragEnabled !== undefined ? !!hostState.windowDragEnabled : false
    readonly property string leadingBadgeText: hostState && hostState.leadingBadgeText !== undefined ? String(hostState.leadingBadgeText) : ""
    readonly property var leftButtonItems: hostState && hostState.leftButtons ? hostState.leftButtons : []
    readonly property var rightButtonItems: hostState && hostState.rightButtons ? hostState.rightButtons : []
    readonly property var buttonItems: hostState && hostState.buttons ? hostState.buttons : []
    readonly property var metrics: HostTheme.metrics
    readonly property var surfaces: HostTheme.surfaces
    readonly property var dividerTokens: HostTheme.dividers.default
    readonly property var toolbarTokens: HostTheme.buttons.toolbar
    readonly property var badgeTokens: HostTheme.buttons.modeBadge
    readonly property var bodyFont: HostTheme.fonts.body

    component GlassButtonBackground: Item {
        id: material

        property var tokens: ({})
        property bool checkedState: false
        property bool hoveredState: false
        property bool pressedState: false
        property bool enabledState: true

        readonly property real materialRadius: Number(tokens.radius || 0)
        readonly property bool interactionState: hoveredState || pressedState

        opacity: enabledState ? 1.0 : 0.54

        function token(name, fallbackValue) {
            return tokens && tokens[name] !== undefined ? tokens[name] : fallbackValue
        }

        Rectangle {
            anchors.fill: parent
            radius: material.materialRadius
            color: material.checkedState
                ? material.token("backgroundChecked", "#990A84FF")
                : (material.interactionState
                       ? material.token("backgroundHover", "#4DE8E8ED")
                       : material.token("background", "transparent"))
        }

        Rectangle {
            anchors.fill: parent
            visible: material.checkedState
            radius: material.materialRadius
            opacity: material.pressedState ? 0.82 : 1.0
            gradient: Gradient {
                GradientStop { position: 0.00; color: material.token("checkedGlassTop", "#A96CCFFF") }
                GradientStop { position: 0.36; color: material.token("checkedGlassMiddle", "#820A84FF") }
                GradientStop { position: 1.00; color: material.token("checkedGlassBottom", "#B20066D6") }
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: material.checkedState
            radius: material.materialRadius
            opacity: material.hoveredState ? 0.72 : 0.52
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.00; color: "#00FFFFFF" }
                GradientStop { position: 0.24; color: material.token("checkedGlassSheen", "#3DFFFFFF") }
                GradientStop { position: 0.58; color: material.token("checkedGlassTint", "#2600E5FF") }
                GradientStop { position: 1.00; color: "#00000000" }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 1
            anchors.rightMargin: 1
            anchors.topMargin: 1
            visible: material.checkedState
            height: 1
            radius: 1
            color: material.token("checkedGlassHighlight", "#70FFFFFF")
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            visible: material.checkedState
            height: Math.max(2, parent.height * 0.42)
            radius: material.materialRadius
            opacity: material.pressedState ? 0.44 : 0.28
            gradient: Gradient {
                GradientStop { position: 0.00; color: "#00000000" }
                GradientStop { position: 1.00; color: material.token("checkedGlassShadow", "#26003580") }
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: material.materialRadius
            color: "transparent"
            border.width: material.checkedState || material.interactionState ? 1 : 0
            border.color: material.checkedState
                ? material.token("checkedGlassBorder", "#66FFFFFF")
                : material.token("hoverBorder", "#263C3C43")
        }
    }

    visible: visibleState
    height: visibleState ? Number(metrics.topBarHeight) : 0

    function buttonWidth(buttonData) {
        const widthValue = Number(buttonData && buttonData.width !== undefined ? buttonData.width : metrics.toolbarButtonWidth)
        return isNaN(widthValue) ? Number(metrics.toolbarButtonWidth) : widthValue
    }

    function beginWindowDragFromArea(area, mouse) {
        if (mouse.button !== Qt.LeftButton) {
            return
        }
        var globalPos = area.mapToGlobal(mouse.x, mouse.y)
        toolBridge.startWindowDrag(globalPos.x, globalPos.y)
        mouse.accepted = true
    }

    function updateWindowDragFromArea(area, mouse) {
        var globalPos = area.mapToGlobal(mouse.x, mouse.y)
        toolBridge.updateWindowDrag(globalPos.x, globalPos.y)
    }

    Rectangle {
        anchors.fill: parent
        color: surfaces.topBar

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Number(dividerTokens.thickness)
            color: dividerTokens.color
        }

        MouseArea {
            id: backgroundDragArea
            anchors.fill: parent
            enabled: windowDragEnabled
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            cursorShape: enabled ? Qt.SizeAllCursor : Qt.ArrowCursor

            onPressed: function(mouse) {
                root.beginWindowDragFromArea(backgroundDragArea, mouse)
            }

            onPositionChanged: function(mouse) {
                root.updateWindowDragFromArea(backgroundDragArea, mouse)
            }

            onReleased: {
                toolBridge.endWindowDrag()
            }

            onCanceled: {
                toolBridge.endWindowDrag()
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8

            Item {
                visible: leadingBadgeText.length > 0
                Layout.preferredWidth: Number(metrics.toolbarButtonWidth)
                Layout.preferredHeight: Number(metrics.toolbarButtonHeight)

                GlassButtonBackground {
                    anchors.fill: parent
                    tokens: badgeTokens
                    checkedState: true
                    hoveredState: false
                    pressedState: false
                    enabledState: true
                }

                MouseArea {
                    id: badgeDragArea
                    anchors.fill: parent
                    enabled: windowDragEnabled
                    acceptedButtons: Qt.LeftButton
                    preventStealing: true
                    cursorShape: enabled ? Qt.SizeAllCursor : Qt.ArrowCursor

                    onPressed: function(mouse) {
                        root.beginWindowDragFromArea(badgeDragArea, mouse)
                    }

                    onPositionChanged: function(mouse) {
                        root.updateWindowDragFromArea(badgeDragArea, mouse)
                    }

                    onReleased: {
                        toolBridge.endWindowDrag()
                    }

                    onCanceled: {
                        toolBridge.endWindowDrag()
                    }
                }

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: leadingBadgeText
                    color: badgeTokens.text
                    elide: Text.ElideRight
                    font.family: badgeTokens.fontFamily || bodyFont.family
                    font.pixelSize: Number(badgeTokens.fontPixelSize || bodyFont.pixelSize)
                    font.weight: Number(badgeTokens.weight || bodyFont.weight)
                }
            }

            Repeater {
                model: leftButtonItems

                delegate: Button {
                    id: leftActionButton

                    property var buttonData: modelData || ({})
                    readonly property string variantName: buttonData.variant !== undefined ? String(buttonData.variant) : "toolbar"
                    readonly property bool checkedState: buttonData.checked !== undefined ? !!buttonData.checked : false
                    readonly property bool visibleState: buttonData.visible !== undefined ? !!buttonData.visible : true
                    readonly property bool enabledState: buttonData.enabled !== undefined ? !!buttonData.enabled : true
                    readonly property var variantTokens: variantName === "modeBadge" ? badgeTokens : toolbarTokens

                    visible: visibleState
                    enabled: enabledState
                    focusPolicy: Qt.NoFocus
                    hoverEnabled: true
                    Layout.preferredWidth: root.buttonWidth(buttonData)
                    Layout.preferredHeight: Number(variantTokens.height || metrics.toolbarButtonHeight)
                    text: buttonData.text !== undefined ? String(buttonData.text) : ""

                    background: GlassButtonBackground {
                        tokens: leftActionButton.variantTokens
                        checkedState: leftActionButton.checkedState
                        hoveredState: leftActionButton.hovered
                        pressedState: leftActionButton.down
                        enabledState: leftActionButton.enabledState
                    }

                    contentItem: Text {
                        text: leftActionButton.text
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        color: leftActionButton.checkedState
                            ? leftActionButton.variantTokens.textChecked
                            : leftActionButton.variantTokens.text
                        font.family: leftActionButton.variantTokens.fontFamily || bodyFont.family
                        font.pixelSize: Number(leftActionButton.variantTokens.fontPixelSize || bodyFont.pixelSize)
                        font.weight: leftActionButton.checkedState
                            ? Number(leftActionButton.variantTokens.checkedWeight || leftActionButton.variantTokens.weight || bodyFont.weight)
                            : Number(leftActionButton.variantTokens.weight || bodyFont.weight)
                    }

                    onClicked: {
                        if (buttonData.actionId !== undefined) {
                            root.actionInvoked(String(buttonData.actionId))
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: parent.height

                MouseArea {
                    anchors.fill: parent
                    enabled: windowDragEnabled
                    acceptedButtons: Qt.LeftButton
                    preventStealing: true
                    cursorShape: enabled ? Qt.SizeAllCursor : Qt.ArrowCursor

                    onPressed: function(mouse) {
                        root.beginWindowDragFromArea(parent, mouse)
                    }

                    onPositionChanged: function(mouse) {
                        root.updateWindowDragFromArea(parent, mouse)
                    }

                    onReleased: {
                        toolBridge.endWindowDrag()
                    }

                    onCanceled: {
                        toolBridge.endWindowDrag()
                    }
                }
            }

            Repeater {
                model: buttonItems

                delegate: Button {
                    id: actionButton

                    property var buttonData: modelData || ({})
                    readonly property string variantName: buttonData.variant !== undefined ? String(buttonData.variant) : "toolbar"
                    readonly property bool checkedState: buttonData.checked !== undefined ? !!buttonData.checked : false
                    readonly property bool visibleState: buttonData.visible !== undefined ? !!buttonData.visible : true
                    readonly property bool enabledState: buttonData.enabled !== undefined ? !!buttonData.enabled : true
                    readonly property var variantTokens: variantName === "modeBadge" ? badgeTokens : toolbarTokens

                    visible: visibleState
                    enabled: enabledState
                    focusPolicy: Qt.NoFocus
                    hoverEnabled: true
                    Layout.preferredWidth: root.buttonWidth(buttonData)
                    Layout.preferredHeight: Number(variantTokens.height || metrics.toolbarButtonHeight)
                    text: buttonData.text !== undefined ? String(buttonData.text) : ""

                    background: GlassButtonBackground {
                        tokens: actionButton.variantTokens
                        checkedState: actionButton.checkedState
                        hoveredState: actionButton.hovered
                        pressedState: actionButton.down
                        enabledState: actionButton.enabledState
                    }

                    contentItem: Text {
                        text: actionButton.text
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        color: actionButton.checkedState
                            ? actionButton.variantTokens.textChecked
                            : actionButton.variantTokens.text
                        font.family: actionButton.variantTokens.fontFamily || bodyFont.family
                        font.pixelSize: Number(actionButton.variantTokens.fontPixelSize || bodyFont.pixelSize)
                        font.weight: actionButton.checkedState
                            ? Number(actionButton.variantTokens.checkedWeight || actionButton.variantTokens.weight || bodyFont.weight)
                            : Number(actionButton.variantTokens.weight || bodyFont.weight)
                    }

                    onClicked: {
                        if (buttonData.actionId !== undefined) {
                            root.actionInvoked(String(buttonData.actionId))
                        }
                    }
                }
            }

            Repeater {
                model: rightButtonItems

                delegate: Button {
                    id: rightActionButton

                    property var buttonData: modelData || ({})
                    readonly property string variantName: buttonData.variant !== undefined ? String(buttonData.variant) : "toolbar"
                    readonly property bool checkedState: buttonData.checked !== undefined ? !!buttonData.checked : false
                    readonly property bool visibleState: buttonData.visible !== undefined ? !!buttonData.visible : true
                    readonly property bool enabledState: buttonData.enabled !== undefined ? !!buttonData.enabled : true
                    readonly property var variantTokens: variantName === "modeBadge" ? badgeTokens : toolbarTokens

                    visible: visibleState
                    enabled: enabledState
                    focusPolicy: Qt.NoFocus
                    hoverEnabled: true
                    Layout.preferredWidth: root.buttonWidth(buttonData)
                    Layout.preferredHeight: Number(variantTokens.height || metrics.toolbarButtonHeight)
                    text: buttonData.text !== undefined ? String(buttonData.text) : ""

                    background: GlassButtonBackground {
                        tokens: rightActionButton.variantTokens
                        checkedState: rightActionButton.checkedState
                        hoveredState: rightActionButton.hovered
                        pressedState: rightActionButton.down
                        enabledState: rightActionButton.enabledState
                    }

                    contentItem: Text {
                        text: rightActionButton.text
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        color: rightActionButton.checkedState
                            ? rightActionButton.variantTokens.textChecked
                            : rightActionButton.variantTokens.text
                        font.family: rightActionButton.variantTokens.fontFamily || bodyFont.family
                        font.pixelSize: Number(rightActionButton.variantTokens.fontPixelSize || bodyFont.pixelSize)
                        font.weight: rightActionButton.checkedState
                            ? Number(rightActionButton.variantTokens.checkedWeight || rightActionButton.variantTokens.weight || bodyFont.weight)
                            : Number(rightActionButton.variantTokens.weight || bodyFont.weight)
                    }

                    onClicked: {
                        if (buttonData.actionId !== undefined) {
                            root.actionInvoked(String(buttonData.actionId))
                        }
                    }
                }
            }
        }
    }
}
