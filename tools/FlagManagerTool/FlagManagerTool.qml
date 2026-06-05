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
    property int sizeIndex: 0
    property string selectedTag: ""
    property string selectedImportId: ""
    property var flagCards: []
    property var currentImport: ({})
    property int importCount: 0
    property bool pendingOverwrite: false
    property var pendingOverwriteFiles: []
    property string lastError: ""
    property string statusText: ""
    property int cropLeft: 0
    property int cropTop: 0
    property int cropRight: 0
    property int cropBottom: 0
    property string flagName: ""
    property real zoom: 1.0
    property bool refreshingFields: false
    property string invalidCropField: ""
    property bool nameDirty: false
    property bool cropDirty: false
    property bool loadingActive: true
    property string loadingText: ""

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

    function dataSource(base64) {
        const value = safeString(base64)
        return value.length ? toolBridge.imageSource(value) : ""
    }

    function loadingDisplayText() {
        var text = safeString(loadingText)
        if (!text.length || text === "Starting worker process...") {
            return trText("LoadingFlags", "Loading flags...")
        }
        return text
    }

    function currentImageSource() {
        if (!currentImport || currentImport.previewBase64 === undefined) {
            return ""
        }
        return toolBridge.imageSource(safeString(currentImport.previewBase64), safeString(currentImport.id || currentImport.name || "currentImport"))
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

    function dispatchAction(actionType, targetId, argumentsObject) {
        toolBridge.dispatchAction(actionType, targetId || "", argumentsObject || {})
    }

    function refreshState() {
        mode = safeString(toolBridge.value("mode", "manage"))
        sizeIndex = Number(toolBridge.value("sizeIndex", 0))
        selectedTag = safeString(toolBridge.value("selectedTag", ""))
        selectedImportId = safeString(toolBridge.value("selectedImportId", ""))
        flagCards = mode === "new" ? [] : toolBridge.value("flagCards", [])
        currentImport = toolBridge.value("currentImport", {})
        importCount = Number(toolBridge.value("importCount", 0))
        pendingOverwrite = !!toolBridge.value("pendingOverwrite", false)
        pendingOverwriteFiles = toolBridge.value("pendingOverwriteFiles", [])
        lastError = safeString(toolBridge.value("lastError", ""))
        statusText = safeString(toolBridge.value("statusText", ""))
        loadingActive = !!toolBridge.value("loadingActive", false)
        loadingText = safeString(toolBridge.value("loadingText", trText("LoadingFlags", "Loading flags...")))

        refreshingFields = true
        flagName = safeString(currentImport && currentImport.name !== undefined ? currentImport.name : "")
        var crop = currentImport && currentImport.crop !== undefined ? currentImport.crop : ({})
        cropLeft = Number(crop.left !== undefined ? crop.left : 0)
        cropTop = Number(crop.top !== undefined ? crop.top : 0)
        cropRight = Number(crop.right !== undefined ? crop.right : 0)
        cropBottom = Number(crop.bottom !== undefined ? crop.bottom : 0)
        normalizeLocalCrop()
        invalidCropField = ""
        nameDirty = false
        cropDirty = false
        refreshingFields = false

        if (pendingOverwrite && !overwriteDialog.opened) {
            overwriteDialog.open()
        }
    }

    function imageWidth() {
        return currentImport && Number(currentImport.width) > 0 ? Number(currentImport.width) : 0
    }

    function imageHeight() {
        return currentImport && Number(currentImport.height) > 0 ? Number(currentImport.height) : 0
    }

    function cropValue(key) {
        if (key === "left") return cropLeft
        if (key === "top") return cropTop
        if (key === "right") return cropRight
        return cropBottom
    }

    function setCropValue(key, value) {
        if (key === "left") cropLeft = value
        else if (key === "top") cropTop = value
        else if (key === "right") cropRight = value
        else cropBottom = value
    }

    function cropMinimum(key) {
        var w = imageWidth()
        var h = imageHeight()
        if (key === "right") return w > 1 ? Math.min(w - 1, cropLeft + 1) : 0
        if (key === "bottom") return h > 1 ? Math.min(h - 1, cropTop + 1) : 0
        return 0
    }

    function cropMaximum(key) {
        var w = imageWidth()
        var h = imageHeight()
        if (key === "left") return w > 1 ? Math.max(0, cropRight - 1) : 0
        if (key === "top") return h > 1 ? Math.max(0, cropBottom - 1) : 0
        if (key === "right") return w > 0 ? w - 1 : 0
        return h > 0 ? h - 1 : 0
    }

    function boundedCropValue(key, candidate) {
        var minValue = cropMinimum(key)
        var maxValue = cropMaximum(key)
        if (maxValue < minValue) {
            maxValue = minValue
        }
        var parsed = Number(candidate)
        if (isNaN(parsed)) {
            parsed = cropValue(key)
        }
        return Math.max(minValue, Math.min(maxValue, Math.round(parsed)))
    }

    function isCropRangeValid(left, top, right, bottom) {
        var w = imageWidth()
        var h = imageHeight()
        return w > 1 && h > 1
            && left >= 0 && top >= 0
            && right < w && bottom < h
            && left < right && top < bottom
    }

    function isCropValid() {
        return isCropRangeValid(cropLeft, cropTop, cropRight, cropBottom)
    }

    function cropCandidate(key, candidate) {
        var text = safeString(candidate).trim()
        if (!/^-?\d+$/.test(text)) {
            return { "valid": false, "field": key }
        }

        var parsed = Number(text)
        if (isNaN(parsed)) {
            return { "valid": false, "field": key }
        }

        var left = cropLeft
        var top = cropTop
        var right = cropRight
        var bottom = cropBottom
        if (key === "left") left = parsed
        else if (key === "top") top = parsed
        else if (key === "right") right = parsed
        else bottom = parsed

        return {
            "valid": isCropRangeValid(left, top, right, bottom),
            "field": key,
            "left": left,
            "top": top,
            "right": right,
            "bottom": bottom
        }
    }

    function normalizeLocalCrop() {
        if (imageWidth() <= 0 || imageHeight() <= 0) {
            return
        }
        cropLeft = boundedCropValue("left", cropLeft)
        cropTop = boundedCropValue("top", cropTop)
        cropRight = boundedCropValue("right", cropRight)
        cropBottom = boundedCropValue("bottom", cropBottom)
    }

    function applyCropInput(key, candidate, sendUpdate) {
        if (refreshingFields || !selectedImportId.length) {
            return false
        }
        var candidateCrop = cropCandidate(key, candidate)
        if (!candidateCrop.valid) {
            invalidCropField = candidateCrop.field
            lastError = trText("ErrorInvalidCrop", "Invalid crop range")
            return false
        }

        invalidCropField = ""
        cropLeft = candidateCrop.left
        cropTop = candidateCrop.top
        cropRight = candidateCrop.right
        cropBottom = candidateCrop.bottom
        cropDirty = true
        if (sendUpdate) {
            sendCropUpdate()
        }
        return true
    }

    function sendCropUpdate() {
        if (refreshingFields || !selectedImportId.length) {
            return
        }
        if (!isCropValid()) {
            invalidCropField = "crop"
            lastError = trText("ErrorInvalidCrop", "Invalid crop range")
            return
        }
        dispatchAction("update_import_crop", selectedImportId, {
            "id": selectedImportId,
            "crop": {
                "left": cropLeft,
                "top": cropTop,
                "right": cropRight,
                "bottom": cropBottom
            }
        })
        cropDirty = false
    }

    function currentEditorArguments() {
        if (!selectedImportId.length) {
            return {}
        }
        return {
            "hasEditorState": true,
            "editorImportId": selectedImportId,
            "editorName": flagName,
            "editorCrop": {
                "left": cropLeft,
                "top": cropTop,
                "right": cropRight,
                "bottom": cropBottom
            }
        }
    }

    function canExportCurrent() {
        return mode === "new" && selectedImportId.length > 0
    }

    function canExportAll() {
        return mode === "new" && importCount > 0
    }

    function applyTopbarAction(actionId) {
        if (actionId === "import_files") {
            importDialog.open()
            return
        }
        if (actionId === "export_current") {
            if (!canExportCurrent()) {
                return
            }
            if (!isCropValid()) {
                invalidCropField = "crop"
                lastError = trText("ErrorInvalidCrop", "Invalid crop range")
                return
            }
            dispatchAction(actionId, selectedImportId, currentEditorArguments())
            nameDirty = false
            cropDirty = false
            return
        }
        if (actionId === "export_all") {
            if (!canExportAll()) {
                return
            }
            if (selectedImportId.length > 0 && !isCropValid()) {
                invalidCropField = "crop"
                lastError = trText("ErrorInvalidCrop", "Invalid crop range")
                return
            }
            dispatchAction(actionId, selectedImportId, currentEditorArguments())
            nameDirty = false
            cropDirty = false
            return
        }
        if (actionId === "set_mode_manage" || actionId === "set_mode_new") {
            if (!isCropValid() && selectedImportId.length) {
                invalidCropField = "crop"
                lastError = trText("ErrorInvalidCrop", "Invalid crop range")
                return
            }
            dispatchAction(actionId, selectedImportId, currentEditorArguments())
            nameDirty = false
            cropDirty = false
            return
        }
        dispatchAction(actionId)
    }

    Connections {
        target: toolBridge
        function onValuesChanged() {
            root.refreshState()
        }
        function onLocalizedStringsChanged() {
            root.refreshState()
        }
    }

    Component.onCompleted: refreshState()

    FileDialog {
        id: importDialog
        title: root.trText("ImportFiles", "Import Files")
        fileMode: FileDialog.OpenFiles
        nameFilters: [ root.trText("ImageFilter", "Images (*.png *.jpg *.jpeg *.tga *.dds *.jxr *.webp)") ]
        onAccepted: {
            var paths = []
            for (var i = 0; i < selectedFiles.length; ++i) {
                paths.push(root.localPathFromUrl(selectedFiles[i]))
            }
            if (paths.length) {
                root.dispatchAction("import_files", "", { "paths": paths })
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
                    {
                        "actionId": "set_mode_manage",
                        "text": root.trText("TabManage", "Manage"),
                        "checked": root.mode !== "new",
                        "variant": "toolbar",
                        "width": 92
                    },
                    {
                        "actionId": "set_mode_new",
                        "text": root.trText("TabNew", "Create"),
                        "checked": root.mode === "new",
                        "variant": "toolbar",
                        "width": 92
                    }
                ],
                "rightButtons": root.mode === "new"
                    ? [
                        { "actionId": "import_files", "text": root.trText("ImportFiles", "Import Files"), "shortcut": "Ctrl+O", "width": 106 },
                        { "actionId": "export_current", "text": root.trText("Export", "Export Current"), "shortcut": "Ctrl+S", "enabled": root.canExportCurrent(), "width": 118 },
                        { "actionId": "export_all", "text": root.trText("ExportAll", "Export All"), "shortcut": "Ctrl+Shift+S", "enabled": root.canExportAll(), "width": 100 }
                    ]
                    : [
                        { "actionId": "set_size_0", "text": root.trText("SizeLarge", "Large"), "shortcut": "L", "checked": root.sizeIndex === 0, "width": 82 },
                        { "actionId": "set_size_1", "text": root.trText("SizeMedium", "Medium"), "shortcut": "M", "checked": root.sizeIndex === 1, "width": 82 },
                        { "actionId": "set_size_2", "text": root.trText("SizeSmall", "Small"), "shortcut": "S", "checked": root.sizeIndex === 2, "width": 82 }
                    ]
            })
            onActionInvoked: function(actionId) {
                root.applyTopbarAction(actionId)
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            DropArea {
                anchors.fill: parent
                enabled: root.mode === "new"
                onDropped: function(drop) {
                    var paths = []
                    if (drop.urls) {
                        for (var i = 0; i < drop.urls.length; ++i) {
                            paths.push(root.localPathFromUrl(drop.urls[i]))
                        }
                    }
                    if (paths.length) {
                        root.dispatchAction("import_files", "", { "paths": paths })
                    }
                }
            }

            ScrollView {
                anchors.fill: parent
                visible: root.mode !== "new"
                clip: true

                GridLayout {
                    width: Math.max(parent.width, 1)
                    columns: Math.max(1, Math.floor(width / 180))
                    rowSpacing: 20
                    columnSpacing: 20

                    Repeater {
                        model: root.flagCards

                        delegate: Item {
                            Layout.preferredWidth: 160
                            Layout.preferredHeight: 116
                            property string cardImageSource: root.dataSource(modelData.imageBase64)
                            property int expectedFlagWidth: root.sizeIndex === 2 ? 10 : (root.sizeIndex === 1 ? 41 : 82)
                            property int expectedFlagHeight: root.sizeIndex === 2 ? 7 : (root.sizeIndex === 1 ? 26 : 52)

                            Column {
                                anchors.centerIn: parent
                                width: parent.width
                                spacing: 6

                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: flagImage.visible && flagImage.implicitWidth > 0
                                        ? flagImage.implicitWidth + 2
                                        : Math.max(expectedFlagWidth, missingText.implicitWidth) + 8
                                    height: flagImage.visible && flagImage.implicitHeight > 0
                                        ? flagImage.implicitHeight + 2
                                        : Math.max(expectedFlagHeight, missingText.implicitHeight) + 8
                                    color: flagImage.visible ? "transparent" : colors.surfaceAlt
                                    border.color: colors.border
                                    border.width: 1

                                    Image {
                                        id: flagImage
                                        anchors.centerIn: parent
                                        width: implicitWidth
                                        height: implicitHeight
                                        fillMode: Image.PreserveAspectFit
                                        source: cardImageSource
                                        visible: cardImageSource.length > 0
                                        cache: false
                                        asynchronous: false
                                        smooth: false
                                    }

                                    Text {
                                        id: missingText
                                        anchors.centerIn: parent
                                        visible: !modelData.imageBase64 || !String(modelData.imageBase64).length
                                        text: root.trText("Missing", "MISSING")
                                        color: colors.textMuted
                                        font.family: fonts.small.family
                                        font.pixelSize: root.sizeIndex === 2 ? 8 : Number(fonts.small.pixelSize)
                                        font.weight: Font.DemiBold
                                    }
                                }

                                Text {
                                    width: parent.width
                                    text: root.safeString(modelData.name)
                                    color: colors.textPrimary
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                    font.family: fonts.body.family
                                    font.pixelSize: Number(fonts.body.pixelSize)
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                visible: root.mode === "new"
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: colors.surfaceAlt
                    clip: true

                    Flickable {
                        id: previewFlick
                        anchors.fill: parent
                        contentWidth: Math.max(width, previewImage.implicitWidth * root.zoom)
                        contentHeight: Math.max(height, previewImage.implicitHeight * root.zoom)
                        boundsBehavior: Flickable.StopAtBounds

                        Image {
                            id: previewImage
                            property string imageSourceText: root.currentImageSource()
                            source: imageSourceText
                            visible: imageSourceText.length > 0
                            cache: false
                            asynchronous: false
                            fillMode: Image.PreserveAspectFit
                            smooth: false
                            width: implicitWidth * root.zoom
                            height: implicitHeight * root.zoom
                            x: Math.max(0, (previewFlick.width - width) / 2)
                            y: Math.max(0, (previewFlick.height - height) / 2)

                            Rectangle {
                                visible: previewImage.visible && currentImport && currentImport.width > 0 && currentImport.height > 0
                                x: (root.cropLeft / currentImport.width) * parent.width
                                y: (root.cropTop / currentImport.height) * parent.height
                                width: Math.max(1, ((root.cropRight - root.cropLeft + 1) / currentImport.width) * parent.width)
                                height: Math.max(1, ((root.cropBottom - root.cropTop + 1) / currentImport.height) * parent.height)
                                color: "transparent"
                                border.color: "#22C55E"
                                border.width: 2
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !previewImage.visible
                            text: root.trText("NoImage", "No Image")
                            color: colors.textMuted
                            font.family: fonts.title.family
                            font.pixelSize: Number(fonts.title.pixelSize)
                        }

                        WheelHandler {
                            target: null
                            onWheel: function(event) {
                                root.zoom = Math.max(0.1, Math.min(5.0, root.zoom + (event.angleDelta.y > 0 ? 0.1 : -0.1)))
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    color: surfaces.topBar
                    border.color: toolTheme.dividers.default.color
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 8

                        Item { Layout.fillWidth: true }

                        Text {
                            text: root.trText("FlagName", "Flag Name:")
                            color: colors.textMuted
                            font.family: fonts.small.family
                            font.pixelSize: Number(fonts.small.pixelSize)
                        }

                        TextField {
                            Layout.preferredWidth: 120
                            Layout.preferredHeight: 32
                            text: root.flagName
                            enabled: root.selectedImportId.length > 0
                            selectByMouse: true
                            placeholderText: root.trText("NamePlaceholder", "TAG_suffix")
                            font.family: fonts.body.family
                            font.pixelSize: Number(fonts.body.pixelSize)
                            color: colors.textPrimary
                            placeholderTextColor: colors.textMuted
                            selectionColor: colors.selection
                            selectedTextColor: colors.textInverted
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            padding: 0
                            leftPadding: 10
                            rightPadding: 10
                            topPadding: 6
                            bottomPadding: 6
                            background: Rectangle {
                                radius: 8
                                color: colors.surface
                                border.color: toolTheme.dividers.default.color
                                border.width: 1
                            }
                            onTextEdited: {
                                if (!root.refreshingFields) {
                                    root.flagName = text
                                    root.nameDirty = true
                                }
                            }
                            onEditingFinished: {
                                if (!root.refreshingFields && root.nameDirty) {
                                    root.dispatchAction("update_import_name", root.selectedImportId, {
                                        "id": root.selectedImportId,
                                        "name": text
                                    })
                                    root.nameDirty = false
                                }
                            }
                        }

                        Text {
                            text: root.trText("Crop", "Crop:")
                            color: colors.textMuted
                            font.family: fonts.small.family
                            font.pixelSize: Number(fonts.small.pixelSize)
                        }

                        Repeater {
                            model: [
                                { "label": root.trText("L", "L"), "key": "left" },
                                { "label": root.trText("T", "T"), "key": "top" },
                                { "label": root.trText("R", "R"), "key": "right" },
                                { "label": root.trText("B", "B"), "key": "bottom" }
                            ]

                            delegate: RowLayout {
                                spacing: 4
                                Text {
                                    text: modelData.label
                                    color: colors.textMuted
                                    font.family: fonts.small.family
                                    font.pixelSize: Number(fonts.small.pixelSize)
                                }
                                TextField {
                                    id: cropField
                                    property bool invalid: root.invalidCropField === modelData.key
                                    Layout.preferredWidth: 68
                                    Layout.preferredHeight: 32
                                    enabled: root.selectedImportId.length > 0
                                    selectByMouse: true
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    font.family: fonts.body.family
                                    font.pixelSize: Number(fonts.body.pixelSize)
                                    color: enabled ? colors.textPrimary : colors.textMuted
                                    selectionColor: colors.selection
                                    selectedTextColor: colors.textInverted
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    padding: 0
                                    leftPadding: 8
                                    rightPadding: 8
                                    topPadding: 6
                                    bottomPadding: 6
                                    background: Rectangle {
                                        radius: 8
                                        color: colors.surface
                                        border.color: cropField.invalid ? colors.danger : toolTheme.dividers.default.color
                                        border.width: cropField.invalid ? 2 : 1
                                    }

                                    function syncFromRoot(force) {
                                        if (force || !activeFocus || !invalid) {
                                            text = String(root.cropValue(modelData.key))
                                        }
                                    }

                                    Component.onCompleted: syncFromRoot(true)

                                    Connections {
                                        target: root
                                        function onCropLeftChanged() { cropField.syncFromRoot(false) }
                                        function onCropTopChanged() { cropField.syncFromRoot(false) }
                                        function onCropRightChanged() { cropField.syncFromRoot(false) }
                                        function onCropBottomChanged() { cropField.syncFromRoot(false) }
                                        function onSelectedImportIdChanged() { cropField.syncFromRoot(true) }
                                        function onCurrentImportChanged() { cropField.syncFromRoot(true) }
                                        function onInvalidCropFieldChanged() {
                                            if (!cropField.invalid) {
                                                cropField.syncFromRoot(false)
                                            }
                                        }
                                    }

                                    onTextEdited: {
                                        root.applyCropInput(modelData.key, text, false)
                                    }

                                    onEditingFinished: {
                                        if (root.cropDirty && !invalid) {
                                            root.sendCropUpdate()
                                        }
                                    }

                                    onActiveFocusChanged: {
                                        if (!activeFocus) {
                                            if (invalid) {
                                                root.invalidCropField = ""
                                            }
                                            syncFromRoot(true)
                                        }
                                    }

                                    ThemedToolTip {
                                        id: invalidCropTooltip
                                        target: cropField
                                        visible: cropField.invalid && cropField.hovered
                                        text: root.trText("ErrorInvalidCrop", "Invalid crop range")
                                        delay: 120
                                        timeout: 4000
                                        x: Math.round((cropField.width - implicitWidth) / 2)
                                        y: -implicitHeight - 7
                                    }
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
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
            text: root.safeString(root.trText("ConfirmOverwrite", "The following files already exist. Do you want to overwrite them?\n\n%1"))
                .replace("%1", root.pendingOverwriteFiles.join("\n"))
            color: colors.textPrimary
            wrapMode: Text.WordWrap
            font.family: fonts.body.family
            font.pixelSize: Number(fonts.body.pixelSize)
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
