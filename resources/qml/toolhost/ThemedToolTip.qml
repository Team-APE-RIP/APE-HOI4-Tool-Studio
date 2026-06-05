//-------------------------------------------------------------------------------------
// ThemedToolTip.qml -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
import QtQuick 2.15
import QtQuick.Controls 2.15
import APE.ToolHost 1.0

ToolTip {
    id: control

    property Item target: parent
    property int maximumTextWidth: 320

    x: target ? Math.round((target.width - implicitWidth) / 2) : 0
    y: -implicitHeight - 8
    delay: 450
    timeout: 4000
    padding: 8
    leftPadding: 10
    rightPadding: 10
    font.family: HostTheme.fonts.small.family
    font.pixelSize: Math.max(11, Number(HostTheme.fonts.small.pixelSize))
    font.weight: Number(HostTheme.fonts.small.weight)

    readonly property color tooltipBase: solidThemeColor(
        HostTheme.colors.tooltipBackground,
        HostTheme.theme === "dark" ? "#2C2C2E" : "#FFFFFF"
    )
    readonly property color tooltipBorder: HostTheme.colors.tooltipBorder !== undefined
        ? HostTheme.colors.tooltipBorder
        : (HostTheme.theme === "dark" ? "#33FFFFFF" : "#333C3C43")
    readonly property color tooltipText: HostTheme.colors.tooltipText !== undefined
        ? HostTheme.colors.tooltipText
        : HostTheme.colors.textPrimary

    function solidThemeColor(colorValue, fallback) {
        var text = String(colorValue)
        if (text.length === 9 && text.charAt(0) === "#") {
            return "#" + text.substring(3)
        }
        if (text.length === 7 && text.charAt(0) === "#") {
            return text
        }
        return fallback
    }

    TextMetrics {
        id: tooltipMetrics
        text: control.text
        font: control.font
    }

    contentItem: Text {
        width: Math.min(control.maximumTextWidth, Math.max(1, Math.ceil(tooltipMetrics.width)))
        text: control.text
        color: control.tooltipText
        font: control.font
        wrapMode: Text.Wrap
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitWidth: control.contentItem ? control.contentItem.implicitWidth + control.leftPadding + control.rightPadding : 1
        implicitHeight: control.contentItem ? control.contentItem.implicitHeight + control.topPadding + control.bottomPadding : 1
        radius: 7
        color: control.tooltipBase
        border.width: 1
        border.color: control.tooltipBorder

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: HostTheme.theme === "dark" ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(1, 1, 1, 0.34)
                }
                GradientStop {
                    position: 0.42
                    color: HostTheme.theme === "dark" ? Qt.rgba(1, 1, 1, 0.055) : Qt.rgba(1, 1, 1, 0.12)
                }
                GradientStop {
                    position: 1.0
                    color: HostTheme.theme === "dark" ? Qt.rgba(0, 0, 0, 0.10) : Qt.rgba(0, 0, 0, 0.035)
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 1
            anchors.rightMargin: 1
            anchors.topMargin: 1
            height: 1
            color: HostTheme.theme === "dark" ? Qt.rgba(1, 1, 1, 0.26) : Qt.rgba(1, 1, 1, 0.58)
        }
    }
}
