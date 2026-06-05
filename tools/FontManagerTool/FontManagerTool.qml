import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs 6.2
import APE.ToolHost 1.0

Rectangle {
    id: root
    anchors.fill: parent
    color: "transparent"

    property string mode: "manage"
    property string selectedImportId: ""
    property string previewText: ""
    property string previewBase64: ""
    property var settings: ({})
    property bool pendingOverwrite: false
    property var pendingOverwriteFiles: []
    property string lastError: ""
    property string statusText: ""
    property bool refreshing: false
    property bool loadingActive: true
    property string loadingText: ""
    property int previewRevision: 0

    readonly property var colors: toolTheme.colors
    readonly property var surfaces: toolTheme.surfaces
    readonly property var metrics: toolTheme.metrics
    readonly property var fonts: toolTheme.fonts

    function safeString(value) {
        return value === undefined || value === null ? "" : String(value)
    }

    function trText(key, fallback) {
        if (toolBridge.localizationRevision < 0) {
            return fallback
        }
        return toolBridge.text(key, fallback)
    }

    function localPathFromUrl(urlValue) {
        var text = safeString(urlValue)
        if (text.indexOf("file:///") === 0) {
            text = text.substring(8)
            if (Qt.platform.os === "windows" && text.length > 2 && text.charAt(1) === ":") {
                return decodeURIComponent(text)
            }
            return "/" + decodeURIComponent(text)
        }
        if (text.indexOf("file://") === 0) {
            return decodeURIComponent(text.substring(7))
        }
        return decodeURIComponent(text)
    }

    function dispatchAction(actionType, targetId, args) {
        toolBridge.dispatchAction(actionType, targetId || "", args || {})
    }

    function imageSource(base64) {
        var value = safeString(base64)
        return value.length ? toolBridge.imageSource(value, "fontPreview" + previewRevision) : ""
    }

    function loadingDisplayText() {
        var text = safeString(loadingText)
        if (!text.length || text === "Starting worker process...") {
            return trText("LoadingFonts", "Loading fonts...")
        }
        return text
    }

    function refreshState() {
        refreshing = true
        mode = safeString(toolBridge.value("mode", "manage"))
        selectedImportId = safeString(toolBridge.value("selectedImportId", ""))
        previewText = safeString(toolBridge.value("previewText", ""))
        var nextPreviewBase64 = safeString(toolBridge.value("previewBase64", ""))
        if (nextPreviewBase64 !== previewBase64) {
            previewBase64 = nextPreviewBase64
            previewRevision += 1
        }
        settings = toolBridge.value("settings", {})
        pendingOverwrite = !!toolBridge.value("pendingOverwrite", false)
        pendingOverwriteFiles = toolBridge.value("pendingOverwriteFiles", [])
        lastError = safeString(toolBridge.value("lastError", ""))
        statusText = safeString(toolBridge.value("statusText", ""))
        loadingActive = !!toolBridge.value("loadingActive", false)
        loadingText = safeString(toolBridge.value("loadingText", trText("LoadingFonts", "Loading fonts...")))
        refreshing = false
        if (pendingOverwrite && !overwriteDialog.opened) {
            overwriteDialog.open()
        }
    }

    function setting(key, fallback) {
        return settings && settings[key] !== undefined ? settings[key] : fallback
    }

    function numberSetting(key, fallback) {
        return Number(setting(key, fallback))
    }

    function boundedNumberSetting(key, fallback, minimum, maximum) {
        return clampNumber(setting(key, fallback), fallback, minimum, maximum)
    }

    function boolSetting(key, fallback) {
        return !!setting(key, fallback)
    }

    function clampNumber(value, fallback, minimum, maximum) {
        var numericValue = Number(value)
        if (!isFinite(numericValue)) {
            numericValue = Number(fallback)
        }
        numericValue = Math.round(numericValue)
        return Math.max(minimum, Math.min(maximum, numericValue))
    }

    function updateSetting(key, value) {
        var next = {}
        for (var prop in settings) {
            next[prop] = settings[prop]
        }
        next[key] = value
        settings = next
        dispatchAction("update_settings", "", next)
    }

    function textColorRows() {
        var rows = setting("textColors", [])
        return rows && rows.length !== undefined ? rows : []
    }

    function textColorComponent(row, key, fallback) {
        return clampNumber(row && row[key] !== undefined ? row[key] : fallback, fallback, 0, 255)
    }

    function textColorQColor(row) {
        return Qt.rgba(
            textColorComponent(row, "red", 0) / 255,
            textColorComponent(row, "green", 255) / 255,
            textColorComponent(row, "blue", 0) / 255,
            1
        )
    }

    function normalizedTextColorCode(value) {
        var text = safeString(value).trim()
        return text.length > 0 ? text.charAt(0) : "g"
    }

    function cloneTextColorRows() {
        var source = textColorRows()
        var rows = []
        for (var i = 0; i < source.length; ++i) {
            var row = source[i] || {}
            rows.push({
                "code": normalizedTextColorCode(row.code),
                "red": textColorComponent(row, "red", 0),
                "green": textColorComponent(row, "green", 255),
                "blue": textColorComponent(row, "blue", 0)
            })
        }
        return rows
    }

    function updateTextColorRows(rows) {
        updateSetting("textColors", rows)
    }

    function updateTextColorValue(index, key, value) {
        var rows = cloneTextColorRows()
        if (index < 0 || index >= rows.length) {
            return
        }
        if (key === "code") {
            rows[index].code = normalizedTextColorCode(value)
        } else {
            rows[index][key] = clampNumber(value, key === "green" ? 255 : 0, 0, 255)
        }
        updateTextColorRows(rows)
    }

    function addTextColor() {
        var rows = cloneTextColorRows()
        rows.push({ "code": "g", "red": 0, "green": 255, "blue": 0 })
        updateTextColorRows(rows)
    }

    function removeTextColor(index) {
        var rows = cloneTextColorRows()
        if (index < 0 || index >= rows.length) {
            return
        }
        rows.splice(index, 1)
        updateTextColorRows(rows)
    }

    function responsiveFlowColumnCount(totalWidth, preferredWidth, itemCount, spacing) {
        var widthValue = Math.max(1, Number(totalWidth))
        var preferred = Math.max(1, Number(preferredWidth))
        var itemTotal = Math.max(1, Number(itemCount))
        var gap = Math.max(0, Number(spacing || 0))
        var count = Math.max(1, Math.floor((widthValue + gap) / (preferred + gap)))
        return Math.min(itemTotal, count)
    }

    function responsiveFlowItemWidth(totalWidth, preferredWidth, itemCount, spacing) {
        var widthValue = Math.max(1, Number(totalWidth))
        var gap = Math.max(0, Number(spacing || 0))
        var columns = responsiveFlowColumnCount(widthValue, preferredWidth, itemCount, gap)
        return Math.max(1, Math.floor((widthValue - gap * (columns - 1)) / columns))
    }

    function fontToggleColumnCount(totalWidth, spacing) {
        var widthValue = Math.max(1, Number(totalWidth))
        var gap = Math.max(0, Number(spacing || 0))
        var compactWidth = 104
        if (widthValue >= compactWidth * 6 + gap * 5) {
            return 6
        }
        if (widthValue >= compactWidth * 3 + gap * 2) {
            return 3
        }
        return 2
    }

    function fontToggleItemWidth(totalWidth, spacing) {
        var widthValue = Math.max(1, Number(totalWidth))
        var gap = Math.max(0, Number(spacing || 0))
        var columns = fontToggleColumnCount(widthValue, gap)
        return Math.max(1, Math.floor((widthValue - gap * (columns - 1)) / columns))
    }

    function updateNumberSetting(key, value, fallback, minimum, maximum) {
        updateSetting(key, clampNumber(value, fallback, minimum, maximum))
    }

    function exportFont() {
        dispatchAction("export_font", selectedImportId, { "settings": settings })
    }

    function importPathsFromDrop(drop) {
        var paths = []
        if (drop.urls) {
            for (var i = 0; i < drop.urls.length; ++i) {
                var path = root.localPathFromUrl(drop.urls[i])
                var lowered = path.toLowerCase()
                if (lowered.endsWith(".ttf") || lowered.endsWith(".otf")) {
                    paths.push(path)
                }
            }
        }
        if (paths.length) {
            root.dispatchAction("import_ttf", "", { "paths": paths })
        }
    }

    Timer {
        id: previewUpdateTimer
        interval: 180
        repeat: false
        onTriggered: root.dispatchAction("set_preview_text", "", { "text": previewEditor.text })
    }

    function applyTopbarAction(actionId) {
        if (actionId === "set_mode_manage" || actionId === "set_mode_new" || actionId === "refresh") {
            dispatchAction(actionId)
            return
        }
        if (actionId === "import_ttf") {
            importDialog.open()
            return
        }
        if (actionId === "export_font") {
            exportFont()
            return
        }
    }

    Connections {
        target: toolBridge
        function onValuesChanged() { root.refreshState() }
        function onLocalizedStringsChanged() { root.refreshState() }
    }

    Component.onCompleted: refreshState()

    FileDialog {
        id: importDialog
        title: root.trText("ImportTtf", "Import")
        fileMode: FileDialog.OpenFiles
        nameFilters: [ "TrueType (*.ttf *.otf)" ]
        onAccepted: {
            var paths = []
            for (var i = 0; i < selectedFiles.length; ++i) {
                paths.push(root.localPathFromUrl(selectedFiles[i]))
            }
            if (paths.length) {
                root.dispatchAction("import_ttf", "", { "paths": paths })
            }
        }
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
            Layout.fillWidth: true
            Layout.preferredHeight: Number(metrics.topBarHeight)
            hostState: ({
                "visible": true,
                "windowDragEnabled": true,
                "leftButtons": [
                    { "actionId": "set_mode_manage", "text": root.trText("TabManage", "Manage"), "checked": root.mode !== "new", "variant": "toolbar", "width": 92 },
                    { "actionId": "set_mode_new", "text": root.trText("TabNew", "Create"), "checked": root.mode === "new", "variant": "toolbar", "width": 92 }
                ],
                "rightButtons": root.mode === "new"
                    ? [
                        { "actionId": "import_ttf", "text": root.trText("ImportTtf", "Import"), "shortcut": "Ctrl+O", "width": 88 },
                        { "actionId": "export_font", "text": root.trText("Export", "Export"), "shortcut": "Ctrl+S", "enabled": root.selectedImportId.length > 0, "width": 88 }
                    ]
                    : []
            })
            onActionInvoked: function(actionId) { root.applyTopbarAction(actionId) }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            DropArea {
                anchors.fill: parent
                enabled: root.mode === "new"
                onDropped: function(drop) {
                    root.importPathsFromDrop(drop)
                }
            }

            Rectangle {
                anchors.fill: parent
                color: colors.surfaceAlt
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        visible: root.lastError.length > 0
                        Layout.fillWidth: true
                        text: root.lastError
                        color: colors.danger
                        wrapMode: Text.WordWrap
                        font.family: fonts.body.family
                        font.pixelSize: Number(fonts.body.pixelSize)
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        Item {
                            id: contentBody
                            anchors.fill: parent

                            ColumnLayout {
                                id: contentColumn
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                spacing: 14

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 1
                                    Layout.preferredHeight: 220
                                    color: colors.surface
                                    border.color: toolTheme.dividers.default.color
                                    border.width: 1
                                    radius: 6

                                    Image {
                                        id: previewImage
                                        anchors.centerIn: parent
                                        source: root.imageSource(root.previewBase64)
                                        visible: root.previewBase64.length > 0 && status === Image.Ready
                                        cache: false
                                        asynchronous: false
                                        fillMode: Image.PreserveAspectFit
                                        smooth: false
                                        width: Math.min(Math.max(parent.width - 28, 1), implicitWidth)
                                        height: Math.min(Math.max(parent.height - 28, 1), implicitHeight)
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        visible: !previewImage.visible
                                        text: root.trText("NoPreview", "No Preview")
                                        color: colors.textMuted
                                        font.family: fonts.title.family
                                        font.pixelSize: Number(fonts.title.pixelSize)
                                    }
                                }

                                Label {
                                    text: root.trText("PreviewText", "Preview Text")
                                    color: colors.textMuted
                                    font.family: fonts.small.family
                                    font.pixelSize: Number(fonts.small.pixelSize)
                                }

                                TextArea {
                                    id: previewEditor
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 1
                                    Layout.preferredHeight: 92
                                    text: root.previewText
                                    selectByMouse: true
                                    wrapMode: TextEdit.Wrap
                                    color: colors.textPrimary
                                    selectionColor: colors.selection
                                    selectedTextColor: colors.textInverted
                                    font.family: fonts.body.family
                                    font.pixelSize: Number(fonts.body.pixelSize)
                                    background: Rectangle {
                                        radius: 6
                                        color: colors.surface
                                        border.color: toolTheme.dividers.default.color
                                        border.width: 1
                                    }
                                    onTextChanged: {
                                        if (!root.refreshing && activeFocus) {
                                            previewUpdateTimer.restart()
                                        }
                                    }
                                    onActiveFocusChanged: {
                                        if (!activeFocus && !root.refreshing) {
                                            root.dispatchAction("set_preview_text", "", { "text": text })
                                        }
                                    }
                                }

                                RowLayout {
                                    id: fontColorEditorRow
                                    visible: root.mode === "new"
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 1
                                    Layout.preferredHeight: 38
                                    spacing: 8

                                    Label {
                                        Layout.preferredWidth: 82
                                        Layout.fillHeight: true
                                        text: root.trText("FontColors", "Colors")
                                        color: colors.textMuted
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
                                        font.family: fonts.small.family
                                        font.pixelSize: Number(fonts.small.pixelSize)
                                    }

                                    Flickable {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 1
                                        Layout.preferredHeight: 38
                                        clip: true
                                        contentWidth: colorCardsRow.implicitWidth
                                        contentHeight: height
                                        boundsBehavior: Flickable.StopAtBounds
                                        interactive: contentWidth > width

                                        Row {
                                            id: colorCardsRow
                                            height: parent.height
                                            spacing: 8

                                            Repeater {
                                                id: colorCardRepeater
                                                model: root.textColorRows()

                                                Rectangle {
                                                    width: 306
                                                    height: 34
                                                    radius: 6
                                                    color: colors.surface
                                                    border.color: toolTheme.dividers.default.color
                                                    border.width: 1

                                                    RowLayout {
                                                        anchors.fill: parent
                                                        anchors.margins: 4
                                                        spacing: 5

                                                        FormTextField {
                                                            Layout.preferredWidth: 34
                                                            Layout.preferredHeight: 26
                                                            text: root.normalizedTextColorCode(modelData.code)
                                                            color: root.textColorQColor(modelData)
                                                            horizontalAlignment: TextInput.AlignHCenter
                                                            verticalAlignment: TextInput.AlignVCenter
                                                            selectByMouse: true
                                                            onEditingFinished: root.updateTextColorValue(index, "code", text)
                                                        }

                                                        FormSpinBox {
                                                            Layout.preferredWidth: 72
                                                            Layout.preferredHeight: 26
                                                            indicatorWidth: 18
                                                            textHorizontalPadding: 19
                                                            from: 0
                                                            to: 255
                                                            value: root.textColorComponent(modelData, "red", 0)
                                                            onValueModified: root.updateTextColorValue(index, "red", value)
                                                        }

                                                        FormSpinBox {
                                                            Layout.preferredWidth: 72
                                                            Layout.preferredHeight: 26
                                                            indicatorWidth: 18
                                                            textHorizontalPadding: 19
                                                            from: 0
                                                            to: 255
                                                            value: root.textColorComponent(modelData, "green", 255)
                                                            onValueModified: root.updateTextColorValue(index, "green", value)
                                                        }

                                                        FormSpinBox {
                                                            Layout.preferredWidth: 72
                                                            Layout.preferredHeight: 26
                                                            indicatorWidth: 18
                                                            textHorizontalPadding: 19
                                                            from: 0
                                                            to: 255
                                                            value: root.textColorComponent(modelData, "blue", 0)
                                                            onValueModified: root.updateTextColorValue(index, "blue", value)
                                                        }

                                                        Rectangle {
                                                            id: removeFontColorButton
                                                            Layout.preferredWidth: 24
                                                            Layout.preferredHeight: 26
                                                            Layout.alignment: Qt.AlignVCenter
                                                            radius: 6
                                                            color: removeFontColorMouse.pressed
                                                                ? Qt.rgba(1, 0.23, 0.19, 0.24)
                                                                : (removeFontColorMouse.containsMouse ? Qt.rgba(1, 0.23, 0.19, 0.14) : "transparent")
                                                            border.color: removeFontColorMouse.containsMouse ? Qt.rgba(1, 0.23, 0.19, 0.38) : "transparent"
                                                            border.width: 1

                                                            readonly property color iconColor: removeFontColorMouse.containsMouse ? colors.danger : colors.textMuted

                                                            Item {
                                                                anchors.centerIn: parent
                                                                width: 10
                                                                height: 10

                                                                Rectangle {
                                                                    anchors.centerIn: parent
                                                                    width: 11
                                                                    height: 2
                                                                    radius: 1
                                                                    rotation: 45
                                                                    color: removeFontColorButton.iconColor
                                                                }

                                                                Rectangle {
                                                                    anchors.centerIn: parent
                                                                    width: 11
                                                                    height: 2
                                                                    radius: 1
                                                                    rotation: -45
                                                                    color: removeFontColorButton.iconColor
                                                                }
                                                            }

                                                            MouseArea {
                                                                id: removeFontColorMouse
                                                                anchors.fill: parent
                                                                hoverEnabled: true
                                                                cursorShape: Qt.PointingHandCursor
                                                                onClicked: root.removeTextColor(index)
                                                            }

                                                            ThemedToolTip {
                                                                target: removeFontColorButton
                                                                visible: removeFontColorMouse.containsMouse
                                                                text: root.trText("RemoveFontColor", "Remove color")
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Rectangle {
                                        id: addFontColorButton
                                        Layout.preferredWidth: 34
                                        Layout.preferredHeight: 34
                                        radius: 6
                                        color: addFontColorButtonMouse.pressed
                                            ? colors.accentPressed
                                            : (addFontColorButtonMouse.containsMouse ? colors.accentHover : colors.accent)
                                        border.color: addFontColorButtonMouse.containsMouse ? colors.accent : toolTheme.dividers.default.color
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: "+"
                                            color: colors.textInverted
                                            font.family: fonts.title.family
                                            font.pixelSize: 16
                                            font.bold: true
                                            verticalAlignment: Text.AlignVCenter
                                            horizontalAlignment: Text.AlignHCenter
                                        }

                                        MouseArea {
                                            id: addFontColorButtonMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.addTextColor()
                                        }

                                        ThemedToolTip {
                                            target: addFontColorButton
                                            visible: addFontColorButtonMouse.containsMouse
                                            text: root.trText("AddFontColor", "Add color")
                                        }
                                    }
                                }

                                ColumnLayout {
                                    visible: root.mode === "new"
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 1
                                    spacing: 6

                                    Label {
                                        text: root.trText("FontName", "Font Name")
                                        color: colors.textMuted
                                        font.family: fonts.small.family
                                        font.pixelSize: Number(fonts.small.pixelSize)
                                    }
                                    FormTextField {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 120
                                        Layout.preferredHeight: 32
                                        text: root.setting("atlasName", "ape_font")
                                        onEditingFinished: root.updateSetting("atlasName", text)
                                    }
                                }

                                Flow {
                                    id: fontPrimaryControlFlow
                                    visible: root.mode === "new"
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 1
                                    spacing: 10

                                    RowLayout {
                                        width: root.responsiveFlowItemWidth(fontPrimaryControlFlow.width, 188, 3, fontPrimaryControlFlow.spacing)
                                        height: 32
                                        spacing: 8
                                        Label {
                                            Layout.preferredWidth: 58
                                            text: root.trText("SizePx", "Size")
                                            color: colors.textMuted
                                            elide: Text.ElideRight
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        FormSpinBox {
                                            Layout.preferredWidth: 90
                                            Layout.preferredHeight: 32
                                            from: 1
                                            to: 256
                                            value: root.boundedNumberSetting("sizePx", 16, 1, 256)
                                            onValueModified: root.updateNumberSetting("sizePx", value, 16, 1, 256)
                                        }
                                    }

                                    RowLayout {
                                        width: root.responsiveFlowItemWidth(fontPrimaryControlFlow.width, 188, 3, fontPrimaryControlFlow.spacing)
                                        height: 32
                                        spacing: 8
                                        Label {
                                            Layout.preferredWidth: 76
                                            text: root.trText("HeightPercent", "Height %")
                                            color: colors.textMuted
                                            elide: Text.ElideRight
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        FormSpinBox {
                                            Layout.preferredWidth: 90
                                            Layout.preferredHeight: 32
                                            from: 10
                                            to: 400
                                            value: root.boundedNumberSetting("heightPercent", 100, 10, 400)
                                            onValueModified: root.updateNumberSetting("heightPercent", value, 100, 10, 400)
                                        }
                                    }
                                    RowLayout {
                                        width: root.responsiveFlowItemWidth(fontPrimaryControlFlow.width, 188, 3, fontPrimaryControlFlow.spacing)
                                        height: 32
                                        spacing: 8
                                        Label {
                                            Layout.preferredWidth: 58
                                            text: root.trText("Outline", "Outline")
                                            color: colors.textMuted
                                            elide: Text.ElideRight
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        FormSpinBox {
                                            Layout.preferredWidth: 90
                                            Layout.preferredHeight: 32
                                            from: 0
                                            to: 32
                                            value: root.boundedNumberSetting("outlineThickness", 0, 0, 32)
                                            onValueModified: root.updateNumberSetting("outlineThickness", value, 0, 0, 32)
                                        }
                                    }
                                }

                                Flow {
                                    id: fontToggleFlow
                                    visible: root.mode === "new"
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 1
                                    spacing: 8

                                    FormCheckBox { width: root.fontToggleItemWidth(fontToggleFlow.width, fontToggleFlow.spacing); text: root.trText("Bold", "Bold"); toolTipText: root.trText("BoldTip", "Rasterize glyphs with a bold face."); checked: root.boolSetting("bold", false); onToggled: root.updateSetting("bold", checked) }
                                    FormCheckBox { width: root.fontToggleItemWidth(fontToggleFlow.width, fontToggleFlow.spacing); text: root.trText("Italic", "Italic"); toolTipText: root.trText("ItalicTip", "Rasterize glyphs with an italic face."); checked: root.boolSetting("italic", false); onToggled: root.updateSetting("italic", checked) }
                                    FormCheckBox { width: root.fontToggleItemWidth(fontToggleFlow.width, fontToggleFlow.spacing); text: root.trText("Underline", "Line"); toolTipText: root.trText("UnderlineTip", "Draw underline strokes into exported glyphs."); checked: root.boolSetting("underline", false); onToggled: root.updateSetting("underline", checked) }
                                    FormCheckBox { width: root.fontToggleItemWidth(fontToggleFlow.width, fontToggleFlow.spacing); text: root.trText("Smoothing", "Smooth"); toolTipText: root.trText("SmoothingTip", "Use antialiasing while rasterizing the source font."); checked: root.boolSetting("smoothing", true); onToggled: root.updateSetting("smoothing", checked) }

                                    FormCheckBox { width: root.fontToggleItemWidth(fontToggleFlow.width, fontToggleFlow.spacing); text: root.trText("ClearType", "Subpx"); toolTipText: root.trText("ClearTypeTip", "Use subpixel rendering when supported by the rasterizer."); checked: root.boolSetting("clearType", true); onToggled: root.updateSetting("clearType", checked) }
                                    FormCheckBox { width: root.fontToggleItemWidth(fontToggleFlow.width, fontToggleFlow.spacing); text: root.trText("ForceOffsetsZero", "Zero"); toolTipText: root.trText("ForceOffsetsZeroTip", "Force exported glyph x/y offsets to zero for HOI4 compatibility."); checked: root.boolSetting("forceOffsetsZero", true); onToggled: root.updateSetting("forceOffsetsZero", checked) }
                                }

                                Flow {
                                    id: fontSpacingControlFlow
                                    visible: root.mode === "new"
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 1
                                    spacing: 10

                                    RowLayout {
                                        width: root.responsiveFlowItemWidth(fontSpacingControlFlow.width, 318, 2, fontSpacingControlFlow.spacing)
                                        height: 32
                                        spacing: 8
                                        Label {
                                            Layout.preferredWidth: 58
                                            text: root.trText("Padding", "Padding")
                                            color: colors.textMuted
                                            wrapMode: Text.NoWrap
                                            clip: false
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        FormSpinBox { Layout.fillWidth: true; Layout.preferredWidth: 58; Layout.preferredHeight: 32; toolTipText: root.trText("PaddingUpTip", "Top padding in pixels."); from: 0; to: 128; value: root.boundedNumberSetting("paddingUp", 0, 0, 128); onValueModified: root.updateNumberSetting("paddingUp", value, 0, 0, 128) }
                                        FormSpinBox { Layout.fillWidth: true; Layout.preferredWidth: 58; Layout.preferredHeight: 32; toolTipText: root.trText("PaddingRightTip", "Right padding in pixels."); from: 0; to: 128; value: root.boundedNumberSetting("paddingRight", 0, 0, 128); onValueModified: root.updateNumberSetting("paddingRight", value, 0, 0, 128) }
                                        FormSpinBox { Layout.fillWidth: true; Layout.preferredWidth: 58; Layout.preferredHeight: 32; toolTipText: root.trText("PaddingDownTip", "Bottom padding in pixels."); from: 0; to: 128; value: root.boundedNumberSetting("paddingDown", 0, 0, 128); onValueModified: root.updateNumberSetting("paddingDown", value, 0, 0, 128) }
                                        FormSpinBox { Layout.fillWidth: true; Layout.preferredWidth: 58; Layout.preferredHeight: 32; toolTipText: root.trText("PaddingLeftTip", "Left padding in pixels."); from: 0; to: 128; value: root.boundedNumberSetting("paddingLeft", 0, 0, 128); onValueModified: root.updateNumberSetting("paddingLeft", value, 0, 0, 128) }
                                    }

                                    RowLayout {
                                        width: root.responsiveFlowItemWidth(fontSpacingControlFlow.width, 318, 2, fontSpacingControlFlow.spacing)
                                        height: 32
                                        spacing: 8
                                        Label {
                                            Layout.preferredWidth: 58
                                            text: root.trText("Spacing", "Spacing")
                                            color: colors.textMuted
                                            wrapMode: Text.NoWrap
                                            clip: false
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        FormSpinBox { Layout.fillWidth: true; Layout.preferredWidth: 70; Layout.preferredHeight: 32; toolTipText: root.trText("SpacingHorizontalTip", "Extra horizontal spacing between glyphs."); from: 0; to: 128; value: root.boundedNumberSetting("spacingHorizontal", 0, 0, 128); onValueModified: root.updateNumberSetting("spacingHorizontal", value, 0, 0, 128) }
                                        FormSpinBox { Layout.fillWidth: true; Layout.preferredWidth: 70; Layout.preferredHeight: 32; toolTipText: root.trText("SpacingVerticalTip", "Extra vertical spacing between glyph rows."); from: 0; to: 128; value: root.boundedNumberSetting("spacingVertical", 0, 0, 128); onValueModified: root.updateNumberSetting("spacingVertical", value, 0, 0, 128) }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    LoadingOverlay {
        id: hostLoadingOverlay
        anchors.fill: parent
        backdropSource: toolBridge.acrylicSource
        hostState: ({
            "active": root.loadingActive,
            "text": root.loadingDisplayText()
        })
    }

    Dialog {
        id: overwriteDialog
        modal: true
        anchors.centerIn: parent
        title: root.trText("ConfirmOverwriteTitle", "Confirm Overwrite")
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: root.dispatchAction("confirm_overwrite")
        onRejected: root.dispatchAction("cancel_overwrite")
        contentItem: Text {
            width: 420
            text: root.trText("ConfirmOverwrite", "The following files already exist. Do you want to overwrite them?\n\n%1").replace("%1", root.pendingOverwriteFiles.join("\n"))
            color: colors.textPrimary
            wrapMode: Text.WordWrap
            font.family: fonts.body.family
            font.pixelSize: Number(fonts.body.pixelSize)
        }
    }
}
