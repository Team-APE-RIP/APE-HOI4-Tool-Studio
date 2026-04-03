#include "APEHOI4ParserBridgeTypes.h"
#include "main/Core/ParserSession.h"
#include "../../src/PluginRuntimeContext.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

#include <new>
#include <string>
#include <string_view>
#include <vector>

using APEHOI4Parser::ParseStatsRecord;
using APEHOI4Parser::ParserSession;

namespace {
std::string g_lastPluginError;
std::string g_countryTagsJsonCache = "{}";

struct CountryTagCacheRecord {
    std::string tag;
    std::string targetPath;
    uint32_t isDynamic = 0;
    APEHOI4ParserSourceRange range{};
};

std::vector<CountryTagCacheRecord> g_countryTagRecordCache;
std::vector<APEHOI4ParserTagEntry> g_countryTagEntryCache;

static ParserSession* fromHandle(APEHOI4ParserSessionHandle handle) {
    return reinterpret_cast<ParserSession*>(handle);
}

static void clearPluginError() {
    g_lastPluginError.clear();
}

static void setPluginError(const std::string& message) {
    g_lastPluginError = message;
}

static uint32_t normalizeDocumentKind(uint32_t documentKind) {
    switch (documentKind) {
    case APE_HOI4_PARSER_DOCUMENT_LOCALIZATION:
    case APE_HOI4_PARSER_DOCUMENT_TAGS:
    case APE_HOI4_PARSER_DOCUMENT_FOCUS:
        return documentKind;
    default:
        return APE_HOI4_PARSER_DOCUMENT_UNKNOWN;
    }
}

static int validateSessionHandle(APEHOI4ParserSessionHandle handle) {
    if (handle == nullptr) {
        setPluginError("Session handle is null.");
        return APE_HOI4_PARSER_STATUS_INVALID_ARGUMENT;
    }
    return APE_HOI4_PARSER_STATUS_OK;
}

static bool containsText(std::string_view text, std::string_view needle) {
    return text.find(needle) != std::string_view::npos;
}

static int mapSessionErrorToStatus(std::string_view message) {
    if (message.empty()) {
        return APE_HOI4_PARSER_STATUS_INTERNAL_ERROR;
    }

    if (containsText(message, "not found")) {
        return APE_HOI4_PARSER_STATUS_NOT_FOUND;
    }

    if (containsText(message, "must not be empty")
        || containsText(message, "Invalid ")
        || containsText(message, "null ")
        || containsText(message, "null.")
        || containsText(message, "pointer")) {
        return APE_HOI4_PARSER_STATUS_INVALID_ARGUMENT;
    }

    if (containsText(message, "Failed to open")) {
        return APE_HOI4_PARSER_STATUS_NOT_FOUND;
    }

    return APE_HOI4_PARSER_STATUS_PARSE_ERROR;
}

static int convertParseResultToStatus(const ParserSession* session, const char* fallbackMessage) {
    if (session == nullptr) {
        setPluginError("Session handle is null.");
        return APE_HOI4_PARSER_STATUS_INVALID_ARGUMENT;
    }

    const char* lastError = session->getLastError();
    if (lastError != nullptr && lastError[0] != '\0') {
        setPluginError(lastError);
        return mapSessionErrorToStatus(lastError);
    }

    setPluginError(fallbackMessage != nullptr ? fallbackMessage : "Unknown parser failure.");
    return APE_HOI4_PARSER_STATUS_INTERNAL_ERROR;
}

static void setNullHandleErrorAndReturnZero() {
    setPluginError("Session handle is null.");
}

static uint32_t toParserSourceKind(PluginRuntimeContext::EffectiveFileSource source) {
    switch (source) {
    case PluginRuntimeContext::EffectiveFileSource::Game:
        return APE_HOI4_PARSER_SOURCE_GAME;
    case PluginRuntimeContext::EffectiveFileSource::Mod:
        return APE_HOI4_PARSER_SOURCE_MOD;
    case PluginRuntimeContext::EffectiveFileSource::Dlc:
        return APE_HOI4_PARSER_SOURCE_DLC;
    default:
        return APE_HOI4_PARSER_SOURCE_UNKNOWN;
    }
}

static bool isCountryTagLogicalPath(const QString& logicalPath) {
    const QString normalizedPath = QDir::cleanPath(logicalPath).replace('\\', '/').toLower();
    return normalizedPath.startsWith("common/country_tags/")
        || normalizedPath.contains("/common/country_tags/");
}

static bool includeDynamicCountryTags(uint32_t queryFlags) {
    return (queryFlags & APE_HOI4_PARSER_COUNTRY_TAG_QUERY_INCLUDE_DYNAMIC) != 0;
}

static bool rebuildCountryTagEntryCache(uint32_t queryFlags) {
    clearPluginError();
    g_countryTagsJsonCache = "{}";
    g_countryTagRecordCache.clear();
    g_countryTagEntryCache.clear();

    const PluginRuntimeContext::EffectiveFileListResult effectiveFilesResult =
        PluginRuntimeContext::instance().listEffectiveFiles();

    if (!effectiveFilesResult.success) {
        setPluginError(effectiveFilesResult.errorMessage.toStdString());
        return false;
    }

    std::vector<QByteArray> logicalPathStorage;
    logicalPathStorage.reserve(static_cast<size_t>(effectiveFilesResult.entries.size()));

    std::vector<APEHOI4ParserFileEntry> parserFileEntries;
    parserFileEntries.reserve(static_cast<size_t>(effectiveFilesResult.entries.size()));

    std::vector<QString> tagLogicalPaths;
    tagLogicalPaths.reserve(effectiveFilesResult.entries.size());

    for (const PluginRuntimeContext::EffectiveFileEntry& entry : effectiveFilesResult.entries) {
        logicalPathStorage.push_back(entry.logicalPath.toUtf8());

        APEHOI4ParserFileEntry parserEntry{};
        parserEntry.logicalPathUtf8 = logicalPathStorage.back().constData();
        parserEntry.sourceKind = toParserSourceKind(entry.source);
        parserFileEntries.push_back(parserEntry);

        if (isCountryTagLogicalPath(entry.logicalPath)) {
            tagLogicalPaths.push_back(entry.logicalPath);
        }
    }

    ParserSession session;
    if (!session.setEffectiveFiles(
            parserFileEntries.empty() ? nullptr : parserFileEntries.data(),
            static_cast<int>(parserFileEntries.size()))) {
        setPluginError(session.getLastError());
        return false;
    }

    const bool includeDynamic = includeDynamicCountryTags(queryFlags);

    for (const QString& logicalPath : tagLogicalPaths) {
        const PluginRuntimeContext::TextReadResult readResult =
            PluginRuntimeContext::instance().readEffectiveTextFile(logicalPath);

        if (!readResult.success) {
            setPluginError(readResult.errorMessage.toStdString());
            return false;
        }

        const QByteArray logicalPathUtf8 = logicalPath.toUtf8();
        const QByteArray utf8Text = readResult.content.toUtf8();

        if (!session.parseBuffer(
                std::string_view(logicalPathUtf8.constData(), static_cast<size_t>(logicalPathUtf8.size())),
                std::string_view(utf8Text.constData(), static_cast<size_t>(utf8Text.size())),
                APE_HOI4_PARSER_DOCUMENT_TAGS)) {
            setPluginError(session.getLastError());
            return false;
        }

        const uint32_t tagCount = session.getTagEntryCount();
        if (tagCount == 0) {
            continue;
        }

        std::vector<APEHOI4ParserTagEntry> tagEntries(static_cast<size_t>(tagCount));
        const uint32_t copiedCount = session.copyTagEntries(tagEntries.data(), tagCount);

        for (uint32_t i = 0; i < copiedCount; ++i) {
            const APEHOI4ParserTagEntry& tagEntry = tagEntries[static_cast<size_t>(i)];
            if (tagEntry.tagUtf8 == nullptr || tagEntry.targetPathUtf8 == nullptr) {
                continue;
            }
            if (tagEntry.tagUtf8[0] == '\0' || tagEntry.targetPathUtf8[0] == '\0') {
                continue;
            }
            if (!includeDynamic && tagEntry.isDynamic != 0) {
                continue;
            }

            CountryTagCacheRecord record;
            record.tag = tagEntry.tagUtf8;
            record.targetPath = tagEntry.targetPathUtf8;
            record.isDynamic = tagEntry.isDynamic;
            record.range = tagEntry.range;
            g_countryTagRecordCache.push_back(std::move(record));
        }
    }

    g_countryTagEntryCache.reserve(g_countryTagRecordCache.size());
    for (CountryTagCacheRecord& record : g_countryTagRecordCache) {
        APEHOI4ParserTagEntry cachedEntry{};
        cachedEntry.tagUtf8 = record.tag.c_str();
        cachedEntry.targetPathUtf8 = record.targetPath.c_str();
        cachedEntry.isDynamic = record.isDynamic;
        cachedEntry.range = record.range;
        g_countryTagEntryCache.push_back(cachedEntry);
    }

    clearPluginError();
    return true;
}

} // namespace

APE_HOI4_PARSER_EXPORT const char* APE_HOI4Parser_GetPluginName() {
    return "APEHOI4Parser";
}

APE_HOI4_PARSER_EXPORT const char* APE_HOI4Parser_GetLastError() {
    return g_lastPluginError.c_str();
}

APE_HOI4_PARSER_EXPORT const char* APE_HOI4Parser_GetCountryTagsJson() {
    if (!rebuildCountryTagEntryCache(APE_HOI4_PARSER_COUNTRY_TAG_QUERY_INCLUDE_DYNAMIC)) {
        g_countryTagsJsonCache = "{}";
        return g_countryTagsJsonCache.c_str();
    }

    QJsonObject resultObject;
    for (const APEHOI4ParserTagEntry& tagEntry : g_countryTagEntryCache) {
        if (tagEntry.tagUtf8 == nullptr || tagEntry.targetPathUtf8 == nullptr) {
            continue;
        }

        resultObject.insert(
            QString::fromUtf8(tagEntry.tagUtf8),
            QString::fromUtf8(tagEntry.targetPathUtf8)
        );
    }

    g_countryTagsJsonCache =
        QJsonDocument(resultObject).toJson(QJsonDocument::Compact).toStdString();
    clearPluginError();
    return g_countryTagsJsonCache.c_str();
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetCountryTagEntryCount(uint32_t queryFlags) {
    if (!rebuildCountryTagEntryCache(queryFlags)) {
        return 0;
    }

    return static_cast<uint32_t>(g_countryTagEntryCache.size());
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyCountryTagEntries(
    APEHOI4ParserTagEntry* outItems,
    uint32_t capacity,
    uint32_t queryFlags
) {
    if (outItems == nullptr || capacity == 0) {
        if (outItems == nullptr && capacity != 0) {
            setPluginError("Output tag buffer is null.");
        } else {
            clearPluginError();
        }
        return 0;
    }

    if (!rebuildCountryTagEntryCache(queryFlags)) {
        return 0;
    }

    const uint32_t copyCount = std::min(
        static_cast<uint32_t>(g_countryTagEntryCache.size()),
        capacity
    );

    for (uint32_t i = 0; i < copyCount; ++i) {
        outItems[i] = g_countryTagEntryCache[static_cast<size_t>(i)];
    }

    clearPluginError();
    return copyCount;
}

APE_HOI4_PARSER_EXPORT APEHOI4ParserSessionHandle APE_HOI4Parser_CreateSession() {
    try {
        ParserSession* session = new ParserSession();
        clearPluginError();
        return reinterpret_cast<APEHOI4ParserSessionHandle>(session);
    } catch (const std::bad_alloc&) {
        setPluginError("Failed to allocate parser session.");
        return nullptr;
    } catch (...) {
        setPluginError("Unexpected exception while creating parser session.");
        return nullptr;
    }
}

APE_HOI4_PARSER_EXPORT void APE_HOI4Parser_DestroySession(APEHOI4ParserSessionHandle handle) {
    ParserSession* session = fromHandle(handle);
    delete session;
}

APE_HOI4_PARSER_EXPORT int APE_HOI4Parser_SetEffectiveFiles(
    APEHOI4ParserSessionHandle handle,
    const APEHOI4ParserFileEntry* entries,
    int count
) {
    const int sessionStatus = validateSessionHandle(handle);
    if (sessionStatus != APE_HOI4_PARSER_STATUS_OK) {
        return sessionStatus;
    }

    ParserSession* session = fromHandle(handle);
    if (!session->setEffectiveFiles(entries, count)) {
        return convertParseResultToStatus(session, "Failed to set effective files.");
    }

    clearPluginError();
    return APE_HOI4_PARSER_STATUS_OK;
}

APE_HOI4_PARSER_EXPORT int APE_HOI4Parser_SetReplacePaths(
    APEHOI4ParserSessionHandle handle,
    const APEHOI4ParserReplacePathEntry* entries,
    int count
) {
    const int sessionStatus = validateSessionHandle(handle);
    if (sessionStatus != APE_HOI4_PARSER_STATUS_OK) {
        return sessionStatus;
    }

    ParserSession* session = fromHandle(handle);
    if (!session->setReplacePaths(entries, count)) {
        return convertParseResultToStatus(session, "Failed to set replace paths.");
    }

    clearPluginError();
    return APE_HOI4_PARSER_STATUS_OK;
}

APE_HOI4_PARSER_EXPORT int APE_HOI4Parser_ParseBuffer(
    APEHOI4ParserSessionHandle handle,
    const char* logicalPathUtf8,
    const char* textUtf8,
    uint32_t documentKind
) {
    const int sessionStatus = validateSessionHandle(handle);
    if (sessionStatus != APE_HOI4_PARSER_STATUS_OK) {
        return sessionStatus;
    }

    if (logicalPathUtf8 == nullptr || textUtf8 == nullptr) {
        setPluginError("Logical path or text buffer is null.");
        return APE_HOI4_PARSER_STATUS_INVALID_ARGUMENT;
    }

    ParserSession* session = fromHandle(handle);
    if (!session->parseBuffer(logicalPathUtf8, textUtf8, normalizeDocumentKind(documentKind))) {
        return convertParseResultToStatus(session, "Failed to parse buffer.");
    }

    clearPluginError();
    return APE_HOI4_PARSER_STATUS_OK;
}

APE_HOI4_PARSER_EXPORT int APE_HOI4Parser_ParseEffectiveFile(
    APEHOI4ParserSessionHandle handle,
    const char* logicalPathUtf8,
    uint32_t documentKind
) {
    const int sessionStatus = validateSessionHandle(handle);
    if (sessionStatus != APE_HOI4_PARSER_STATUS_OK) {
        return sessionStatus;
    }

    if (logicalPathUtf8 == nullptr) {
        setPluginError("Logical path is null.");
        return APE_HOI4_PARSER_STATUS_INVALID_ARGUMENT;
    }

    const PluginRuntimeContext::TextReadResult readResult =
        PluginRuntimeContext::instance().readEffectiveTextFile(QString::fromUtf8(logicalPathUtf8));

    if (!readResult.success) {
        setPluginError(readResult.errorMessage.toStdString());
        return mapSessionErrorToStatus(g_lastPluginError);
    }

    const QByteArray utf8Text = readResult.content.toUtf8();

    ParserSession* session = fromHandle(handle);
    if (!session->parseBuffer(
            std::string_view(logicalPathUtf8),
            std::string_view(utf8Text.constData(), static_cast<size_t>(utf8Text.size())),
            normalizeDocumentKind(documentKind))) {
        return convertParseResultToStatus(session, "Failed to parse effective file.");
    }

    clearPluginError();
    return APE_HOI4_PARSER_STATUS_OK;
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetDiagnosticCount(
    APEHOI4ParserSessionHandle handle
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setNullHandleErrorAndReturnZero();
        return 0;
    }

    clearPluginError();
    return session->getDiagnosticCount();
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyDiagnostics(
    APEHOI4ParserSessionHandle handle,
    APEHOI4ParserDiagnostic* outItems,
    uint32_t capacity
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setNullHandleErrorAndReturnZero();
        return 0;
    }

    clearPluginError();
    return session->copyDiagnostics(outItems, capacity);
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetLocalizationEntryCount(
    APEHOI4ParserSessionHandle handle
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setNullHandleErrorAndReturnZero();
        return 0;
    }

    clearPluginError();
    return session->getLocalizationEntryCount();
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyLocalizationEntries(
    APEHOI4ParserSessionHandle handle,
    APEHOI4ParserLocalizationEntry* outItems,
    uint32_t capacity
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setNullHandleErrorAndReturnZero();
        return 0;
    }

    clearPluginError();
    return session->copyLocalizationEntries(outItems, capacity);
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetTagEntryCount(
    APEHOI4ParserSessionHandle handle
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setNullHandleErrorAndReturnZero();
        return 0;
    }

    clearPluginError();
    return session->getTagEntryCount();
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyTagEntries(
    APEHOI4ParserSessionHandle handle,
    APEHOI4ParserTagEntry* outItems,
    uint32_t capacity
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setNullHandleErrorAndReturnZero();
        return 0;
    }

    clearPluginError();
    return session->copyTagEntries(outItems, capacity);
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetFocusEntryCount(
    APEHOI4ParserSessionHandle handle
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setNullHandleErrorAndReturnZero();
        return 0;
    }

    clearPluginError();
    return session->getFocusEntryCount();
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyFocusEntries(
    APEHOI4ParserSessionHandle handle,
    APEHOI4ParserFocusEntry* outItems,
    uint32_t capacity
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setNullHandleErrorAndReturnZero();
        return 0;
    }

    clearPluginError();
    return session->copyFocusEntries(outItems, capacity);
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetIdeaEntryCount(
    APEHOI4ParserSessionHandle handle
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setNullHandleErrorAndReturnZero();
        return 0;
    }

    clearPluginError();
    return session->getIdeaEntryCount();
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyIdeaEntries(
    APEHOI4ParserSessionHandle handle,
    APEHOI4ParserIdeaEntry* outItems,
    uint32_t capacity
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setNullHandleErrorAndReturnZero();
        return 0;
    }

    clearPluginError();
    return session->copyIdeaEntries(outItems, capacity);
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetScriptedTriggerEntryCount(
    APEHOI4ParserSessionHandle handle
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setNullHandleErrorAndReturnZero();
        return 0;
    }

    clearPluginError();
    return session->getScriptedTriggerEntryCount();
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyScriptedTriggerEntries(
    APEHOI4ParserSessionHandle handle,
    APEHOI4ParserScriptedTriggerEntry* outItems,
    uint32_t capacity
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setNullHandleErrorAndReturnZero();
        return 0;
    }

    clearPluginError();
    return session->copyScriptedTriggerEntries(outItems, capacity);
}

APE_HOI4_PARSER_EXPORT int APE_HOI4Parser_GetParseStats(
    APEHOI4ParserSessionHandle handle,
    APEHOI4ParserParseStats* outStats
) {
    const int sessionStatus = validateSessionHandle(handle);
    if (sessionStatus != APE_HOI4_PARSER_STATUS_OK) {
        return sessionStatus;
    }

    if (outStats == nullptr) {
        setPluginError("Output stats pointer is null.");
        return APE_HOI4_PARSER_STATUS_INVALID_ARGUMENT;
    }

    ParserSession* session = fromHandle(handle);
    const ParseStatsRecord stats = session->getParseStats();
    outStats->tokenCount = stats.tokenCount;
    outStats->nodeCount = stats.nodeCount;
    outStats->diagnosticCount = stats.diagnosticCount;
    outStats->documentKind = stats.documentKind;
    clearPluginError();
    return APE_HOI4_PARSER_STATUS_OK;
}

APE_HOI4_PARSER_EXPORT const char* APE_HOI4Parser_GetDebugSyntaxTreeJson(
    APEHOI4ParserSessionHandle handle
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setPluginError("Session handle is null.");
        return "";
    }

    clearPluginError();
    return session->getDebugSyntaxTreeJson();
}

APE_HOI4_PARSER_EXPORT const char* APE_HOI4Parser_GetDebugDiagnosticsJson(
    APEHOI4ParserSessionHandle handle
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setPluginError("Session handle is null.");
        return "";
    }

    clearPluginError();
    return session->getDebugDiagnosticsJson();
}