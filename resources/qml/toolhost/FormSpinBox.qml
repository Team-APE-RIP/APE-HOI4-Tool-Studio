//-------------------------------------------------------------------------------------
// FormSpinBox.qml -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
import QtQuick 2.15
import QtQuick.Controls 2.15
import APE.ToolHost 1.0

SpinBox {
    id: control

    property string toolTipText: ""
    property int indicatorWidth: 28
    property int textHorizontalPadding: indicatorWidth + 2

    editable: true
    implicitWidth: 90
    implicitHeight: 32
    font.family: HostTheme.fonts.body.family
    font.pixelSize: Number(HostTheme.fonts.body.pixelSize)
    focusPolicy: Qt.StrongFocus

    function boundedValue(rawValue) {
        var minimum = Math.min(control.from, control.to)
        var maximum = Math.max(control.from, control.to)
        var numericValue = Number(rawValue)
        if (!isFinite(numericValue)) {
            numericValue = control.value
        }
        numericValue = Math.round(numericValue)
        return Math.max(minimum, Math.min(maximum, numericValue))
    }

    function setUserValue(rawValue) {
        var nextValue = boundedValue(rawValue)
        if (nextValue !== control.value) {
            control.value = nextValue
            control.valueModified()
            return
        }
        if (control.contentItem) {
            control.contentItem.text = control.textFromValue(control.value, control.locale)
        }
    }

    contentItem: TextInput {
        z: 2
        text: control.textFromValue(control.value, control.locale)
        font: control.font
        color: control.enabled ? HostTheme.colors.textPrimary : HostTheme.colors.textMuted
        selectionColor: HostTheme.colors.selection
        selectedTextColor: HostTheme.colors.textInverted
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        leftPadding: control.textHorizontalPadding
        rightPadding: control.textHorizontalPadding
        readOnly: !control.editable
        selectByMouse: true
        validator: IntValidator {
            bottom: Math.min(control.from, control.to)
            top: Math.max(control.from, control.to)
        }
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        onEditingFinished: control.setUserValue(text)
        Keys.onReturnPressed: control.setUserValue(text)
        Keys.onEnterPressed: control.setUserValue(text)
    }

    up.indicator: Item {
        z: 4
        x: control.mirrored ? 0 : parent.width - width
        width: control.indicatorWidth
        height: parent.height
        clip: true
        opacity: control.enabled && control.value < control.to ? 1.0 : 0.45

        Rectangle {
            x: control.mirrored ? 0 : -radius
            width: parent.width + radius
            height: parent.height
            radius: 6
            color: upMouse.pressed || upMouse.containsMouse ? HostTheme.colors.accentSoft : "transparent"
        }

        MouseArea {
            id: upMouse
            anchors.fill: parent
            enabled: control.enabled && control.value < control.to
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.PointingHandCursor
            hoverEnabled: true
            preventStealing: true
            onPressed: {
                mouse.accepted = true
                control.forceActiveFocus()
            }
            onClicked: {
                mouse.accepted = true
                control.setUserValue(control.value + control.stepSize)
            }
        }

        Text {
            anchors.centerIn: parent
            text: "+"
            color: control.enabled ? HostTheme.colors.textPrimary : HostTheme.colors.textMuted
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }
    }

    down.indicator: Item {
        z: 4
        x: control.mirrored ? parent.width - width : 0
        width: control.indicatorWidth
        height: parent.height
        clip: true
        opacity: control.enabled && control.value > control.from ? 1.0 : 0.45

        Rectangle {
            x: control.mirrored ? -radius : 0
            width: parent.width + radius
            height: parent.height
            radius: 6
            color: downMouse.pressed || downMouse.containsMouse ? HostTheme.colors.accentSoft : "transparent"
        }

        MouseArea {
            id: downMouse
            anchors.fill: parent
            enabled: control.enabled && control.value > control.from
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.PointingHandCursor
            hoverEnabled: true
            preventStealing: true
            onPressed: {
                mouse.accepted = true
                control.forceActiveFocus()
            }
            onClicked: {
                mouse.accepted = true
                control.setUserValue(control.value - control.stepSize)
            }
        }

        Text {
            anchors.centerIn: parent
            text: "-"
            color: control.enabled ? HostTheme.colors.textPrimary : HostTheme.colors.textMuted
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }
    }

    background: Rectangle {
        implicitWidth: 82
        implicitHeight: 32
        radius: 6
        color: HostTheme.colors.surface
        border.color: control.activeFocus ? HostTheme.colors.accent : HostTheme.dividers.default.color
        border.width: control.activeFocus ? 2 : 1
    }

    ThemedToolTip {
        target: control
        visible: control.hovered && control.toolTipText.length > 0
        text: control.toolTipText
    }
}
