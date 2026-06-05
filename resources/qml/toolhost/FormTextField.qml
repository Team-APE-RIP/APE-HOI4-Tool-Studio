//-------------------------------------------------------------------------------------
// FormTextField.qml -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
import QtQuick 2.15
import QtQuick.Controls 2.15
import APE.ToolHost 1.0

TextField {
    id: control

    property bool invalid: false

    selectByMouse: true
    font.family: HostTheme.fonts.body.family
    font.pixelSize: Number(HostTheme.fonts.body.pixelSize)
    color: enabled ? HostTheme.colors.textPrimary : HostTheme.colors.textMuted
    placeholderTextColor: HostTheme.colors.textMuted
    selectionColor: HostTheme.colors.selection
    selectedTextColor: HostTheme.colors.textInverted
    verticalAlignment: Text.AlignVCenter
    padding: 0
    leftPadding: 10
    rightPadding: 10
    topPadding: 6
    bottomPadding: 6

    background: Rectangle {
        radius: 8
        color: HostTheme.colors.surface
        border.color: control.invalid ? HostTheme.colors.danger : HostTheme.dividers.default.color
        border.width: control.invalid ? 2 : 1
    }
}
