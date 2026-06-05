//-------------------------------------------------------------------------------------
// LoadingOverlay.qml -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
import QtQuick 2.15
import QtQuick.Controls 2.15
import APE.ToolHost 1.0

Item {
    id: root

    property var hostState: ({})
    property string backdropSource: ""
    property real animationProgress: 0.0

    readonly property bool active: hostState && hostState.active !== undefined ? !!hostState.active : false
    readonly property string loadingText: hostState && hostState.text !== undefined ? String(hostState.text) : ""
    readonly property var metrics: HostTheme.metrics
    readonly property var surfaces: HostTheme.surfaces
    readonly property var colors: HostTheme.colors
    readonly property var fonts: HostTheme.fonts
    readonly property real preferredCardWidth: Number(metrics.loadingWidth) > 0 ? Number(metrics.loadingWidth) : 320
    readonly property real progressHeight: Number(metrics.loadingProgressHeight) > 0 ? Number(metrics.loadingProgressHeight) : 4
    readonly property color overlayWash: HostTheme.theme === "dark"
        ? Qt.rgba(0, 0, 0, 0.49)
        : Qt.rgba(28 / 255, 28 / 255, 30 / 255, 0.49)
    readonly property color overlayBackground: solidThemeColor(
        surfaces.window,
        HostTheme.theme === "dark" ? "#2A2A2C" : "#F6F6F8"
    )

    visible: active
    enabled: active
    z: 1000

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

    Rectangle {
        anchors.fill: parent
        visible: active
        color: root.backdropSource.length > 0 ? "transparent" : root.overlayBackground

        Image {
            anchors.fill: parent
            source: root.backdropSource
            visible: root.backdropSource.length > 0
            fillMode: Image.Stretch
            cache: false
            smooth: true
        }

        Rectangle {
            anchors.fill: parent
            visible: root.backdropSource.length > 0
            color: root.overlayWash
        }

        Rectangle {
            anchors.fill: parent
            visible: root.backdropSource.length > 0
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: HostTheme.theme === "dark" ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(1, 1, 1, 0.11)
                }
                GradientStop {
                    position: 1.0
                    color: Qt.rgba(1, 1, 1, 0)
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
        }

        Rectangle {
            width: Math.min(root.preferredCardWidth, Math.max(220, parent.width - 48))
            height: loadingContent.implicitHeight + 36
            anchors.centerIn: parent
            radius: 8
            color: colors.loadingCard
            border.width: 1
            border.color: colors.loadingBorder

            Column {
                id: loadingContent
                anchors.centerIn: parent
                width: parent.width - 36
                spacing: 10

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: loadingText
                    color: colors.loadingText
                    font.family: fonts.overlay.family
                    font.pixelSize: Number(fonts.overlay.pixelSize)
                    font.weight: Number(fonts.overlay.weight)
                }

                Rectangle {
                    width: parent.width
                    height: root.progressHeight
                    radius: height / 2
                    color: colors.loadingTrack
                    clip: true

                    Rectangle {
                        width: parent.width * 0.42
                        height: parent.height
                        radius: height / 2
                        color: colors.accent
                        x: root.animationProgress * (parent.width - width)
                    }
                }
            }
        }
    }

    NumberAnimation {
        target: root
        property: "animationProgress"
        from: 0
        to: 1
        duration: 900
        loops: Animation.Infinite
        running: active
    }
}
