//-------------------------------------------------------------------------------------
// FormCheckBox.qml -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
import QtQuick 2.15
import QtQuick.Controls 2.15
import APE.ToolHost 1.0

AbstractButton {
    id: control

    property int indicatorSize: 18
    property string toolTipText: ""

    checkable: true
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    spacing: 8
    padding: 0
    leftPadding: 8
    rightPadding: 8
    topPadding: 0
    bottomPadding: 0
    implicitHeight: 28
    implicitWidth: Math.max(
        88,
        leftPadding + indicatorSize + spacing + Math.ceil(textMetrics.width) + rightPadding + 10
    )
    font.family: HostTheme.fonts.body.family
    font.pixelSize: Number(HostTheme.fonts.body.pixelSize)

    TextMetrics {
        id: textMetrics
        text: control.text
        font: control.font
    }

    background: Rectangle {
        radius: 6
        color: control.hovered ? HostTheme.colors.accentSoft : "transparent"
        border.width: control.activeFocus ? 1 : 0
        border.color: control.activeFocus ? HostTheme.colors.accent : "transparent"
    }

    contentItem: Item {
        implicitWidth: control.implicitWidth
        implicitHeight: control.implicitHeight

        Rectangle {
            id: indicatorBox
            width: control.indicatorSize
            height: control.indicatorSize
            x: control.leftPadding
            anchors.verticalCenter: parent.verticalCenter
            radius: 5
            color: control.checked ? HostTheme.colors.accent : HostTheme.colors.surface
            border.color: control.checked ? HostTheme.colors.accent : HostTheme.dividers.default.color
            border.width: 1

            Text {
                anchors.centerIn: parent
                visible: control.checked
                text: "\u2713"
                color: HostTheme.colors.textInverted
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
        }

        Text {
            anchors.left: indicatorBox.right
            anchors.leftMargin: control.spacing
            anchors.right: parent.right
            anchors.rightMargin: control.rightPadding
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            color: control.enabled ? HostTheme.colors.textPrimary : HostTheme.colors.textMuted
            font: control.font
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.NoWrap
            clip: false
        }
    }

    ThemedToolTip {
        target: control
        visible: control.hovered && control.toolTipText.length > 0
        text: control.toolTipText
    }
}
