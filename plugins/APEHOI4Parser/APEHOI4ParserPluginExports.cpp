#include "APEHOI4ParserBridgeTypes.h"
#include "main/Core/ParserSession.h"
#include "../../src/PluginRuntimeContext.h"
#include "../../src/PluginAbi.h"

#include <QDir>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstdlib>
#include <cstdint>
#include <cstring>
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

struct FontCacheRecord {
    std::string name;
    std::string path;
    std::string color;
    std::string fontFiles;
    std::string languages;
    std::string textColors;
    APEHOI4ParserSourceRange nameRange{};
};

std::vector<FontCacheRecord> g_fontRecordCache;
std::vector<APEHOI4ParserFontEntry> g_fontEntryCache;
QString g_fontEntryCacheSignature;
std::string g_fontGlobalTextColors;
bool g_fontEntryCacheValid = false;

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
    case APE_HOI4_PARSER_DOCUMENT_FONT_GFX:
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

static QString normalizedLogicalPathKey(const QString& logicalPath) {
    return QDir::cleanPath(logicalPath).replace('\\', '/').toLower();
}

static bool isFontGfxLogicalPath(const QString& logicalPath) {
    const QString normalizedPath = normalizedLogicalPathKey(logicalPath);
    return (normalizedPath.startsWith("interface/") || normalizedPath.contains("/interface/"))
        && normalizedPath.endsWith(".gfx");
}

static bool includeDynamicCountryTags(uint32_t queryFlags) {
    return (queryFlags & APE_HOI4_PARSER_COUNTRY_TAG_QUERY_INCLUDE_DYNAMIC) != 0;
}

static bool isMissingEffectiveFileError(const QString& message) {
    return message.contains(QStringLiteral("Effective file does not exist"), Qt::CaseInsensitive)
        || message.contains(QStringLiteral("Effective file no longer exists"), Qt::CaseInsensitive)
        || message.contains(QStringLiteral("Failed to open effective file for reading"), Qt::CaseInsensitive);
}

static bool containsBitmapFontRegistration(const QString& content) {
    return content.contains(QStringLiteral("bitmapfont"), Qt::CaseInsensitive);
}

static QString buildFontEntryCacheSignature(const QList<PluginRuntimeContext::EffectiveFileEntry>& entries) {
    QString signature;
    for (const PluginRuntimeContext::EffectiveFileEntry& entry : entries) {
        if (!isFontGfxLogicalPath(entry.logicalPath)) {
            continue;
        }
        signature += QString::number(static_cast<int>(entry.source));
        signature += QLatin1Char('\t');
        signature += normalizedLogicalPathKey(entry.logicalPath);
        signature += QLatin1Char('\t');
        signature += QString::number(entry.lastModifiedMs);
        signature += QLatin1Char('\n');
    }
    return signature;
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
            if (isMissingEffectiveFileError(readResult.errorMessage)) {
                continue;
            }
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

static bool rebuildFontEntryCache(uint32_t queryFlags) {
    clearPluginError();

    const PluginRuntimeContext::EffectiveFileListResult effectiveFilesResult =
        PluginRuntimeContext::instance().listEffectiveFiles(QStringLiteral("interface"), QStringLiteral(".gfx"));

    if (!effectiveFilesResult.success) {
        setPluginError(effectiveFilesResult.errorMessage.toStdString());
        return false;
    }

    const QString signature = buildFontEntryCacheSignature(effectiveFilesResult.entries);
    const bool forceRefresh = (queryFlags & APE_HOI4_PARSER_FONT_QUERY_FORCE_REFRESH) != 0;
    if (g_fontEntryCacheValid && !forceRefresh && signature == g_fontEntryCacheSignature) {
        clearPluginError();
        return true;
    }

    g_fontRecordCache.clear();
    g_fontEntryCache.clear();
    g_fontGlobalTextColors.clear();
    g_fontEntryCacheSignature = signature;
    g_fontEntryCacheValid = false;

    const PluginRuntimeContext::MatchingTextFilesResult textFilesResult =
        PluginRuntimeContext::instance().readEffectiveTextFiles(QStringLiteral("interface"), QStringLiteral(".gfx"));
    if (!textFilesResult.success) {
        setPluginError(textFilesResult.errorMessage.toStdString());
        return false;
    }

    QHash<QString, QString> contentByLogicalPath;
    contentByLogicalPath.reserve(textFilesResult.entries.size());
    for (const PluginRuntimeContext::TextFileMatchEntry& textEntry : textFilesResult.entries) {
        contentByLogicalPath.insert(normalizedLogicalPathKey(textEntry.relativePath), textEntry.content);
    }

    QString combinedFontGfxText;
    for (const PluginRuntimeContext::EffectiveFileEntry& entry : effectiveFilesResult.entries) {
        if (!isFontGfxLogicalPath(entry.logicalPath)) {
            continue;
        }

        const auto contentIt = contentByLogicalPath.constFind(normalizedLogicalPathKey(entry.logicalPath));
        if (contentIt == contentByLogicalPath.constEnd()) {
            continue;
        }

        if (!containsBitmapFontRegistration(contentIt.value())) {
            continue;
        }

        combinedFontGfxText += QStringLiteral("\n# ");
        combinedFontGfxText += entry.logicalPath;
        combinedFontGfxText += QLatin1Char('\n');
        combinedFontGfxText += contentIt.value();
        combinedFontGfxText += QLatin1Char('\n');
    }

    if (!combinedFontGfxText.isEmpty()) {
        ParserSession session;
        const QByteArray logicalPathUtf8 = QByteArrayLiteral("interface/__combined_font_cache__.gfx");
        const QByteArray utf8Text = combinedFontGfxText.toUtf8();
        if (!session.parseBuffer(
                std::string_view(logicalPathUtf8.constData(), static_cast<size_t>(logicalPathUtf8.size())),
                std::string_view(utf8Text.constData(), static_cast<size_t>(utf8Text.size())),
                APE_HOI4_PARSER_DOCUMENT_FONT_GFX)) {
            setPluginError(session.getLastError());
            return false;
        }

        g_fontGlobalTextColors = session.getFontGlobalTextColorsUtf8();
        const uint32_t fontCount = session.getFontEntryCount();
        if (fontCount > 0) {
            std::vector<APEHOI4ParserFontEntry> fontEntries(static_cast<size_t>(fontCount));
            const uint32_t copiedCount = session.copyFontEntries(fontEntries.data(), fontCount);
            for (uint32_t i = 0; i < copiedCount; ++i) {
                const APEHOI4ParserFontEntry& fontEntry = fontEntries[static_cast<size_t>(i)];
                if (fontEntry.nameUtf8 == nullptr || fontEntry.nameUtf8[0] == '\0') {
                    continue;
                }

                FontCacheRecord record;
                record.name = fontEntry.nameUtf8;
                record.path = fontEntry.pathUtf8 != nullptr ? fontEntry.pathUtf8 : "";
                record.color = fontEntry.colorUtf8 != nullptr ? fontEntry.colorUtf8 : "";
                record.fontFiles = fontEntry.fontFilesUtf8 != nullptr ? fontEntry.fontFilesUtf8 : "";
                record.languages = fontEntry.languagesUtf8 != nullptr ? fontEntry.languagesUtf8 : "";
                record.textColors = fontEntry.textColorsUtf8 != nullptr ? fontEntry.textColorsUtf8 : "";
                record.nameRange = fontEntry.nameRange;
                g_fontRecordCache.push_back(std::move(record));
            }
        }
    }

    g_fontEntryCache.reserve(g_fontRecordCache.size());
    for (FontCacheRecord& record : g_fontRecordCache) {
        APEHOI4ParserFontEntry cachedEntry{};
        cachedEntry.nameUtf8 = record.name.c_str();
        cachedEntry.pathUtf8 = record.path.c_str();
        cachedEntry.colorUtf8 = record.color.c_str();
        cachedEntry.fontFilesUtf8 = record.fontFiles.c_str();
        cachedEntry.languagesUtf8 = record.languages.c_str();
        cachedEntry.textColorsUtf8 = record.textColors.c_str();
        cachedEntry.nameRange = record.nameRange;
        g_fontEntryCache.push_back(cachedEntry);
    }

    g_fontEntryCacheValid = true;
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

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetFontEntryCount(uint32_t queryFlags) {
    if (!rebuildFontEntryCache(queryFlags)) {
        return 0;
    }

    return static_cast<uint32_t>(g_fontEntryCache.size());
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyFontEntries(
    APEHOI4ParserFontEntry* outItems,
    uint32_t capacity,
    uint32_t queryFlags
) {
    if (outItems == nullptr || capacity == 0) {
        if (outItems == nullptr && capacity != 0) {
            setPluginError("Output font buffer is null.");
        } else {
            clearPluginError();
        }
        return 0;
    }

    if (!rebuildFontEntryCache(queryFlags)) {
        return 0;
    }

    const uint32_t copyCount = std::min(
        static_cast<uint32_t>(g_fontEntryCache.size()),
        capacity
    );

    for (uint32_t i = 0; i < copyCount; ++i) {
        outItems[i] = g_fontEntryCache[static_cast<size_t>(i)];
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

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetFontEntryCountForSession(
    APEHOI4ParserSessionHandle handle
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setNullHandleErrorAndReturnZero();
        return 0;
    }

    clearPluginError();
    return session->getFontEntryCount();
}

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyFontEntriesForSession(
    APEHOI4ParserSessionHandle handle,
    APEHOI4ParserFontEntry* outItems,
    uint32_t capacity
) {
    ParserSession* session = fromHandle(handle);
    if (session == nullptr) {
        setNullHandleErrorAndReturnZero();
        return 0;
    }

    clearPluginError();
    return session->copyFontEntries(outItems, capacity);
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

namespace {

void clearAbiResponse(ApePluginAbiResponse* response) {
    if (!response) {
        return;
    }
    response->abiVersion = APE_PLUGIN_ABI_VERSION;
    response->status = APE_PLUGIN_ABI_STATUS_INTERNAL_ERROR;
    response->contentType = APE_PLUGIN_ABI_CONTENT_NONE;
    response->flags = 0;
    response->payload = nullptr;
    response->payloadSize = 0;
    response->errorUtf8 = nullptr;
}

char* allocateAbiString(const QByteArray& text) {
    char* data = static_cast<char*>(std::malloc(static_cast<std::size_t>(text.size() + 1)));
    if (!data) {
        return nullptr;
    }
    std::memcpy(data, text.constData(), static_cast<std::size_t>(text.size()));
    data[text.size()] = '\0';
    return data;
}

bool setAbiPayload(ApePluginAbiResponse* response, const QByteArray& payload) {
    if (!response) {
        return false;
    }
    response->contentType = APE_PLUGIN_ABI_CONTENT_JSON_UTF8;
    response->payloadSize = static_cast<std::uint64_t>(payload.size());
    if (payload.isEmpty()) {
        response->payload = nullptr;
        return true;
    }
    auto* bytes = static_cast<std::uint8_t*>(std::malloc(static_cast<std::size_t>(payload.size())));
    if (!bytes) {
        return false;
    }
    std::memcpy(bytes, payload.constData(), static_cast<std::size_t>(payload.size()));
    response->payload = bytes;
    return true;
}

void setAbiError(ApePluginAbiResponse* response, std::uint32_t status, const QString& message) {
    if (!response) {
        return;
    }
    response->status = status;
    response->errorUtf8 = allocateAbiString(message.toUtf8());
}

int setJsonResponse(ApePluginAbiResponse* response, const QJsonObject& object) {
    if (!setAbiPayload(response, QJsonDocument(object).toJson(QJsonDocument::Compact))) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INTERNAL_ERROR, QStringLiteral("Failed to allocate parser response."));
        return 1;
    }
    response->status = APE_PLUGIN_ABI_STATUS_OK;
    return 0;
}

int invokeParserListCountryTags(ApePluginAbiResponse* response) {
    const std::uint32_t count = APE_HOI4Parser_GetCountryTagEntryCount(APE_HOI4_PARSER_COUNTRY_TAG_QUERY_INCLUDE_DYNAMIC);
    std::vector<APEHOI4ParserTagEntry> entries(static_cast<std::size_t>(count));
    const std::uint32_t copied = count == 0
        ? 0
        : APE_HOI4Parser_CopyCountryTagEntries(entries.data(), count, APE_HOI4_PARSER_COUNTRY_TAG_QUERY_INCLUDE_DYNAMIC);

    QJsonArray items;
    for (std::uint32_t i = 0; i < copied; ++i) {
        const APEHOI4ParserTagEntry& entry = entries[static_cast<std::size_t>(i)];
        QJsonObject object;
        object[QStringLiteral("tag")] = QString::fromUtf8(entry.tagUtf8 ? entry.tagUtf8 : "");
        object[QStringLiteral("targetPath")] = QString::fromUtf8(entry.targetPathUtf8 ? entry.targetPathUtf8 : "");
        object[QStringLiteral("isDynamic")] = entry.isDynamic != 0;
        items.append(object);
    }

    QJsonObject root;
    root[QStringLiteral("entries")] = items;
    return setJsonResponse(response, root);
}

int invokeParserListFonts(ApePluginAbiResponse* response) {
    const std::uint32_t count = APE_HOI4Parser_GetFontEntryCount(APE_HOI4_PARSER_FONT_QUERY_FORCE_REFRESH);
    std::vector<APEHOI4ParserFontEntry> entries(static_cast<std::size_t>(count));
    const std::uint32_t copied = count == 0
        ? 0
        : APE_HOI4Parser_CopyFontEntries(entries.data(), count, APE_HOI4_PARSER_FONT_QUERY_FORCE_REFRESH);

    QJsonArray items;
    for (std::uint32_t i = 0; i < copied; ++i) {
        const APEHOI4ParserFontEntry& entry = entries[static_cast<std::size_t>(i)];
        QJsonObject object;
        object[QStringLiteral("name")] = QString::fromUtf8(entry.nameUtf8 ? entry.nameUtf8 : "");
        object[QStringLiteral("path")] = QString::fromUtf8(entry.pathUtf8 ? entry.pathUtf8 : "");
        object[QStringLiteral("color")] = QString::fromUtf8(entry.colorUtf8 ? entry.colorUtf8 : "");
        object[QStringLiteral("fontFiles")] = QString::fromUtf8(entry.fontFilesUtf8 ? entry.fontFilesUtf8 : "");
        object[QStringLiteral("languages")] = QString::fromUtf8(entry.languagesUtf8 ? entry.languagesUtf8 : "");
        object[QStringLiteral("textColors")] = QString::fromUtf8(entry.textColorsUtf8 ? entry.textColorsUtf8 : "");
        items.append(object);
    }

    QJsonObject root;
    root[QStringLiteral("entries")] = items;
    root[QStringLiteral("globalTextColors")] = QString::fromUtf8(g_fontGlobalTextColors.c_str());
    return setJsonResponse(response, root);
}

} // namespace

APE_PLUGIN_ABI_EXPORT const char* APE_Plugin_GetName(void) {
    return "APEHOI4Parser";
}

APE_PLUGIN_ABI_EXPORT std::uint32_t APE_Plugin_GetAbiVersion(void) {
    return APE_PLUGIN_ABI_VERSION;
}

APE_PLUGIN_ABI_EXPORT void APE_Plugin_FreeResponse(ApePluginAbiResponse* response) {
    if (!response) {
        return;
    }
    std::free(response->payload);
    std::free(response->errorUtf8);
    clearAbiResponse(response);
}

APE_PLUGIN_ABI_EXPORT int APE_Plugin_Invoke(const ApePluginAbiRequest* request, ApePluginAbiResponse* response) {
    clearAbiResponse(response);
    if (!request || !response || !request->operationUtf8) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INVALID_ARGUMENT, QStringLiteral("Invalid parser ABI request."));
        return 1;
    }
    if (request->abiVersion != APE_PLUGIN_ABI_VERSION) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_UNSUPPORTED_ABI, QStringLiteral("Unsupported parser ABI version."));
        return 1;
    }

    const QString operation = QString::fromUtf8(request->operationUtf8);
    if (operation == QStringLiteral("hoi4Parser.listCountryTags")) {
        return invokeParserListCountryTags(response);
    }
    if (operation == QStringLiteral("hoi4Parser.listFonts")) {
        return invokeParserListFonts(response);
    }

    setAbiError(response, APE_PLUGIN_ABI_STATUS_UNSUPPORTED_OPERATION, QStringLiteral("Unsupported parser operation."));
    return 1;
}
