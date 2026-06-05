//-------------------------------------------------------------------------------------
// MacScrollBar.qml -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
import QtQuick 2.15
import QtQuick.Templates 2.15 as T
import APE.ToolHost 1.0

T.ScrollBar {
    id: root

    readonly property var tokens: HostTheme.lists && HostTheme.lists.scrollbar ? HostTheme.lists.scrollbar : ({})
    readonly property bool horizontalBar: orientation === Qt.Horizontal
    readonly property bool activeState: active || hovered || pressed

    policy: T.ScrollBar.AsNeeded
    interactive: true
    minimumSize: 0.08
    padding: Number(tokens.padding || 2)
    implicitWidth: horizontalBar ? Number(tokens.crossSize || 10) : Number(tokens.size || 10)
    implicitHeight: horizontalBar ? Number(tokens.size || 10) : Number(tokens.crossSize || 10)
    opacity: activeState ? 1.0 : Number(tokens.idleOpacity || 0.48)

    Behavior on opacity {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    background: Item {
        implicitWidth: root.implicitWidth
        implicitHeight: root.implicitHeight

        Rectangle {
            anchors.fill: parent
            anchors.margins: root.activeState ? 1 : 2
            radius: Math.min(width, height) / 2
            color: tokens.track || "transparent"
            opacity: root.activeState ? Number(tokens.trackOpacity || 0.42) : 0.0

            Behavior on opacity {
                NumberAnimation {
                    duration: 160
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    contentItem: Rectangle {
        readonly property real idleSize: Number(tokens.thumbSize || 5)
        readonly property real activeSize: Number(tokens.thumbActiveSize || 6)
        readonly property real crossSize: Number(tokens.thumbCrossSize || 5)

        implicitWidth: root.horizontalBar ? crossSize : (root.activeState ? activeSize : idleSize)
        implicitHeight: root.horizontalBar ? (root.activeState ? activeSize : idleSize) : crossSize
        radius: Math.min(width, height) / 2
        color: root.pressed
            ? (tokens.thumbPressed || "#8C8C8C")
            : (root.hovered ? (tokens.thumbHover || "#9A9A9A") : (tokens.thumb || "#A8A8A8"))
        border.width: root.activeState ? 1 : 0
        border.color: tokens.thumbBorder || "#2AFFFFFF"

        Behavior on color {
            ColorAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }

        Behavior on implicitWidth {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }

        Behavior on implicitHeight {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }
    }
}
