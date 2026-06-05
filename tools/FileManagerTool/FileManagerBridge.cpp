//-------------------------------------------------------------------------------------
// FileManagerBridge.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "FileManagerBridge.h"

#include "../../src/ToolRuntimeContext.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <utility>

namespace FileManagerBridge {

std::unique_ptr<WorkerSession> g_legacySession;
std::string g_legacyLastError;
std::string g_legacySerializedState;

namespace {

FileManager::FileSource toCoreSource(ToolRuntimeContext::EffectiveFileSource source) {
    switch (source) {
    case ToolRuntimeContext::EffectiveFileSource::Game:
        return FileManager::FileSource::Game;
    case ToolRuntimeContext::EffectiveFileSource::Mod:
        return FileManager::FileSource::Mod;
    case ToolRuntimeContext::EffectiveFileSource::Dlc:
        return FileManager::FileSource::Dlc;
    default:
        return FileManager::FileSource::Unknown;
    }
}

class ToolRuntimeFileSystem final : public FileManager::IFileSystem {
public:
    FileManager::EffectiveFileListResult listEffectiveFiles(const std::string& relativeRoot,
                                                            const std::string& suffixFilter) const override {
        const ToolRuntimeContext::EffectiveFileListResult runtimeResult =
            ToolRuntimeContext::instance().listEffectiveFiles(
                QString::fromUtf8(relativeRoot.c_str()),
                QString::fromUtf8(suffixFilter.c_str())
            );

        FileManager::EffectiveFileListResult result;
        result.success = runtimeResult.success;
        if (!runtimeResult.success) {
            result.errorMessage = runtimeResult.errorMessage.toUtf8().toStdString();
            return result;
        }

        result.entries.reserve(static_cast<std::size_t>(runtimeResult.entries.size()));
        for (const ToolRuntimeContext::EffectiveFileEntry& runtimeEntry : runtimeResult.entries) {
            FileManager::EffectiveFileEntry entry;
            entry.logicalPath = runtimeEntry.logicalPath.toUtf8().toStdString();
            entry.source = toCoreSource(runtimeEntry.source);
            result.entries.push_back(std::move(entry));
        }

        return result;
    }
};

QString localizedFallback(const QString& key) {
    static QMap<QString, QString> fallbacks;
    if (fallbacks.isEmpty()) {
        fallbacks.insert(QStringLiteral("Name"), QStringLiteral("File Manager"));
        fallbacks.insert(QStringLiteral("Description"), QStringLiteral("Browse and manage game files."));
        fallbacks.insert(QStringLiteral("Files"), QStringLiteral("Files"));
        fallbacks.insert(QStringLiteral("SelectedFile"), QStringLiteral("Selected File"));
        fallbacks.insert(QStringLiteral("NoSelection"), QStringLiteral("No file selected"));
        fallbacks.insert(QStringLiteral("FileName"), QStringLiteral("File Name"));
        fallbacks.insert(QStringLiteral("Source"), QStringLiteral("Source"));
        fallbacks.insert(QStringLiteral("RelativePath"), QStringLiteral("Relative Path"));
        fallbacks.insert(QStringLiteral("TotalFiles"), QStringLiteral("Total"));
        fallbacks.insert(QStringLiteral("FilteredFiles"), QStringLiteral("Filtered"));
        fallbacks.insert(QStringLiteral("Refresh"), QStringLiteral("Refresh"));
        fallbacks.insert(QStringLiteral("SearchPlaceholder"), QStringLiteral("Search files..."));
        fallbacks.insert(QStringLiteral("SelectFileHint"), QStringLiteral("Select a file to see details"));
        fallbacks.insert(QStringLiteral("Ready"), QStringLiteral("Ready"));
        fallbacks.insert(QStringLiteral("Empty"), QStringLiteral("No files to display"));
        fallbacks.insert(QStringLiteral("SourceGame"), QStringLiteral("Game"));
        fallbacks.insert(QStringLiteral("SourceMod"), QStringLiteral("Mod"));
        fallbacks.insert(QStringLiteral("SourceDLC"), QStringLiteral("DLC"));
        fallbacks.insert(QStringLiteral("SourceUnknown"), QStringLiteral("Unknown"));
    }
    return fallbacks.value(key, key);
}

QString localizedString(const WorkerSession* session, const QString& key) {
    if (!session) {
        return localizedFallback(key);
    }
    return session->localizedStrings.value(key, localizedFallback(key));
}

QString sourceText(const WorkerSession* session, FileManager::FileSource source) {
    return localizedString(session, QString::fromStdString(FileManager::fileSourceKey(source)));
}

bool loadLocalizedStringsFromJson(WorkerSession* session, const QJsonObject& localizedStringsObject) {
    if (!session || localizedStringsObject.isEmpty()) {
        return false;
    }

    session->localizedStrings.clear();
    for (auto iterator = localizedStringsObject.begin(); iterator != localizedStringsObject.end(); ++iterator) {
        if (iterator->isString()) {
            session->localizedStrings.insert(iterator.key(), iterator->toString());
        }
    }
    return !session->localizedStrings.isEmpty();
}

std::string normalizedActionName(QString value) {
    value = value.trimmed();
    value.replace(QStringLiteral("::"), QStringLiteral("_"));
    value.replace(QStringLiteral("-"), QStringLiteral("_"));
    value.replace(QStringLiteral(" "), QStringLiteral("_"));

    QString normalized;
    normalized.reserve(value.size());
    for (const QChar ch : value) {
        if (ch.isUpper() && !normalized.isEmpty() && normalized.back() != QLatin1Char('_')) {
            normalized.append(QLatin1Char('_'));
        }
        normalized.append(ch.toLower());
    }
    return normalized.toUtf8().toStdString();
}

QString firstNonEmptyString(const QJsonObject& object, std::initializer_list<QString> keys) {
    for (const QString& key : keys) {
        const QString value = object.value(key).toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

QJsonObject buildViewState(WorkerSession* session, const FileManager::StateSnapshot& state) {
    QJsonObject view;
    view[QStringLiteral("filterText")] = QString::fromUtf8(state.filterText.c_str());
    view[QStringLiteral("selectedPath")] = QString::fromUtf8(state.selectedPath.c_str());
    view[QStringLiteral("totalCount")] = static_cast<int>(state.totalCount);
    view[QStringLiteral("filteredCount")] = static_cast<int>(state.filteredCount);
    view[QStringLiteral("visibleCount")] = static_cast<int>(state.visibleCount);
    view[QStringLiteral("hasFiles")] = state.totalCount > 0;
    view[QStringLiteral("hasSelection")] = state.hasSelection;
    view[QStringLiteral("selectedIsDirectory")] = state.selectedIsDirectory;
    view[QStringLiteral("loadingActive")] = false;
    view[QStringLiteral("loadingText")] = QString();
    view[QStringLiteral("lastError")] = QString::fromUtf8(state.lastError.c_str());
    view[QStringLiteral("statusText")] = state.lastError.empty()
        ? localizedString(session, QStringLiteral("Ready"))
        : QString::fromUtf8(state.lastError.c_str());

    QJsonArray treeRows;
    for (const FileManager::TreeRow& treeRow : state.treeRows) {
        QJsonObject row;
        row[QStringLiteral("id")] = QString::fromUtf8(treeRow.rowId.c_str());
        row[QStringLiteral("rowId")] = QString::fromUtf8(treeRow.rowId.c_str());
        row[QStringLiteral("displayName")] = QString::fromUtf8(treeRow.displayName.c_str());
        row[QStringLiteral("source")] = treeRow.isDirectory ? QString() : sourceText(session, treeRow.source);
        row[QStringLiteral("relativePath")] = QString::fromUtf8(treeRow.relativePath.c_str());
        row[QStringLiteral("isDirectory")] = treeRow.isDirectory;
        row[QStringLiteral("expanded")] = treeRow.expanded;
        row[QStringLiteral("hasChildren")] = treeRow.hasChildren;
        row[QStringLiteral("selected")] = treeRow.selected;
        row[QStringLiteral("depth")] = treeRow.depth;
        row[QStringLiteral("childCount")] = static_cast<int>(treeRow.childCount);
        treeRows.append(row);
    }
    view[QStringLiteral("treeRows")] = treeRows;

    if (state.hasSelection) {
        view[QStringLiteral("selectedName")] = QString::fromUtf8(state.selectedDisplayName.c_str());
        view[QStringLiteral("selectedSource")] = state.hasSelectedFile
            ? sourceText(session, state.selectedFile.source)
            : QString();
        view[QStringLiteral("selectedRelativePath")] = QString::fromUtf8(state.selectedPath.c_str());
    } else {
        view[QStringLiteral("selectedName")] = localizedString(session, QStringLiteral("NoSelection"));
        view[QStringLiteral("selectedSource")] = QString();
        view[QStringLiteral("selectedRelativePath")] = QString();
    }

    return view;
}

QJsonObject buildSidebarState(WorkerSession* session) {
    Q_UNUSED(session);
    QJsonObject sidebar;
    sidebar[QStringLiteral("visible")] = false;
    sidebar[QStringLiteral("activeMode")] = QString();
    sidebar[QStringLiteral("modelId")] = QString();
    sidebar[QStringLiteral("modeOrder")] = QJsonArray();
    sidebar[QStringLiteral("searchEnabled")] = false;
    sidebar[QStringLiteral("selectAllEnabled")] = false;
    return sidebar;
}

QJsonObject buildTopbarState() {
    QJsonObject topbar;
    QJsonArray pageOrder;
    pageOrder.append(QStringLiteral("browser"));

    topbar[QStringLiteral("visible")] = false;
    topbar[QStringLiteral("currentPageId")] = QStringLiteral("browser");
    topbar[QStringLiteral("pageOrder")] = pageOrder;
    return topbar;
}

ToolWorkerResult applyCoreActionResult(WorkerSession* session, bool success) {
    if (!session) {
        return TOOL_WORKER_ERROR_INVALID_HANDLE;
    }
    if (!success) {
        setSessionError(session, QString::fromUtf8(session->core.lastError().c_str()));
        return TOOL_WORKER_ERROR_ACTION_FAILED;
    }
    clearSessionError(session);
    return TOOL_WORKER_SUCCESS;
}

} // namespace

ToolWorkerHandle createWorkerHandle(const char* toolId) {
    (void)toolId;
    WorkerSession* session = new (std::nothrow) WorkerSession();
    return reinterpret_cast<ToolWorkerHandle>(session);
}

void destroyWorkerHandle(ToolWorkerHandle handle) {
    delete sessionFromHandle(handle);
}

WorkerSession* sessionFromHandle(ToolWorkerHandle handle) {
    return reinterpret_cast<WorkerSession*>(handle);
}

void setSessionError(WorkerSession* session, const QString& message) {
    if (session) {
        session->lastError = message.toUtf8().toStdString();
    }
}

void clearSessionError(WorkerSession* session) {
    if (session) {
        session->lastError.clear();
    }
}

char* allocateCString(const QByteArray& utf8) {
    char* result = static_cast<char*>(std::malloc(static_cast<std::size_t>(utf8.size()) + 1U));
    if (!result) {
        return nullptr;
    }
    std::memcpy(result, utf8.constData(), static_cast<std::size_t>(utf8.size()));
    result[utf8.size()] = '\0';
    return result;
}

QJsonObject parseJsonObject(const char* jsonText) {
    if (!jsonText || *jsonText == '\0') {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(jsonText));
    return document.isObject() ? document.object() : QJsonObject();
}

QJsonObject buildStatePacket(WorkerSession* session) {
    QJsonObject packet;
    packet[QStringLiteral("pageId")] = QStringLiteral("browser");
    packet[QStringLiteral("currentPage")] = QStringLiteral("browser");
    packet[QStringLiteral("modeId")] = QStringLiteral("default");

    if (!session) {
        packet[QStringLiteral("viewState")] = QJsonObject();
        packet[QStringLiteral("values")] = QJsonObject();
        packet[QStringLiteral("sidebarState")] = QJsonObject();
        packet[QStringLiteral("topbarState")] = buildTopbarState();
        packet[QStringLiteral("runtimeVariables")] = QJsonObject();
        packet[QStringLiteral("listModels")] = QJsonArray();
        packet[QStringLiteral("patches")] = QJsonArray();
        packet[QStringLiteral("models")] = QJsonObject();
        return packet;
    }

    const FileManager::StateSnapshot state = session->core.buildState();
    const QJsonObject viewState = buildViewState(session, state);

    packet[QStringLiteral("viewState")] = viewState;
    packet[QStringLiteral("values")] = viewState;
    packet[QStringLiteral("sidebarState")] = buildSidebarState(session);
    packet[QStringLiteral("topbarState")] = buildTopbarState();
    packet[QStringLiteral("runtimeVariables")] = QJsonObject();
    packet[QStringLiteral("listModels")] = QJsonArray();
    packet[QStringLiteral("patches")] = QJsonArray();
    packet[QStringLiteral("models")] = QJsonObject();
    return packet;
}

char* serializeStatePacket(WorkerSession* session) {
    const QByteArray stateJson = QJsonDocument(buildStatePacket(session)).toJson(QJsonDocument::Compact);
    if (session) {
        session->lastSerializedState = stateJson.toStdString();
    }
    return allocateCString(stateJson);
}

ToolWorkerResult initializeSession(WorkerSession* session, const char* configJson) {
    if (!session) {
        return TOOL_WORKER_ERROR_INVALID_HANDLE;
    }

    const QJsonObject config = parseJsonObject(configJson);
    loadLocalizedStringsFromJson(session, config.value(QStringLiteral("localizedStrings")).toObject());

    session->fileSystem = std::make_unique<ToolRuntimeFileSystem>();
    session->core.setFileSystem(session->fileSystem.get());
    session->lastError.clear();
    session->lastSerializedState.clear();

    return applyCoreActionResult(session, session->core.initialize());
}

ToolWorkerResult applyActionInternal(WorkerSession* session,
                                     const char* actionType,
                                     const char* targetId,
                                     const char* argumentsJson) {
    if (!session) {
        return TOOL_WORKER_ERROR_INVALID_HANDLE;
    }

    const std::string action = normalizedActionName(QString::fromUtf8(actionType ? actionType : ""));
    const QJsonObject arguments = parseJsonObject(argumentsJson);

    if (action.empty()) {
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == "load_language") {
        loadLocalizedStringsFromJson(session, arguments.value(QStringLiteral("localizedStrings")).toObject());
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == "refresh" || action == "refresh_files" || action == "reload") {
        return applyCoreActionResult(session, session->core.refresh());
    }

    if (action == "search" || action == "search_changed" || action == "search_change" || action == "set_search_text") {
        session->core.setSearchText(firstNonEmptyString(
            arguments,
            {QStringLiteral("text"), QStringLiteral("value"), QStringLiteral("searchText"), QStringLiteral("query")}
        ).toUtf8().toStdString());
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == "reset_search" || action == "clear_search") {
        session->core.setSearchText({});
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == "toggle_directory" || action == "toggle_folder" || action == "expand_directory" || action == "collapse_directory") {
        QString directoryPath = firstNonEmptyString(
            arguments,
            {QStringLiteral("rowId"), QStringLiteral("relativePath"), QStringLiteral("targetId"), QStringLiteral("value")}
        );
        if (directoryPath.trimmed().isEmpty() && targetId && *targetId != '\0') {
            directoryPath = QString::fromUtf8(targetId);
        }
        return applyCoreActionResult(session, session->core.toggleDirectory(directoryPath.toUtf8().toStdString()));
    }

    if (action == "select_node" || action == "select_file" || action == "select_row" || action == "file_selected" || action == "on_file_selected") {
        QString selectedPath = firstNonEmptyString(
            arguments,
            {QStringLiteral("rowId"), QStringLiteral("relativePath"), QStringLiteral("targetId"), QStringLiteral("value")}
        );
        if (selectedPath.trimmed().isEmpty() && targetId && *targetId != '\0') {
            selectedPath = QString::fromUtf8(targetId);
        }
        return applyCoreActionResult(session, session->core.selectNode(selectedPath.toUtf8().toStdString()));
    }

    if (action == "page_select" || action == "button_click" || action == "sidebar_button_click") {
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    clearSessionError(session);
    return TOOL_WORKER_SUCCESS;
}

ToolWorkerResult initializeWorkerHandle(ToolWorkerHandle handle, const char* configJson) {
    return initializeSession(sessionFromHandle(handle), configJson);
}

const char* handleWorkerAction(ToolWorkerHandle handle,
                               const char* actionType,
                               const char* targetId,
                               const char* argumentsJson,
                               ToolWorkerResult* outResult) {
    WorkerSession* session = sessionFromHandle(handle);
    const ToolWorkerResult result = applyActionInternal(session, actionType, targetId, argumentsJson);
    if (outResult) {
        *outResult = result;
    }
    return serializeStatePacket(session);
}

const char* getWorkerCurrentState(ToolWorkerHandle handle, ToolWorkerResult* outResult) {
    WorkerSession* session = sessionFromHandle(handle);
    if (!session) {
        if (outResult) {
            *outResult = TOOL_WORKER_ERROR_INVALID_HANDLE;
        }
        return nullptr;
    }

    if (outResult) {
        *outResult = TOOL_WORKER_SUCCESS;
    }
    return serializeStatePacket(session);
}

const char* getWorkerInitialState(ToolWorkerHandle handle, ToolWorkerResult* outResult) {
    WorkerSession* session = sessionFromHandle(handle);
    if (!session) {
        if (outResult) {
            *outResult = TOOL_WORKER_ERROR_INVALID_HANDLE;
        }
        return nullptr;
    }

    const ToolWorkerResult result = applyCoreActionResult(session, session->core.refresh());
    if (outResult) {
        *outResult = result;
    }
    return serializeStatePacket(session);
}

const char* getWorkerLastError(ToolWorkerHandle handle) {
    WorkerSession* session = sessionFromHandle(handle);
    return session ? session->lastError.c_str() : "Invalid worker handle.";
}

char* legacyCurrentStateJson(bool* ok) {
    if (!g_legacySession) {
        if (ok) {
            *ok = false;
        }
        QJsonObject errorObject;
        errorObject[QStringLiteral("error")] = QStringLiteral("Legacy worker session is not initialized.");
        const QByteArray payload = QJsonDocument(errorObject).toJson(QJsonDocument::Compact);
        g_legacySerializedState = payload.toStdString();
        return allocateCString(payload);
    }

    if (ok) {
        *ok = true;
    }
    const QByteArray payload = QJsonDocument(buildStatePacket(g_legacySession.get())).toJson(QJsonDocument::Compact);
    g_legacySerializedState = payload.toStdString();
    return allocateCString(payload);
}

int initializeLegacyWorker(void* runtimeContext) {
    (void)runtimeContext;
    g_legacySession.reset(new WorkerSession());

    QJsonObject initConfig;
    initConfig[QStringLiteral("language")] = QStringLiteral("en_US");
    const QByteArray initJson = QJsonDocument(initConfig).toJson(QJsonDocument::Compact);

    const ToolWorkerResult result = initializeSession(g_legacySession.get(), initJson.constData());
    if (result != TOOL_WORKER_SUCCESS) {
        g_legacyLastError = g_legacySession ? g_legacySession->lastError : std::string("Legacy worker initialization failed.");
        return 1;
    }

    g_legacyLastError.clear();
    return 0;
}

void shutdownLegacyWorker() {
    g_legacySession.reset();
    g_legacyLastError.clear();
    g_legacySerializedState.clear();
}

const char* getLegacyInitialState() {
    bool ok = false;
    char* payload = legacyCurrentStateJson(&ok);
    if (!ok) {
        g_legacyLastError = "Legacy worker session is not initialized.";
    }
    return payload;
}

const char* handleLegacyAction(const char* actionJson) {
    if (!g_legacySession) {
        g_legacyLastError = "Legacy worker session is not initialized.";
        return legacyCurrentStateJson();
    }

    const QJsonObject object = parseJsonObject(actionJson);
    const QByteArray actionTypeUtf8 = object.value(QStringLiteral("actionType")).toString().toUtf8();
    const QByteArray targetIdUtf8 = object.value(QStringLiteral("targetId")).toString().toUtf8();
    const QByteArray argumentsUtf8 = QJsonDocument(object.value(QStringLiteral("arguments")).toObject()).toJson(QJsonDocument::Compact);

    const ToolWorkerResult result = applyActionInternal(
        g_legacySession.get(),
        actionTypeUtf8.constData(),
        targetIdUtf8.constData(),
        argumentsUtf8.constData()
    );
    if (result != TOOL_WORKER_SUCCESS) {
        g_legacyLastError = g_legacySession->lastError;
    } else {
        g_legacyLastError.clear();
    }
    return legacyCurrentStateJson();
}

} // namespace FileManagerBridge

extern "C" {

TOOL_WORKER_API ToolWorkerHandle ToolWorker_Create(const char* toolId) {
    return FileManagerBridge::createWorkerHandle(toolId);
}

TOOL_WORKER_API void ToolWorker_Destroy(ToolWorkerHandle handle) {
    FileManagerBridge::destroyWorkerHandle(handle);
}

TOOL_WORKER_API ToolWorkerResult ToolWorker_Initialize(ToolWorkerHandle handle, const char* configJson) {
    return FileManagerBridge::initializeWorkerHandle(handle, configJson);
}

TOOL_WORKER_API const char* ToolWorker_HandleAction(
    ToolWorkerHandle handle,
    const char* actionType,
    const char* targetId,
    const char* argumentsJson,
    ToolWorkerResult* outResult
) {
    return FileManagerBridge::handleWorkerAction(handle, actionType, targetId, argumentsJson, outResult);
}

TOOL_WORKER_API const char* ToolWorker_GetCurrentState(ToolWorkerHandle handle, ToolWorkerResult* outResult) {
    return FileManagerBridge::getWorkerCurrentState(handle, outResult);
}

TOOL_WORKER_API const char* ToolWorker_GetInitialState(ToolWorkerHandle handle, ToolWorkerResult* outResult) {
    return FileManagerBridge::getWorkerInitialState(handle, outResult);
}

TOOL_WORKER_API const char* ToolWorker_GetLastError(ToolWorkerHandle handle) {
    return FileManagerBridge::getWorkerLastError(handle);
}

TOOL_WORKER_API void ToolWorker_FreeString(const char* value) {
    std::free(const_cast<char*>(value));
}

TOOL_WORKER_API const char* ToolWorker_GetVersion() {
    return "2.2.0";
}

TOOL_WORKER_API const char* tool_worker_get_version() {
    return ToolWorker_GetVersion();
}

TOOL_WORKER_API int tool_worker_initialize(void* runtimeContext) {
    return FileManagerBridge::initializeLegacyWorker(runtimeContext);
}

TOOL_WORKER_API void tool_worker_shutdown() {
    FileManagerBridge::shutdownLegacyWorker();
}

TOOL_WORKER_API const char* tool_worker_get_initial_state() {
    return FileManagerBridge::getLegacyInitialState();
}

TOOL_WORKER_API const char* tool_worker_handle_action(const char* actionJson) {
    return FileManagerBridge::handleLegacyAction(actionJson);
}

TOOL_WORKER_API void tool_worker_free_string(char* value) {
    std::free(value);
}

} // extern "C"
