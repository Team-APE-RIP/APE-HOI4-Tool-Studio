//-------------------------------------------------------------------------------------
// FileManagerTool.qml -- Part of APE HOI4 Tool Studio
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

Rectangle {
    id: root
    anchors.fill: parent
    color: "transparent"

    property var treeRows: []
    property string filterText: ""
    property string selectedRelativePath: ""
    property string lastError: ""
    property bool hasSelection: false
    property int totalCount: 0
    property int filteredCount: 0
    property int visibleCount: 0
    property bool loadingActive: true
    property string loadingText: ""

    readonly property var fontTokens: toolTheme.fonts
    readonly property bool darkTheme: toolTheme.theme === "dark"
    readonly property color legacyBackground: toolTheme.surfaces.window
    readonly property color legacyText: toolTheme.colors.textPrimary
    readonly property color legacyBorder: toolTheme.colors.border
    readonly property color legacyInputBackground: toolTheme.colors.surface
    readonly property color legacyAlternateRow: toolTheme.colors.surfaceAlt
    readonly property color legacyHeaderBackground: toolTheme.colors.header
    readonly property color legacyHoverBackground: toolTheme.colors.hover
    readonly property color legacyPathText: toolTheme.colors.textMuted
    readonly property color legacySelection: toolTheme.colors.selection
    readonly property int treeMargin: 10
    readonly property int treeSpacing: 10
    readonly property int nameColumnWidth: 300
    readonly property int sourceColumnWidth: 100
    readonly property int rowHeight: 28

    function safeString(value) {
        return value === undefined || value === null ? "" : String(value)
    }

    function trText(key, fallback) {
        if (toolBridge.localizationRevision < 0) {
            return fallback
        }
        return toolBridge.text(key, fallback)
    }

    function refreshState() {
        var nextTreeRows = toolBridge.value("treeRows", [])
        treeRows = nextTreeRows
        syncTreeRows(nextTreeRows)
        filterText = safeString(toolBridge.value("filterText", ""))
        selectedRelativePath = safeString(toolBridge.value("selectedRelativePath", ""))
        lastError = safeString(toolBridge.value("lastError", ""))
        hasSelection = !!toolBridge.value("hasSelection", false)
        totalCount = Number(toolBridge.value("totalCount", 0))
        filteredCount = Number(toolBridge.value("filteredCount", 0))
        visibleCount = Number(toolBridge.value("visibleCount", 0))
        loadingActive = !!toolBridge.value("loadingActive", false)
        loadingText = safeString(toolBridge.value("loadingText", trText("LoadingFiles", "Loading files...")))
    }

    function dispatchAction(actionType, targetId, argumentsObject) {
        toolBridge.dispatchAction(actionType, targetId || "", argumentsObject || {})
    }

    function loadingDisplayText() {
        var text = safeString(loadingText)
        if (!text.length || text === "Starting worker process...") {
            return trText("LoadingFiles", "Loading files...")
        }
        return text
    }

    function rowText(row, key) {
        if (row === undefined || row === null || row[key] === undefined || row[key] === null) {
            return ""
        }
        return String(row[key])
    }

    function rowIdentity(row, rowIndex) {
        var explicitId = rowText(row, "rowId")
        if (explicitId.length > 0) {
            return explicitId
        }

        var path = rowText(row, "relativePath")
        if (path.length > 0) {
            return path
        }

        return String(rowIndex) + "|" + rowText(row, "displayName") + "|" + rowText(row, "source")
    }

    function normalizedTreeRow(row, rowIndex) {
        return {
            "rowId": rowIdentity(row, rowIndex),
            "relativePath": rowText(row, "relativePath"),
            "displayName": rowText(row, "displayName"),
            "source": rowText(row, "source"),
            "isDirectory": !!(row && row.isDirectory),
            "expanded": !!(row && row.expanded),
            "selected": !!(row && row.selected),
            "depth": Number(row && row.depth ? row.depth : 0)
        }
    }

    function syncTreeRows(nextRows) {
        var normalizedRows = []
        var nextCount = nextRows && nextRows.length !== undefined ? nextRows.length : 0
        for (var nextIndex = 0; nextIndex < nextCount; ++nextIndex) {
            normalizedRows.push(normalizedTreeRow(nextRows[nextIndex], nextIndex))
        }

        var oldCount = treeModel.count
        var start = 0
        while (start < oldCount
               && start < normalizedRows.length
               && treeModel.get(start).rowId === normalizedRows[start].rowId) {
            ++start
        }

        var oldEnd = oldCount - 1
        var newEnd = normalizedRows.length - 1
        while (oldEnd >= start
               && newEnd >= start
               && treeModel.get(oldEnd).rowId === normalizedRows[newEnd].rowId) {
            --oldEnd
            --newEnd
        }

        for (var removeIndex = oldEnd; removeIndex >= start; --removeIndex) {
            treeModel.remove(removeIndex)
        }

        for (var insertIndex = start; insertIndex <= newEnd; ++insertIndex) {
            treeModel.insert(insertIndex, normalizedRows[insertIndex])
        }

        if (treeModel.count !== normalizedRows.length) {
            treeModel.clear()
            for (var resetIndex = 0; resetIndex < normalizedRows.length; ++resetIndex) {
                treeModel.append(normalizedRows[resetIndex])
            }
            return
        }

        for (var updateIndex = 0; updateIndex < normalizedRows.length; ++updateIndex) {
            if (treeModel.get(updateIndex).rowId === normalizedRows[updateIndex].rowId) {
                treeModel.set(updateIndex, normalizedRows[updateIndex])
            }
        }
    }

    Connections {
        target: toolBridge

        function onStateChanged() {
            root.refreshState()
        }

        function onLocalizedStringsChanged() {
            root.refreshState()
        }
    }

    Component.onCompleted: refreshState()

    ListModel {
        id: treeModel
    }

    Image {
        anchors.fill: parent
        source: toolBridge.acrylicSource
        fillMode: Image.Stretch
        cache: false
        smooth: true
    }

    Rectangle {
        anchors.fill: parent
        color: root.legacyBackground
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.treeMargin
        spacing: root.treeSpacing

        TextField {
            id: searchField
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            leftPadding: 12
            rightPadding: 12
            topPadding: 0
            bottomPadding: 0
            text: root.filterText
            placeholderText: root.trText("SearchPlaceholder", "Search files...")
            selectByMouse: true
            verticalAlignment: TextInput.AlignVCenter
            font.family: root.fontTokens.body.family
            font.pixelSize: 13
            color: root.legacyText
            placeholderTextColor: root.legacyPathText
            background: Rectangle {
                radius: 8
                color: root.legacyInputBackground
                border.width: 1
                border.color: searchField.activeFocus ? root.legacySelection : root.legacyBorder
            }
            onTextEdited: root.dispatchAction("search_changed", "", {"text": text})
        }

        Rectangle {
            id: treeFrame
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: root.legacyInputBackground
            border.width: 1
            border.color: root.legacyBorder
            clip: true

            Rectangle {
                id: headerRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 1
                anchors.rightMargin: 1
                anchors.topMargin: 1
                height: 34
                color: root.legacyHeaderBackground

                Text {
                    x: 6
                    width: root.nameColumnWidth - 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.trText("FileName", "Name")
                    color: root.legacyText
                    font.family: root.fontTokens.bodyStrong.family
                    font.pixelSize: 13
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }

                Text {
                    x: root.nameColumnWidth + 6
                    width: root.sourceColumnWidth - 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.trText("Source", "Source")
                    color: root.legacyText
                    font.family: root.fontTokens.bodyStrong.family
                    font.pixelSize: 13
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }

                Text {
                    x: root.nameColumnWidth + root.sourceColumnWidth + 6
                    width: parent.width - x - 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.trText("RelativePath", "Relative Path")
                    color: root.legacyText
                    font.family: root.fontTokens.bodyStrong.family
                    font.pixelSize: 13
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: root.legacyBorder
                }
            }

            ListView {
                id: treeView
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: headerRow.bottom
                anchors.bottom: parent.bottom
                anchors.leftMargin: 1
                anchors.rightMargin: 1
                anchors.bottomMargin: 1
                clip: true
                model: treeModel
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: root.rowHeight * 18
                reuseItems: true
                highlightMoveDuration: 180
                highlightResizeDuration: 180
                ScrollBar.vertical: MacScrollBar {}
                add: Transition {
                    NumberAnimation { properties: "expansionProgress"; from: 0; to: 1; duration: 220; easing.type: Easing.OutCubic }
                    NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: 190; easing.type: Easing.OutCubic }
                    NumberAnimation { properties: "scale"; from: 0.985; to: 1.0; duration: 210; easing.type: Easing.OutCubic }
                    NumberAnimation { properties: "y"; duration: 210; easing.type: Easing.OutCubic }
                }
                addDisplaced: Transition {
                    NumberAnimation { properties: "y"; duration: 220; easing.type: Easing.OutCubic }
                }
                remove: Transition {
                    NumberAnimation { properties: "expansionProgress"; from: 1; to: 0; duration: 150; easing.type: Easing.InCubic }
                    NumberAnimation { properties: "opacity"; from: 1; to: 0; duration: 130; easing.type: Easing.InCubic }
                    NumberAnimation { properties: "scale"; from: 1.0; to: 0.985; duration: 130; easing.type: Easing.InCubic }
                }
                removeDisplaced: Transition {
                    NumberAnimation { properties: "y"; duration: 230; easing.type: Easing.OutCubic }
                }
                displaced: Transition {
                    NumberAnimation { properties: "y"; duration: 210; easing.type: Easing.OutCubic }
                }

                delegate: Rectangle {
                    id: treeRow
                    width: treeView.width
                    height: Math.max(0, root.rowHeight * expansionProgress)
                    opacity: 1
                    scale: 1.0
                    transformOrigin: Item.Top
                    clip: true
                    layer.enabled: scale !== 1.0 || opacity < 1.0
                    layer.smooth: true
                    color: rowSelected
                        ? root.legacySelection
                        : (rowMouse.containsMouse ? root.legacyHoverBackground : (index % 2 === 0 ? root.legacyInputBackground : root.legacyAlternateRow))

                    property bool rowSelected: !!selected
                    property bool rowDirectory: !!isDirectory
                    property bool rowExpanded: !!expanded
                    property int rowDepth: Number(depth || 0)
                    property string rowPath: root.safeString(relativePath)
                    property string rowName: root.safeString(displayName)
                    property string rowSource: root.safeString(source)
                    property real expansionProgress: 1.0
                    property int indentX: 14 + rowDepth * 20

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton
                        onClicked: root.dispatchAction("select_node", treeRow.rowPath, {"relativePath": treeRow.rowPath})
                        onDoubleClicked: {
                            if (treeRow.rowDirectory) {
                                root.dispatchAction("toggle_directory", treeRow.rowPath, {"relativePath": treeRow.rowPath})
                            }
                        }
                    }

                    Text {
                        x: treeRow.indentX
                        width: 12
                        anchors.verticalCenter: parent.verticalCenter
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: treeRow.rowDirectory ? "\u25B8" : ""
                        color: treeRow.rowSelected ? "#FFFFFF" : root.legacyPathText
                        font.pixelSize: 10
                        rotation: treeRow.rowDirectory && treeRow.rowExpanded ? 90 : 0
                        transformOrigin: Item.Center

                        Behavior on rotation {
                            NumberAnimation { duration: 130; easing.type: Easing.OutCubic }
                        }

                        MouseArea {
                            anchors.fill: parent
                            enabled: treeRow.rowDirectory
                            acceptedButtons: Qt.LeftButton
                            onClicked: root.dispatchAction("toggle_directory", treeRow.rowPath, {"relativePath": treeRow.rowPath})
                        }
                    }

                    Canvas {
                        id: rowIcon
                        x: treeRow.indentX + 24
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        property bool folderIcon: treeRow.rowDirectory
                        property string strokeColor: treeRow.rowSelected ? "#FFFFFF" : (root.darkTheme ? "#FFFFFF" : "#1D1D1F")
                        onFolderIconChanged: requestPaint()
                        onStrokeColorChanged: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.lineWidth = 1
                            ctx.strokeStyle = strokeColor
                            ctx.fillStyle = folderIcon
                                ? (root.darkTheme ? "rgba(255,255,255,0.20)" : "rgba(0,0,0,0.08)")
                                : "transparent"

                            ctx.beginPath()
                            if (folderIcon) {
                                ctx.moveTo(1, 3)
                                ctx.lineTo(6, 3)
                                ctx.lineTo(8, 5)
                                ctx.lineTo(15, 5)
                                ctx.lineTo(15, 13)
                                ctx.lineTo(1, 13)
                                ctx.closePath()
                                ctx.fill()
                                ctx.stroke()
                            } else {
                                ctx.moveTo(3, 1)
                                ctx.lineTo(10, 1)
                                ctx.lineTo(13, 4)
                                ctx.lineTo(13, 15)
                                ctx.lineTo(3, 15)
                                ctx.closePath()
                                ctx.moveTo(10, 1)
                                ctx.lineTo(10, 4)
                                ctx.lineTo(13, 4)
                                ctx.stroke()
                            }
                        }
                        Component.onCompleted: requestPaint()
                    }

                    Text {
                        x: treeRow.indentX + 46
                        width: Math.max(20, root.nameColumnWidth - x - 6)
                        anchors.verticalCenter: parent.verticalCenter
                        text: treeRow.rowName
                        color: treeRow.rowSelected ? "#FFFFFF" : root.legacyText
                        font.family: root.fontTokens.body.family
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }

                    Text {
                        x: root.nameColumnWidth + 6
                        width: root.sourceColumnWidth - 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: treeRow.rowSource
                        color: treeRow.rowSelected ? "#FFFFFF" : root.legacyText
                        font.family: root.fontTokens.body.family
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }

                    Text {
                        x: root.nameColumnWidth + root.sourceColumnWidth + 6
                        width: parent.width - x - 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: treeRow.rowPath
                        color: treeRow.rowSelected ? "#FFFFFF" : root.legacyText
                        font.family: root.fontTokens.body.family
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(20, implicitHeight)
            text: root.hasSelection
                ? root.selectedRelativePath
                : root.trText("SelectFileHint", "Select a file to see details")
            color: root.lastError.length ? "#EF4444" : root.legacyPathText
            font.family: root.fontTokens.small.family
            font.pixelSize: 12
            font.italic: true
            wrapMode: Text.WrapAnywhere
            elide: Text.ElideRight
        }
    }

    LoadingOverlay {
        anchors.fill: parent
        backdropSource: toolBridge.acrylicSource
        hostState: ({
            "active": root.loadingActive,
            "text": root.loadingDisplayText()
        })
    }
}
