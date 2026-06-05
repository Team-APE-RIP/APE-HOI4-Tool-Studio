import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import APE.ToolHost 1.0

Rectangle {
    id: root
    anchors.fill: parent
    color: "transparent"

    property int entryRowCount: 0
    property int compareRowCount: 0
    property var entryColumns: []
    property var compareColumns: []
    property bool compareMode: false
    property bool loadingActive: false
    property bool hasFiles: false
    property bool hasCurrentFile: false
    property string loadingText: ""
    property string searchText: ""
    property string searchPlaceholder: ""
    property string sortMode: "time"
    property string currentFileName: ""
    property string currentFileDisplayName: ""
    property string compareFileDisplayName: ""
    property string emptyStateText: ""
    property string selectedEntryId: ""
    property string selectedCompareId: ""
    property string pendingClipboardText: ""
    property bool initialFileSelectionRequested: false
    property string initialFileSelectionName: ""
    property string currentPage: "error_log"
    property int stateRevision: 0

    readonly property var surfaces: toolTheme.surfaces
    readonly property var metrics: toolTheme.metrics

    component LogResultPane: Item {
    id: root

    property bool visibleState: true
    property bool compareMode: false
    property bool hasFiles: false
    property string searchTextValue: ""
    property string searchPlaceholderText: ""
    property string compareHeaderText: ""
    property string emptyTitle: ""
    property string emptyDetail: ""
    property string mainSelectedId: ""
    property string compareSelectedId: ""
    property string mainModelId: "log_entries"
    property string compareModelId: "compare_entries"
    property var mainColumns: []
    property var compareColumns: []
    property int mainRowCount: 0
    property int compareRowCount: 0
    property int stateRevision: 0

    signal searchChanged(string text)
    signal rowSelected(string modelKind, var rowData)
    signal rowContextRequested(string modelKind, var rowData, string sectionKey, real x, real y)

    readonly property var metrics: HostTheme.metrics
    readonly property var colors: HostTheme.colors
    readonly property var surfaces: HostTheme.surfaces
    readonly property var inputs: HostTheme.inputs.search
    readonly property var tableHeader: HostTheme.tables.header
    readonly property var tableRow: HostTheme.tables.row
    readonly property var compareTokens: HostTheme.tables.compare
    readonly property var dividerTokens: HostTheme.dividers.default
    readonly property var titleFont: HostTheme.fonts.title
    readonly property var bodyFont: HostTheme.fonts.body
    readonly property real searchPaddingLeft: inputs && inputs.paddingLeft !== undefined ? Number(inputs.paddingLeft) : 12
    readonly property real searchPaddingRight: inputs && inputs.paddingRight !== undefined ? Number(inputs.paddingRight) : 12
    readonly property color inputTextColor: inputs && inputs.text !== undefined ? inputs.text : (colors.textPrimary !== undefined ? colors.textPrimary : "#333333")
    readonly property color inputPlaceholderColor: inputs && inputs.placeholder !== undefined ? inputs.placeholder : (colors.textMuted !== undefined ? colors.textMuted : "#666666")
    readonly property color inputSelectionColor: inputs && inputs.selection !== undefined ? inputs.selection : (colors.selection !== undefined ? colors.selection : "#D8007AFF")
    readonly property color inputSelectedTextColor: inputs && inputs.selectedText !== undefined ? inputs.selectedText : (colors.textInverted !== undefined ? colors.textInverted : "#FFFFFF")
    readonly property real tableHeaderHeight: tableHeader && tableHeader.height !== undefined ? Number(tableHeader.height) : 38
    readonly property color tableHeaderBackground: tableHeader && tableHeader.background !== undefined ? tableHeader.background : (surfaces.header !== undefined ? surfaces.header : (surfaces.table !== undefined ? surfaces.table : "#FFFFFF"))
    readonly property color tableHeaderTextColor: tableHeader && tableHeader.text !== undefined ? tableHeader.text : (colors.textPrimary !== undefined ? colors.textPrimary : "#333333")
    readonly property real tableHeaderPadding: tableHeader && tableHeader.padding !== undefined ? Number(tableHeader.padding) : 8
    readonly property real tableRowHeight: tableRow && tableRow.height !== undefined ? Number(tableRow.height) : 34
    readonly property color tableRowBackgroundColor: tableRow && tableRow.background !== undefined ? tableRow.background : "transparent"
    readonly property color tableRowHoverBackgroundColor: tableRow && tableRow.hoverBackground !== undefined ? tableRow.hoverBackground : (colors.hover !== undefined ? colors.hover : "#EAEAEA")
    readonly property color tableRowSelectedBackgroundColor: tableRow && tableRow.selectedBackground !== undefined ? tableRow.selectedBackground : (colors.selection !== undefined ? colors.selection : "#D8007AFF")
    readonly property color tableRowTextColor: tableRow && tableRow.text !== undefined ? tableRow.text : (colors.textPrimary !== undefined ? colors.textPrimary : "#333333")
    readonly property color tableRowSelectedTextColor: tableRow && tableRow.selectedText !== undefined ? tableRow.selectedText : (colors.textInverted !== undefined ? colors.textInverted : "#FFFFFF")
    readonly property color tableRowPriorityTextColor: tableRow && tableRow.priorityText !== undefined ? tableRow.priorityText : (colors.priority !== undefined ? colors.priority : "#D9534F")
    readonly property real tableRowPadding: tableRow && tableRow.padding !== undefined ? Number(tableRow.padding) : 8
    readonly property real compareHeaderHeight: compareTokens && compareTokens.headerHeight !== undefined ? Number(compareTokens.headerHeight) : 40
    readonly property color compareMissingBackgroundColor: compareTokens && compareTokens.missingBackground !== undefined ? compareTokens.missingBackground : (colors.compareHighlight !== undefined ? colors.compareHighlight : "#FFEBD2")

    visible: visibleState

    function safeString(value) {
        return value === undefined || value === null ? "" : String(value)
    }

    function trText(key, fallback) {
        if (toolBridge.localizationRevision < 0) {
            return fallback
        }
        return toolBridge.text(key, fallback)
    }

    function rowValue(rowData, key, fallbackValue) {
        if (!rowData) {
            return fallbackValue === undefined ? "" : fallbackValue
        }
        if (rowData.values && rowData.values[key] !== undefined) {
            return rowData.values[key]
        }
        if (rowData[key] !== undefined) {
            return rowData[key]
        }
        return fallbackValue === undefined ? "" : fallbackValue
    }

    function rowState(rowData, key, fallbackValue) {
        if (!rowData || !rowData.state || rowData.state[key] === undefined) {
            return fallbackValue === undefined ? false : fallbackValue
        }
        return rowData.state[key]
    }

    function columnWidth(columns, index, totalWidth) {
        if (!columns || index < 0 || index >= columns.length) {
            return 0
        }

        var fixedWidth = 0
        var stretchCount = 0
        for (var i = 0; i < columns.length; ++i) {
            var column = columns[i] || {}
            var widthValue = Number(column.width)
            if (!isNaN(widthValue) && widthValue > 0) {
                fixedWidth += widthValue
            } else {
                stretchCount += 1
            }
        }

        var currentColumn = columns[index] || {}
        var currentWidth = Number(currentColumn.width)
        if (!isNaN(currentWidth) && currentWidth > 0) {
            return currentWidth
        }

        var remainingWidth = Math.max(0, totalWidth - fixedWidth)
        return stretchCount > 0 ? remainingWidth / stretchCount : remainingWidth
    }

    function sectionKeyAt(columns, totalWidth, pointX) {
        var cursor = 0
        for (var i = 0; i < columns.length; ++i) {
            cursor += columnWidth(columns, i, totalWidth)
            if (pointX <= cursor) {
                return safeString(columns[i].key)
            }
        }
        return columns.length > 0 ? safeString(columns[columns.length - 1].key) : ""
    }

    function cellText(rowData, columns, index) {
        if (rowData && rowData.cells && index >= 0 && index < rowData.cells.length) {
            var cell = rowData.cells[index]
            if (cell && cell.value !== undefined) {
                return safeString(cell.value)
            }
        }
        if (!columns || index < 0 || index >= columns.length) {
            return ""
        }
        return safeString(rowValue(rowData, safeString(columns[index].key), ""))
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Number(metrics.searchHeight)
            color: surfaces.searchBar

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: Number(dividerTokens.thickness)
                color: dividerTokens.color
            }

            TextField {
                id: searchField
                anchors.fill: parent
                anchors.leftMargin: searchPaddingLeft
                anchors.rightMargin: searchPaddingRight
                padding: 0
                text: searchTextValue
                placeholderText: root.searchPlaceholderText.length
                    ? root.searchPlaceholderText
                    : root.trText("SearchPlaceholder", "Search entries in the current view...")
                verticalAlignment: TextInput.AlignVCenter
                color: inputTextColor
                placeholderTextColor: inputPlaceholderColor
                selectionColor: inputSelectionColor
                selectedTextColor: inputSelectedTextColor
                background: null
                font.family: bodyFont.family
                font.pixelSize: Number(bodyFont.pixelSize)
                font.weight: Number(bodyFont.weight)
                onTextEdited: root.searchChanged(text)
            }

            Connections {
                target: root

                function onSearchTextValueChanged() {
                    if (!searchField.activeFocus && searchField.text !== root.searchTextValue) {
                        searchField.text = root.searchTextValue
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: compareMode ? compareHeaderHeight : 0
            visible: compareMode
            color: surfaces.table

            Text {
                anchors.centerIn: parent
                text: compareHeaderText
                color: colors.textMuted
                font.family: titleFont.family
                font.pixelSize: Number(titleFont.pixelSize)
                font.weight: Number(titleFont.weight)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: tableHeaderHeight
            visible: !compareMode && hasFiles
            color: tableHeaderBackground

            Row {
                anchors.fill: parent
                spacing: 0

                Repeater {
                    model: mainColumns

                    delegate: Item {
                        width: root.columnWidth(mainColumns, index, parent.width)
                        height: parent.height

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: tableHeaderPadding
                            anchors.rightMargin: tableHeaderPadding
                            verticalAlignment: Text.AlignVCenter
                            text: modelData && modelData.text !== undefined ? String(modelData.text) : ""
                            color: tableHeaderTextColor
                            elide: Text.ElideRight
                            font.family: titleFont.family
                            font.pixelSize: Number(bodyFont.pixelSize)
                            font.weight: Number(titleFont.weight)
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: tableHeaderHeight
            visible: compareMode && hasFiles
            color: tableHeaderBackground

            Row {
                anchors.fill: parent
                spacing: 0

                Repeater {
                    model: compareColumns

                    delegate: Item {
                        width: root.columnWidth(compareColumns, index, parent.width)
                        height: parent.height

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: tableHeaderPadding
                            anchors.rightMargin: tableHeaderPadding
                            verticalAlignment: Text.AlignVCenter
                            text: modelData && modelData.text !== undefined ? String(modelData.text) : ""
                            color: tableHeaderTextColor
                            elide: Text.ElideRight
                            font.family: titleFont.family
                            font.pixelSize: Number(bodyFont.pixelSize)
                            font.weight: Number(titleFont.weight)
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: surfaces.table

            ListView {
                id: mainListView
                anchors.fill: parent
                visible: !compareMode && hasFiles && mainRowCount > 0
                clip: true
                model: mainRowCount
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AlwaysOff
                }

                delegate: Item {
                    id: mainRow
                    width: mainListView.width
                    height: tableRowHeight

                    property int rowIndex: index
                    property int revisionToken: root.stateRevision
                    property var rowData: {
                        revisionToken
                        return toolBridge.row(root.mainModelId, rowIndex)
                    }
                    property bool hovered: mainMouseArea.containsMouse
                    property string rowId: root.safeString(root.rowValue(rowData, "rowId", root.rowValue(rowData, "id", "")))
                    property bool selected: rowId === root.mainSelectedId
                    property color baseTextColor: root.rowState(rowData, "is_high_priority", false)
                        ? tableRowPriorityTextColor
                        : tableRowTextColor

                    Rectangle {
                        anchors.fill: parent
                        color: mainRow.selected
                            ? tableRowSelectedBackgroundColor
                            : (mainRow.hovered ? tableRowHoverBackgroundColor : tableRowBackgroundColor)
                    }

                    Row {
                        anchors.fill: parent
                        spacing: 0

                        Repeater {
                            model: mainColumns

                            delegate: Item {
                                width: root.columnWidth(mainColumns, index, mainRow.width)
                                height: parent.height

                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: tableRowPadding
                                    anchors.rightMargin: tableRowPadding
                                    verticalAlignment: Text.AlignVCenter
                                    text: root.cellText(mainRow.rowData, mainColumns, index)
                                    color: mainRow.selected ? tableRowSelectedTextColor : mainRow.baseTextColor
                                    elide: Text.ElideRight
                                    font.family: bodyFont.family
                                    font.pixelSize: Number(bodyFont.pixelSize)
                                    font.weight: Number(bodyFont.weight)
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: mainMouseArea
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        hoverEnabled: true

                        onClicked: function(mouse) {
                            root.rowSelected("main", mainRow.rowData)
                            if (mouse.button !== Qt.RightButton) {
                                return
                            }
                            const point = mainRow.mapToItem(root, mouse.x, mouse.y)
                            root.rowContextRequested(
                                "main",
                                mainRow.rowData,
                                root.sectionKeyAt(mainColumns, mainRow.width, mouse.x),
                                point.x,
                                point.y
                            )
                        }
                    }
                }
            }

            ListView {
                id: compareListView
                anchors.fill: parent
                visible: compareMode && hasFiles && compareRowCount > 0
                clip: true
                model: compareRowCount
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AlwaysOff
                }

                delegate: Item {
                    id: compareRow
                    width: compareListView.width
                    height: tableRowHeight

                    property int rowIndex: index
                    property int revisionToken: root.stateRevision
                    property var rowData: {
                        revisionToken
                        return toolBridge.row(root.compareModelId, rowIndex)
                    }
                    property bool hovered: compareMouseArea.containsMouse
                    property string rowId: root.safeString(root.rowValue(rowData, "rowId", root.rowValue(rowData, "id", "")))
                    property bool selected: rowId === root.compareSelectedId
                    property color baseTextColor: root.rowState(rowData, "is_high_priority", false)
                        ? tableRowPriorityTextColor
                        : tableRowTextColor

                    Rectangle {
                        anchors.fill: parent
                        color: compareRow.selected
                            ? tableRowSelectedBackgroundColor
                            : (compareRow.hovered ? tableRowHoverBackgroundColor : tableRowBackgroundColor)
                    }

                    Row {
                        anchors.fill: parent
                        spacing: 0

                        Repeater {
                            model: compareColumns

                            delegate: Rectangle {
                                readonly property string key: modelData && modelData.key !== undefined ? String(modelData.key) : ""
                                width: root.columnWidth(compareColumns, index, compareRow.width)
                                height: parent.height
                                color: compareRow.selected
                                    ? "transparent"
                                    : (((key === "left" && root.rowState(compareRow.rowData, "left_missing", false))
                                            || (key === "right" && root.rowState(compareRow.rowData, "right_missing", false)))
                                        ? compareMissingBackgroundColor
                                        : "transparent")

                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: tableRowPadding
                                    anchors.rightMargin: tableRowPadding
                                    verticalAlignment: Text.AlignVCenter
                                    text: root.cellText(compareRow.rowData, compareColumns, index)
                                    color: compareRow.selected ? tableRowSelectedTextColor : compareRow.baseTextColor
                                    elide: Text.ElideRight
                                    font.family: bodyFont.family
                                    font.pixelSize: Number(bodyFont.pixelSize)
                                    font.weight: Number(bodyFont.weight)
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: compareMouseArea
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        hoverEnabled: true

                        onClicked: function(mouse) {
                            root.rowSelected("compare", compareRow.rowData)
                            if (mouse.button !== Qt.RightButton) {
                                return
                            }
                            const point = compareRow.mapToItem(root, mouse.x, mouse.y)
                            root.rowContextRequested(
                                "compare",
                                compareRow.rowData,
                                root.sectionKeyAt(compareColumns, compareRow.width, mouse.x),
                                point.x,
                                point.y
                            )
                        }
                    }
                }
            }

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width * 0.7, 520)
                spacing: 10
                visible: {
                    if (!hasFiles) {
                        return true
                    }
                    if (compareMode) {
                        return compareRowCount === 0
                    }
                    return mainRowCount === 0
                }

                Text {
                    width: parent.width
                    text: emptyTitle
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    color: colors.textMuted
                    font.family: titleFont.family
                    font.pixelSize: Number(titleFont.pixelSize)
                    font.weight: Number(titleFont.weight)
                }

                Text {
                    width: parent.width
                    text: emptyDetail
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    color: colors.textMuted
                    font.family: bodyFont.family
                    font.pixelSize: Number(bodyFont.pixelSize)
                    font.weight: Number(bodyFont.weight)
                }
            }
        }
    }
}

    function safeString(value) {
        return value === undefined || value === null ? "" : String(value)
    }

    function trText(key, fallback) {
        if (toolBridge.localizationRevision < 0) {
            return fallback
        }
        return toolBridge.text(key, fallback)
    }

    function rowValue(rowData, key, fallbackValue) {
        if (!rowData) {
            return fallbackValue === undefined ? "" : fallbackValue
        }
        if (rowData.values && rowData.values[key] !== undefined) {
            return rowData.values[key]
        }
        if (rowData[key] !== undefined) {
            return rowData[key]
        }
        return fallbackValue === undefined ? "" : fallbackValue
    }

    function compareHeaderText() {
        if (!compareMode) {
            return trText("CompareMode", "Compare Mode")
        }
        return trText("CompareMode", "Compare Mode")
            + ": " + currentFileDisplayName + " ↔ " + compareFileDisplayName
    }

    function formatTemplate(templateText, value) {
        return safeString(templateText).replace("%1", safeString(value))
    }

    function formatClipboardEntry(systemTime, gameTime, category, message) {
        return "[" + safeString(systemTime) + "]"
            + "[" + safeString(gameTime) + "]"
            + "[" + safeString(category) + "]: "
            + safeString(message)
    }

    function entryClipboardText(entryLike) {
        if (!entryLike) {
            return ""
        }
        if (entryLike.clipboardText !== undefined && safeString(entryLike.clipboardText).length) {
            return safeString(entryLike.clipboardText)
        }

        const valuesObject = entryLike.values ? entryLike.values : entryLike
        const fullMessage = safeString(valuesObject.fullMessage !== undefined ? valuesObject.fullMessage : valuesObject.message)
        if (!fullMessage.length) {
            return ""
        }

        return formatClipboardEntry(
            valuesObject.timestamp !== undefined ? valuesObject.timestamp : valuesObject.systemTime,
            valuesObject.game_date !== undefined ? valuesObject.game_date : valuesObject.gameTime,
            valuesObject.category,
            fullMessage
        )
    }

    function copyToClipboard(text) {
        const content = safeString(text)
        if (!content.length) {
            return
        }
        clipboardBuffer.text = content
        clipboardBuffer.selectAll()
        clipboardBuffer.copy()
        clipboardBuffer.deselect()
        clipboardBuffer.text = ""
    }

    function dispatchAction(actionType, targetId, argumentsObject) {
        toolBridge.dispatchAction(actionType, targetId || "", argumentsObject || {})
    }

    function applyTopbarAction(actionId) {
        const id = safeString(actionId)
        if (!id.length) {
            return
        }
        if (id === "error_log") {
            dispatchAction("page_select", id, { "pageId": id })
            return
        }
        dispatchAction("button_click", id, { "actionId": id })
    }

    function requestInitialCurrentFileLoad() {
        const fileName = safeString(currentFileName)
        if (hasCurrentFile || !hasFiles || !fileName.length) {
            return
        }
        if (initialFileSelectionRequested && initialFileSelectionName === fileName) {
            return
        }

        // OPTIMIZATION: Delay the file selection to ensure the file list is displayed first.
        // This allows users to see the available files immediately before content is parsed.
        initialFileSelectionRequested = true
        initialFileSelectionName = fileName
        initialFileSelectionTimer.restart()
    }

    function openEntryContext(rowData, localX, localY) {
        selectedEntryId = safeString(rowValue(rowData, "rowId", rowValue(rowData, "id", "")))
        pendingClipboardText = entryClipboardText(rowData)
        if (!pendingClipboardText.length) {
            return
        }
        const point = hostResultList.mapToItem(root, localX, localY)
        entryContextMenu.x = point.x
        entryContextMenu.y = point.y
        entryContextMenu.open()
    }

    function openCompareContext(rowData, sectionKey, localX, localY) {
        selectedCompareId = safeString(rowValue(rowData, "rowId", rowValue(rowData, "id", "")))

        if (sectionKey === "left" || sectionKey === "right") {
            const categoryText = safeString(rowValue(rowData, "category", ""))
            const sectionText = safeString(rowValue(rowData, sectionKey, ""))
            pendingClipboardText = categoryText.length
                ? categoryText + "\n" + sectionText
                : sectionText
        } else {
            pendingClipboardText = ""
        }

        if (!pendingClipboardText.length) {
            return
        }

        const point = hostResultList.mapToItem(root, localX, localY)
        compareContextMenu.x = point.x
        compareContextMenu.y = point.y
        compareContextMenu.open()
    }

    function resultEmptyDetail() {
        if (!hasFiles) {
            return trText("NoFiles", "No log files found")
        }
        return formatTemplate(
            trText("StatsText", "Showing %1 entries"),
            compareMode ? compareRowCount : entryRowCount
        )
    }

    function refreshState() {
        compareMode = !!toolBridge.value("isCompareMode", false)
        entryRowCount = toolBridge.modelRowCount("log_entries")
        compareRowCount = toolBridge.modelRowCount("compare_entries")
        entryColumns = toolBridge.modelColumns("log_entries")
        compareColumns = toolBridge.modelColumns("compare_entries")
        loadingActive = !!toolBridge.value("loadingActive", false)
        hasFiles = !!toolBridge.value("hasFiles", false)
        hasCurrentFile = !!toolBridge.value("hasCurrentFile", false)
        loadingText = safeString(toolBridge.value("loadingText", trText("LoadingLogs", "Loading logs...")))
        searchText = safeString(toolBridge.value("searchText", ""))
        var stateSearchPlaceholder = safeString(toolBridge.value("searchPlaceholder", ""))
        searchPlaceholder = stateSearchPlaceholder.length
            ? stateSearchPlaceholder
            : safeString(trText("SearchPlaceholder", "Search entries in the current view..."))
        sortMode = safeString(toolBridge.value("sortMode", "time"))
        currentFileName = safeString(toolBridge.value("currentFileName", ""))
        currentFileDisplayName = safeString(toolBridge.value("currentFileDisplayName", ""))
        compareFileDisplayName = safeString(toolBridge.value("compareFileDisplayName", ""))
        emptyStateText = safeString(toolBridge.value("emptyStateText", trText("NoEntries", "No entries to display")))
        currentPage = safeString(toolBridge.value("currentPage", "error_log"))
        if (!currentPage.length) {
            currentPage = "error_log"
        }

        if (!compareMode) {
            selectedCompareId = ""
        }

        if (hasCurrentFile || currentFileName !== initialFileSelectionName) {
            initialFileSelectionRequested = false
            initialFileSelectionName = currentFileName
        }

        stateRevision += 1
        requestInitialCurrentFileLoad()
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

    Component.onCompleted: {
        refreshState()
    }

    Timer {
        id: initialFileSelectionTimer
        interval: 150  // OPTIMIZATION: Delay file content loading to allow file list to display first
        repeat: false
        onTriggered: {
            if (root.hasCurrentFile || !root.initialFileSelectionRequested || !root.initialFileSelectionName.length) {
                return
            }
            root.dispatchAction("select_file", root.initialFileSelectionName, {
                "rowId": root.initialFileSelectionName,
                "displayName": root.initialFileSelectionName
            })
        }
    }

    TextArea {
        id: clipboardBuffer
        x: -4096
        y: -4096
        width: 1
        height: 1
        opacity: 0
        focus: false
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
        color: root.surfaces.window
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar {
            id: hostTopBar
            Layout.fillWidth: true
            Layout.preferredHeight: Number(metrics.topBarHeight)
            hostState: ({
                "visible": true,
                "windowDragEnabled": true,
                "leftButtons": [
                    {
                        "actionId": "error_log",
                        "text": root.trText("ErrorLog", "Error Log"),
                        "checked": currentPage === "error_log",
                        "variant": "toolbar",
                        "width": metrics.toolbarButtonWidth
                    }
                ],
                "rightButtons": [
                    {
                        "actionId": "error_log::sort_time",
                        "text": root.trText("SortByTime", "Time"),
                        "shortcut": "T",
                        "checked": sortMode !== "category",
                        "variant": "toolbar",
                        "width": metrics.toolbarButtonWidth
                    },
                    {
                        "actionId": "error_log::sort_category",
                        "text": root.trText("SortByCategory", "Category"),
                        "shortcut": "C",
                        "checked": sortMode === "category",
                        "variant": "toolbar",
                        "width": metrics.toolbarButtonWidth
                    }
                ]
            })
            onActionInvoked: function(actionId) {
                applyTopbarAction(actionId)
            }
        }

        LogResultPane {
            id: hostResultList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visibleState: true
            compareMode: root.compareMode
            hasFiles: root.hasFiles
            searchTextValue: root.searchText
            searchPlaceholderText: root.searchPlaceholder
            compareHeaderText: root.compareHeaderText()
            emptyTitle: root.emptyStateText.length ? root.emptyStateText : root.trText("NoEntries", "No entries to display")
            emptyDetail: root.resultEmptyDetail()
            mainSelectedId: root.selectedEntryId
            compareSelectedId: root.selectedCompareId
            mainModelId: "log_entries"
            compareModelId: "compare_entries"
            mainColumns: root.entryColumns
            compareColumns: root.compareColumns
            mainRowCount: root.entryRowCount
            compareRowCount: root.compareRowCount
            stateRevision: root.stateRevision
            onSearchChanged: function(text) {
                dispatchAction("search_changed", "", { "text": text })
            }
            onRowSelected: function(modelKind, rowData) {
                const rowId = safeString(rowValue(rowData, "rowId", rowValue(rowData, "id", "")))
                if (modelKind === "compare") {
                    selectedCompareId = rowId
                    return
                }
                selectedEntryId = rowId
            }
            onRowContextRequested: function(modelKind, rowData, sectionKey, x, y) {
                if (modelKind === "compare") {
                    openCompareContext(rowData, sectionKey, x, y)
                    return
                }
                openEntryContext(rowData, x, y)
            }
        }
    }

    LoadingOverlay {
        id: hostLoadingOverlay
        anchors.fill: parent
        backdropSource: toolBridge.acrylicSource
        hostState: ({
            "active": loadingActive,
            "text": loadingText.length ? loadingText : root.trText("LoadingLogs", "Loading logs...")
        })
    }

    Menu {
        id: entryContextMenu
        font.family: toolTheme.fonts.body.family
        font.pixelSize: Number(toolTheme.fonts.body.pixelSize)

        MenuItem {
            text: root.trText("CopyFullEntry", "Copy Full Entry")
            onTriggered: root.copyToClipboard(root.pendingClipboardText)
        }
    }

    Menu {
        id: compareContextMenu
        font.family: toolTheme.fonts.body.family
        font.pixelSize: Number(toolTheme.fonts.body.pixelSize)

        MenuItem {
            text: root.trText("CopyFullEntry", "Copy Full Entry")
            onTriggered: root.copyToClipboard(root.pendingClipboardText)
        }
    }
}
