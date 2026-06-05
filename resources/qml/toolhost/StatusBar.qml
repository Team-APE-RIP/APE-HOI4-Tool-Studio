//-------------------------------------------------------------------------------------
// StatusBar.qml -- Part of APE HOI4 Tool Studio
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

    readonly property bool visibleState: hostState && hostState.visible !== undefined ? !!hostState.visible : false
    readonly property string leftText: hostState && hostState.leftText !== undefined ? String(hostState.leftText) : ""
    readonly property var rightStats: hostState && hostState.rightStats ? hostState.rightStats : []
    readonly property var metrics: HostTheme.metrics
    readonly property var surfaces: HostTheme.surfaces
    readonly property var dividerTokens: HostTheme.dividers.default
    readonly property var bodyFont: HostTheme.fonts.body
    readonly property var strongFont: HostTheme.fonts.bodyStrong

    visible: visibleState
    height: visibleState ? Number(metrics.statusBarHeight) : 0

    Rectangle {
        anchors.fill: parent
        color: surfaces.topBar

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Number(dividerTokens.thickness)
            color: dividerTokens.color
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: leftText
                color: HostTheme.colors.textMuted
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                font.family: bodyFont.family
                font.pixelSize: Number(bodyFont.pixelSize)
                font.weight: Number(bodyFont.weight)
            }

            Repeater {
                model: rightStats

                delegate: Row {
                    spacing: 4

                    Text {
                        text: modelData && modelData.label !== undefined ? String(modelData.label) : ""
                        color: HostTheme.colors.textMuted
                        font.family: bodyFont.family
                        font.pixelSize: Number(bodyFont.pixelSize)
                        font.weight: Number(bodyFont.weight)
                    }

                    Text {
                        text: modelData && modelData.value !== undefined ? String(modelData.value) : ""
                        color: HostTheme.colors.textPrimary
                        font.family: strongFont.family
                        font.pixelSize: Number(bodyFont.pixelSize)
                        font.weight: Number(strongFont.weight)
                    }
                }
            }
        }
    }
}
