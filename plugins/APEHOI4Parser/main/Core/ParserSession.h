#ifndef APE_HOI4_PARSER_CORE_PARSER_SESSION_H
#define APE_HOI4_PARSER_CORE_PARSER_SESSION_H

#include "../../APEHOI4ParserBridgeTypes.h"

#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace APEHOI4Parser {

std::string buildDebugSyntaxTreeJson(const class ParserSession& session);
std::string buildDebugDiagnosticsJson(const class ParserSession& session);

struct EffectiveFileRecord {
    std::string logicalPath;
    uint32_t sourceKind = APE_HOI4_PARSER_SOURCE_UNKNOWN;
};

struct ReplacePathRecord {
    std::string path;
};

struct DiagnosticRecord {
    uint32_t severity = APE_HOI4_PARSER_DIAGNOSTIC_ERROR;
    uint32_t code = 0;
    APEHOI4ParserSourceRange range{};
    std::string message;
};

struct LocalizationRecord {
    std::string key;
    std::string value;
    APEHOI4ParserSourceRange keyRange{};
    APEHOI4ParserSourceRange valueRange{};
};

struct TagRecord {
    std::string tag;
    std::string targetPath;
    bool isDynamic = false;
    APEHOI4ParserSourceRange range{};
};

struct FocusRecord {
    std::string id;
    std::string icon;
    std::string x;
    std::string y;
    APEHOI4ParserSourceRange idRange{};
};

struct IdeaRecord {
    std::string id;
    std::string category;
    APEHOI4ParserSourceRange idRange{};
};

struct ScriptedTriggerRecord {
    std::string id;
    APEHOI4ParserSourceRange idRange{};
};

struct ScriptedEffectRecord {
    std::string id;
    APEHOI4ParserSourceRange idRange{};
};

struct ParseStatsRecord {
    uint32_t tokenCount = 0;
    uint32_t nodeCount = 0;
    uint32_t diagnosticCount = 0;
    uint32_t documentKind = APE_HOI4_PARSER_DOCUMENT_UNKNOWN;
};

class ParserSession {
    friend std::string buildDebugSyntaxTreeJson(const ParserSession& session);
    friend std::string buildDebugDiagnosticsJson(const ParserSession& session);

public:
    ParserSession();
    ~ParserSession();

    ParserSession(const ParserSession&) = delete;
    ParserSession& operator=(const ParserSession&) = delete;

    bool setEffectiveFiles(const APEHOI4ParserFileEntry* entries, int count);
    bool setReplacePaths(const APEHOI4ParserReplacePathEntry* entries, int count);

    bool parseBuffer(
        std::string_view logicalPathUtf8,
        std::string_view textUtf8,
        uint32_t documentKind
    );

    bool parseEffectiveFile(
        std::string_view logicalPathUtf8,
        uint32_t documentKind
    );

    const char* getLastError() const;

    uint32_t getDiagnosticCount() const;
    uint32_t copyDiagnostics(APEHOI4ParserDiagnostic* outItems, uint32_t capacity) const;

    uint32_t getLocalizationEntryCount() const;
    uint32_t copyLocalizationEntries(APEHOI4ParserLocalizationEntry* outItems, uint32_t capacity) const;

    uint32_t getTagEntryCount() const;
    uint32_t copyTagEntries(APEHOI4ParserTagEntry* outItems, uint32_t capacity) const;

    uint32_t getFocusEntryCount() const;
    uint32_t copyFocusEntries(APEHOI4ParserFocusEntry* outItems, uint32_t capacity) const;

    uint32_t getIdeaEntryCount() const;
    uint32_t copyIdeaEntries(APEHOI4ParserIdeaEntry* outItems, uint32_t capacity) const;

    uint32_t getScriptedTriggerEntryCount() const;
    uint32_t copyScriptedTriggerEntries(APEHOI4ParserScriptedTriggerEntry* outItems, uint32_t capacity) const;

    ParseStatsRecord getParseStats() const;

    const char* getDebugSyntaxTreeJson();
    const char* getDebugDiagnosticsJson();

private:
    void clearTransientState();
    void clearDomainState();

    void setLastError(std::string message);

    void buildDebugSyntaxTreeJson();
    void buildDebugDiagnosticsJson();

    static uint32_t clampCountToCapacity(uint32_t count, uint32_t capacity);

private:
    std::pmr::monotonic_buffer_resource m_sessionMemory;
    std::unordered_map<std::string, EffectiveFileRecord> m_effectiveFiles;
    std::vector<ReplacePathRecord> m_replacePaths;

    std::string m_lastLogicalPath;
    std::string m_lastSourceText;

    std::vector<DiagnosticRecord> m_diagnostics;
    std::vector<LocalizationRecord> m_localizationEntries;
    std::vector<TagRecord> m_tagEntries;
    std::vector<FocusRecord> m_focusEntries;
    std::vector<IdeaRecord> m_ideaEntries;
    std::vector<ScriptedTriggerRecord> m_scriptedTriggerEntries;
    std::vector<ScriptedEffectRecord> m_scriptedEffectEntries;
    ParseStatsRecord m_parseStats{};

    std::string m_lastError;
    std::string m_debugSyntaxTreeJson;
    std::string m_debugDiagnosticsJson;
};

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_CORE_PARSER_SESSION_H