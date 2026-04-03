#include "ParserSession.h"

#include "../Domain/Focus/FocusTreeParser.h"
#include "../Domain/Ideas/IdeasParser.h"
#include "../Domain/Localization/LocalizationParser.h"
#include "../Domain/ScriptedEffects/ScriptedEffectParser.h"
#include "../Domain/ScriptedTriggers/ScriptedTriggerParser.h"
#include "../Domain/Tags/TagFileParser.h"
#include "../Lexer/Lexer.h"
#include "../Parser/Parser.h"
#include "../Queries/ParseResultJson.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace APEHOI4Parser {
namespace {

static std::string_view trimView(std::string_view value) {
    size_t begin = 0;
    size_t end = value.size();

    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }

    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return value.substr(begin, end - begin);
}

static std::string_view removeComment(std::string_view line) {
    const size_t hashPos = line.find('#');
    if (hashPos == std::string_view::npos) {
        return line;
    }
    return line.substr(0, hashPos);
}

static std::string toLowerAscii(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());

    for (const char ch : value) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    return lowered;
}

static std::string normalizePath(std::string_view value) {
    std::string normalized = toLowerAscii(value);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

static bool containsPathPart(const std::string& normalizedPath, const char* fragment) {
    return normalizedPath.find(fragment) != std::string::npos;
}

static uint32_t inferDocumentKindFromPath(std::string_view logicalPathUtf8, uint32_t requestedDocumentKind) {
    if (requestedDocumentKind != APE_HOI4_PARSER_DOCUMENT_UNKNOWN) {
        return requestedDocumentKind;
    }

    const std::string normalizedPath = normalizePath(logicalPathUtf8);

    if (containsPathPart(normalizedPath, "/localisation/")
        || containsPathPart(normalizedPath, "/localization/")
        || containsPathPart(normalizedPath, ".yml")
        || containsPathPart(normalizedPath, ".yaml")) {
        return APE_HOI4_PARSER_DOCUMENT_LOCALIZATION;
    }

    if (containsPathPart(normalizedPath, "/common/country_tags/")) {
        return APE_HOI4_PARSER_DOCUMENT_TAGS;
    }

    if (containsPathPart(normalizedPath, "/common/national_focus/")) {
        return APE_HOI4_PARSER_DOCUMENT_FOCUS;
    }

    if (containsPathPart(normalizedPath, "/common/ideas/")
        || containsPathPart(normalizedPath, "/common/scripted_triggers/")
        || containsPathPart(normalizedPath, "/common/scripted_effects/")) {
        return APE_HOI4_PARSER_DOCUMENT_UNKNOWN;
    }

    return APE_HOI4_PARSER_DOCUMENT_UNKNOWN;
}

static APEHOI4ParserSourceRange makeSingleLineRange(
    uint32_t lineIndex,
    uint32_t lineStartOffset,
    uint32_t startColumn,
    uint32_t endColumn
) {
    APEHOI4ParserSourceRange range{};
    range.startLine = lineIndex;
    range.endLine = lineIndex;
    range.startColumn = startColumn;
    range.endColumn = endColumn;
    range.startOffset = lineStartOffset + startColumn;
    range.endOffset = lineStartOffset + endColumn;
    return range;
}

static DiagnosticRecord makeUnsupportedDocumentDiagnostic(std::string_view logicalPathUtf8) {
    DiagnosticRecord diagnostic;
    diagnostic.severity = APE_HOI4_PARSER_DIAGNOSTIC_WARNING;
    diagnostic.code = 9001;
    diagnostic.message = "Document kind is unknown or not yet supported for path: ";
    diagnostic.message.append(logicalPathUtf8.begin(), logicalPathUtf8.end());
    return diagnostic;
}

static void appendLocalizationDiagnostics(std::string_view text, std::vector<DiagnosticRecord>& diagnostics) {
    size_t lineStart = 0;
    uint32_t lineIndex = 0;

    while (lineStart <= text.size()) {
        size_t lineEnd = text.find('\n', lineStart);
        if (lineEnd == std::string_view::npos) {
            lineEnd = text.size();
        }

        std::string_view line = text.substr(lineStart, lineEnd - lineStart);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        const std::string_view uncommented = removeComment(line);
        const std::string_view trimmed = trimView(uncommented);

        if (!trimmed.empty()) {
            const size_t firstNonSpace = static_cast<size_t>(trimmed.data() - line.data());

            if (trimmed.front() != 'l' || trimmed.find(':') == std::string_view::npos || trimmed.find(' ') != std::string_view::npos) {
                if (trimmed.find(':') == std::string_view::npos) {
                    DiagnosticRecord diagnostic;
                    diagnostic.severity = APE_HOI4_PARSER_DIAGNOSTIC_WARNING;
                    diagnostic.code = 1001;
                    diagnostic.range = makeSingleLineRange(
                        lineIndex,
                        static_cast<uint32_t>(lineStart),
                        static_cast<uint32_t>(firstNonSpace),
                        static_cast<uint32_t>(firstNonSpace + trimmed.size())
                    );
                    diagnostic.message = "Localization line does not contain a ':' separator.";
                    diagnostics.push_back(std::move(diagnostic));
                } else {
                    const size_t colonPos = trimmed.find(':');
                    if (colonPos == trimmed.size() - 1) {
                        DiagnosticRecord diagnostic;
                        diagnostic.severity = APE_HOI4_PARSER_DIAGNOSTIC_WARNING;
                        diagnostic.code = 1002;
                        diagnostic.range = makeSingleLineRange(
                            lineIndex,
                            static_cast<uint32_t>(lineStart),
                            static_cast<uint32_t>(firstNonSpace),
                            static_cast<uint32_t>(firstNonSpace + trimmed.size())
                        );
                        diagnostic.message = "Localization line has a key separator but no value payload.";
                        diagnostics.push_back(std::move(diagnostic));
                    }
                }
            } else {
                const std::string loweredText = toLowerAscii(trimmed);
                const bool looksLikeLanguageHeader = loweredText.rfind("l_", 0) == 0 && trimmed.back() == ':';
                if (!looksLikeLanguageHeader && trimmed.find(':') == std::string_view::npos) {
                    DiagnosticRecord diagnostic;
                    diagnostic.severity = APE_HOI4_PARSER_DIAGNOSTIC_WARNING;
                    diagnostic.code = 1001;
                    diagnostic.range = makeSingleLineRange(
                        lineIndex,
                        static_cast<uint32_t>(lineStart),
                        static_cast<uint32_t>(firstNonSpace),
                        static_cast<uint32_t>(firstNonSpace + trimmed.size())
                    );
                    diagnostic.message = "Localization line does not contain a ':' separator.";
                    diagnostics.push_back(std::move(diagnostic));
                }
            }
        }

        if (lineEnd == text.size()) {
            break;
        }

        lineStart = lineEnd + 1;
        ++lineIndex;
    }
}

static void appendTagDiagnostics(std::string_view text, std::vector<DiagnosticRecord>& diagnostics) {
    size_t lineStart = 0;
    uint32_t lineIndex = 0;

    while (lineStart <= text.size()) {
        size_t lineEnd = text.find('\n', lineStart);
        if (lineEnd == std::string_view::npos) {
            lineEnd = text.size();
        }

        std::string_view line = text.substr(lineStart, lineEnd - lineStart);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        const std::string_view uncommented = removeComment(line);
        const std::string_view trimmed = trimView(uncommented);

        if (!trimmed.empty()) {
            const size_t firstNonSpace = static_cast<size_t>(trimmed.data() - line.data());
            const size_t equalsPos = trimmed.find('=');

            if (equalsPos == std::string_view::npos) {
                DiagnosticRecord diagnostic;
                diagnostic.severity = APE_HOI4_PARSER_DIAGNOSTIC_WARNING;
                diagnostic.code = 2001;
                diagnostic.range = makeSingleLineRange(
                    lineIndex,
                    static_cast<uint32_t>(lineStart),
                    static_cast<uint32_t>(firstNonSpace),
                    static_cast<uint32_t>(firstNonSpace + trimmed.size())
                );
                diagnostic.message = "Tag line does not contain '='.";
                diagnostics.push_back(std::move(diagnostic));
            } else {
                const std::string_view leftPart = trimView(trimmed.substr(0, equalsPos));
                const std::string_view rightPart = trimView(trimmed.substr(equalsPos + 1));

                if (leftPart.empty() || rightPart.empty()) {
                    DiagnosticRecord diagnostic;
                    diagnostic.severity = APE_HOI4_PARSER_DIAGNOSTIC_WARNING;
                    diagnostic.code = 2002;
                    diagnostic.range = makeSingleLineRange(
                        lineIndex,
                        static_cast<uint32_t>(lineStart),
                        static_cast<uint32_t>(firstNonSpace),
                        static_cast<uint32_t>(firstNonSpace + trimmed.size())
                    );
                    diagnostic.message = "Tag assignment must contain both tag name and target path.";
                    diagnostics.push_back(std::move(diagnostic));
                }
            }
        }

        if (lineEnd == text.size()) {
            break;
        }

        lineStart = lineEnd + 1;
        ++lineIndex;
    }
}

static void appendDuplicateLocalizationDiagnostics(
    const std::vector<LocalizationRecord>& entries,
    std::vector<DiagnosticRecord>& diagnostics
) {
    std::unordered_map<std::string, const LocalizationRecord*> firstEntries;
    firstEntries.reserve(entries.size());

    for (const LocalizationRecord& entry : entries) {
        if (entry.key.empty()) {
            continue;
        }

        const auto it = firstEntries.find(entry.key);
        if (it == firstEntries.end()) {
            firstEntries.emplace(entry.key, &entry);
            continue;
        }

        DiagnosticRecord diagnostic;
        diagnostic.severity = APE_HOI4_PARSER_DIAGNOSTIC_WARNING;
        diagnostic.code = 1101;
        diagnostic.range = entry.keyRange;
        diagnostic.message = "Duplicate localization key detected: ";
        diagnostic.message += entry.key;
        diagnostics.push_back(std::move(diagnostic));
    }
}

static void appendDuplicateTagDiagnostics(
    const std::vector<TagRecord>& entries,
    std::vector<DiagnosticRecord>& diagnostics
) {
    std::unordered_map<std::string, const TagRecord*> firstEntries;
    firstEntries.reserve(entries.size());

    for (const TagRecord& entry : entries) {
        if (entry.tag.empty()) {
            continue;
        }

        const auto it = firstEntries.find(entry.tag);
        if (it == firstEntries.end()) {
            firstEntries.emplace(entry.tag, &entry);
            continue;
        }

        DiagnosticRecord diagnostic;
        diagnostic.severity = APE_HOI4_PARSER_DIAGNOSTIC_WARNING;
        diagnostic.code = 2101;
        diagnostic.range = entry.range;
        diagnostic.message = "Duplicate country tag detected: ";
        diagnostic.message += entry.tag;
        diagnostics.push_back(std::move(diagnostic));
    }
}

static void appendDuplicateIdeaDiagnostics(
    const std::vector<IdeaRecord>& entries,
    std::vector<DiagnosticRecord>& diagnostics
) {
    std::unordered_map<std::string, const IdeaRecord*> firstEntries;
    firstEntries.reserve(entries.size());

    for (const IdeaRecord& entry : entries) {
        if (entry.id.empty()) {
            continue;
        }

        const auto it = firstEntries.find(entry.id);
        if (it == firstEntries.end()) {
            firstEntries.emplace(entry.id, &entry);
            continue;
        }

        DiagnosticRecord diagnostic;
        diagnostic.severity = APE_HOI4_PARSER_DIAGNOSTIC_WARNING;
        diagnostic.code = 4101;
        diagnostic.range = entry.idRange;
        diagnostic.message = "Duplicate idea id detected: ";
        diagnostic.message += entry.id;
        diagnostics.push_back(std::move(diagnostic));
    }
}

static void appendDuplicateScriptedTriggerDiagnostics(
    const std::vector<ScriptedTriggerRecord>& entries,
    std::vector<DiagnosticRecord>& diagnostics
) {
    std::unordered_map<std::string, const ScriptedTriggerRecord*> firstEntries;
    firstEntries.reserve(entries.size());

    for (const ScriptedTriggerRecord& entry : entries) {
        if (entry.id.empty()) {
            continue;
        }

        const auto it = firstEntries.find(entry.id);
        if (it == firstEntries.end()) {
            firstEntries.emplace(entry.id, &entry);
            continue;
        }

        DiagnosticRecord diagnostic;
        diagnostic.severity = APE_HOI4_PARSER_DIAGNOSTIC_WARNING;
        diagnostic.code = 5101;
        diagnostic.range = entry.idRange;
        diagnostic.message = "Duplicate scripted trigger id detected: ";
        diagnostic.message += entry.id;
        diagnostics.push_back(std::move(diagnostic));
    }
}

static void appendDuplicateScriptedEffectDiagnostics(
    const std::vector<ScriptedEffectRecord>& entries,
    std::vector<DiagnosticRecord>& diagnostics
) {
    std::unordered_map<std::string, const ScriptedEffectRecord*> firstEntries;
    firstEntries.reserve(entries.size());

    for (const ScriptedEffectRecord& entry : entries) {
        if (entry.id.empty()) {
            continue;
        }

        const auto it = firstEntries.find(entry.id);
        if (it == firstEntries.end()) {
            firstEntries.emplace(entry.id, &entry);
            continue;
        }

        DiagnosticRecord diagnostic;
        diagnostic.severity = APE_HOI4_PARSER_DIAGNOSTIC_WARNING;
        diagnostic.code = 6101;
        diagnostic.range = entry.idRange;
        diagnostic.message = "Duplicate scripted effect id detected: ";
        diagnostic.message += entry.id;
        diagnostics.push_back(std::move(diagnostic));
    }
}

static void appendFocusDiagnostics(std::string_view text, const std::vector<FocusRecord>& focusEntries, std::vector<DiagnosticRecord>& diagnostics) {
    int braceBalance = 0;
    for (const char ch : text) {
        if (ch == '{') {
            ++braceBalance;
        } else if (ch == '}') {
            --braceBalance;
        }
    }

    if (braceBalance != 0) {
        DiagnosticRecord diagnostic;
        diagnostic.severity = APE_HOI4_PARSER_DIAGNOSTIC_WARNING;
        diagnostic.code = 3002;
        diagnostic.message = "Focus document contains unbalanced braces.";
        diagnostics.push_back(std::move(diagnostic));
    }

    std::unordered_map<std::string, const FocusRecord*> firstEntries;
    firstEntries.reserve(focusEntries.size());

    for (const FocusRecord& entry : focusEntries) {
        if (entry.id.empty()) {
            continue;
        }

        const auto it = firstEntries.find(entry.id);
        if (it == firstEntries.end()) {
            firstEntries.emplace(entry.id, &entry);
            continue;
        }

        DiagnosticRecord diagnostic;
        diagnostic.severity = APE_HOI4_PARSER_DIAGNOSTIC_WARNING;
        diagnostic.code = 3101;
        diagnostic.range = entry.idRange;
        diagnostic.message = "Duplicate focus id detected: ";
        diagnostic.message += entry.id;
        diagnostics.push_back(std::move(diagnostic));
    }

    if (!focusEntries.empty()) {
        return;
    }

    if (text.find("focus") != std::string_view::npos) {
        DiagnosticRecord diagnostic;
        diagnostic.severity = APE_HOI4_PARSER_DIAGNOSTIC_WARNING;
        diagnostic.code = 3001;
        diagnostic.message = "Focus content was detected but no valid focus id was extracted.";
        diagnostics.push_back(std::move(diagnostic));
    }
}

} // namespace

ParserSession::ParserSession()
    : m_sessionMemory(1024 * 64) {
}

ParserSession::~ParserSession() = default;

bool ParserSession::setEffectiveFiles(const APEHOI4ParserFileEntry* entries, int count) {
    if ((entries == nullptr && count != 0) || count < 0) {
        setLastError("Invalid effective file input.");
        return false;
    }

    m_effectiveFiles.clear();
    for (int i = 0; i < count; ++i) {
        const APEHOI4ParserFileEntry& entry = entries[i];
        if (entry.logicalPathUtf8 == nullptr) {
            setLastError("Effective file entry contains a null logical path pointer.");
            return false;
        }

        EffectiveFileRecord record;
        record.logicalPath = entry.logicalPathUtf8;
        record.sourceKind = entry.sourceKind;

        m_effectiveFiles[record.logicalPath] = std::move(record);
    }

    m_lastError.clear();
    return true;
}

bool ParserSession::setReplacePaths(const APEHOI4ParserReplacePathEntry* entries, int count) {
    if ((entries == nullptr && count != 0) || count < 0) {
        setLastError("Invalid replace path input.");
        return false;
    }

    m_replacePaths.clear();
    m_replacePaths.reserve(static_cast<size_t>(count));

    for (int i = 0; i < count; ++i) {
        if (entries[i].pathUtf8 == nullptr) {
            setLastError("Replace path entry contains a null string pointer.");
            return false;
        }

        ReplacePathRecord record;
        record.path = entries[i].pathUtf8;
        m_replacePaths.push_back(std::move(record));
    }

    m_lastError.clear();
    return true;
}

bool ParserSession::parseBuffer(
    std::string_view logicalPathUtf8,
    std::string_view textUtf8,
    uint32_t documentKind
) {
    if (logicalPathUtf8.empty()) {
        setLastError("Logical path must not be empty.");
        return false;
    }

    clearTransientState();

    m_lastLogicalPath.assign(logicalPathUtf8.begin(), logicalPathUtf8.end());
    m_lastSourceText.assign(textUtf8.begin(), textUtf8.end());
    m_parseStats.documentKind = inferDocumentKindFromPath(logicalPathUtf8, documentKind);

    const SourceText sourceText(m_lastSourceText);
    const Lexer lexer(sourceText);
    const std::vector<Token> tokens = lexer.lexAll();
    const Parser parser(tokens);
    const SyntaxTree syntaxTree = parser.buildSyntaxTree(m_parseStats.documentKind);

    m_parseStats.tokenCount = static_cast<uint32_t>(tokens.size());
    m_parseStats.nodeCount = static_cast<uint32_t>(syntaxTree.size());

    switch (m_parseStats.documentKind) {
    case APE_HOI4_PARSER_DOCUMENT_LOCALIZATION: {
        appendLocalizationDiagnostics(sourceText.view(), m_diagnostics);

        const std::vector<LocalizationDomainEntry> domainEntries = parseLocalizationDocument(sourceText.view());
        m_localizationEntries.reserve(domainEntries.size());

        for (const LocalizationDomainEntry& entry : domainEntries) {
            LocalizationRecord record;
            record.key.assign(entry.key.begin(), entry.key.end());
            record.value.assign(entry.value.begin(), entry.value.end());
            record.keyRange = entry.keyRange;
            record.valueRange = entry.valueRange;
            m_localizationEntries.push_back(std::move(record));
        }

        appendDuplicateLocalizationDiagnostics(m_localizationEntries, m_diagnostics);
        break;
    }
    case APE_HOI4_PARSER_DOCUMENT_TAGS: {
        appendTagDiagnostics(sourceText.view(), m_diagnostics);

        const std::vector<TagDomainEntry> domainEntries = parseTagDocument(sourceText.view());
        m_tagEntries.reserve(domainEntries.size());

        for (const TagDomainEntry& entry : domainEntries) {
            TagRecord record;
            record.tag.assign(entry.tag.begin(), entry.tag.end());
            record.targetPath.assign(entry.targetPath.begin(), entry.targetPath.end());
            record.isDynamic = entry.isDynamic;
            record.range = entry.range;
            m_tagEntries.push_back(std::move(record));
        }

        appendDuplicateTagDiagnostics(m_tagEntries, m_diagnostics);
        break;
    }
    case APE_HOI4_PARSER_DOCUMENT_FOCUS: {
        const std::vector<FocusDomainEntry> domainEntries = parseFocusDocument(sourceText.view());
        m_focusEntries.reserve(domainEntries.size());

        for (const FocusDomainEntry& entry : domainEntries) {
            FocusRecord record;
            record.id.assign(entry.id.begin(), entry.id.end());
            record.icon.assign(entry.icon.begin(), entry.icon.end());
            record.x.assign(entry.x.begin(), entry.x.end());
            record.y.assign(entry.y.begin(), entry.y.end());
            record.idRange = entry.idRange;
            m_focusEntries.push_back(std::move(record));
        }

        appendFocusDiagnostics(sourceText.view(), m_focusEntries, m_diagnostics);
        break;
    }
    case APE_HOI4_PARSER_DOCUMENT_UNKNOWN: {
        const std::string normalizedPath = normalizePath(logicalPathUtf8);

        if (normalizedPath.find("/common/ideas/") != std::string::npos) {
            const std::vector<IdeaDomainEntry> domainEntries = parseIdeasDocument(sourceText.view());
            m_ideaEntries.reserve(domainEntries.size());

            for (const IdeaDomainEntry& entry : domainEntries) {
                IdeaRecord record;
                record.id.assign(entry.id.begin(), entry.id.end());
                record.category.assign(entry.category.begin(), entry.category.end());
                record.idRange = entry.idRange;
                m_ideaEntries.push_back(std::move(record));
            }

            appendDuplicateIdeaDiagnostics(m_ideaEntries, m_diagnostics);
            break;
        }

        if (normalizedPath.find("/common/scripted_triggers/") != std::string::npos) {
            const std::vector<ScriptedTriggerDomainEntry> domainEntries = parseScriptedTriggersDocument(sourceText.view());
            m_scriptedTriggerEntries.reserve(domainEntries.size());

            for (const ScriptedTriggerDomainEntry& entry : domainEntries) {
                ScriptedTriggerRecord record;
                record.id.assign(entry.id.begin(), entry.id.end());
                record.idRange = entry.idRange;
                m_scriptedTriggerEntries.push_back(std::move(record));
            }

            appendDuplicateScriptedTriggerDiagnostics(m_scriptedTriggerEntries, m_diagnostics);
            break;
        }

        if (normalizedPath.find("/common/scripted_effects/") != std::string::npos) {
            const std::vector<ScriptedEffectDomainEntry> domainEntries = parseScriptedEffectsDocument(sourceText.view());
            m_scriptedEffectEntries.reserve(domainEntries.size());

            for (const ScriptedEffectDomainEntry& entry : domainEntries) {
                ScriptedEffectRecord record;
                record.id.assign(entry.id.begin(), entry.id.end());
                record.idRange = entry.idRange;
                m_scriptedEffectEntries.push_back(std::move(record));
            }

            appendDuplicateScriptedEffectDiagnostics(m_scriptedEffectEntries, m_diagnostics);
            break;
        }

        m_diagnostics.push_back(makeUnsupportedDocumentDiagnostic(logicalPathUtf8));
        break;
    }
    default:
        m_diagnostics.push_back(makeUnsupportedDocumentDiagnostic(logicalPathUtf8));
        break;
    }

    m_parseStats.diagnosticCount = static_cast<uint32_t>(m_diagnostics.size());
    buildDebugSyntaxTreeJson();
    buildDebugDiagnosticsJson();
    m_lastError.clear();
    return true;
}

bool ParserSession::parseEffectiveFile(
    std::string_view logicalPathUtf8,
    uint32_t documentKind
) {
    (void)documentKind;

    if (logicalPathUtf8.empty()) {
        setLastError("Logical path must not be empty.");
        return false;
    }

    if (m_effectiveFiles.find(std::string(logicalPathUtf8)) == m_effectiveFiles.end()) {
        setLastError("Logical path was not found in effective files.");
        return false;
    }

    setLastError("Effective file loading must be provided by the bridge layer.");
    return false;
}

const char* ParserSession::getLastError() const {
    return m_lastError.c_str();
}

uint32_t ParserSession::getDiagnosticCount() const {
    return static_cast<uint32_t>(m_diagnostics.size());
}

uint32_t ParserSession::copyDiagnostics(APEHOI4ParserDiagnostic* outItems, uint32_t capacity) const {
    if (outItems == nullptr || capacity == 0) {
        return 0;
    }

    const uint32_t copyCount = clampCountToCapacity(static_cast<uint32_t>(m_diagnostics.size()), capacity);
    for (uint32_t i = 0; i < copyCount; ++i) {
        outItems[i].severity = m_diagnostics[i].severity;
        outItems[i].code = m_diagnostics[i].code;
        outItems[i].range = m_diagnostics[i].range;
        outItems[i].messageUtf8 = m_diagnostics[i].message.c_str();
    }
    return copyCount;
}

uint32_t ParserSession::getLocalizationEntryCount() const {
    return static_cast<uint32_t>(m_localizationEntries.size());
}

uint32_t ParserSession::copyLocalizationEntries(APEHOI4ParserLocalizationEntry* outItems, uint32_t capacity) const {
    if (outItems == nullptr || capacity == 0) {
        return 0;
    }

    const uint32_t copyCount = clampCountToCapacity(static_cast<uint32_t>(m_localizationEntries.size()), capacity);
    for (uint32_t i = 0; i < copyCount; ++i) {
        outItems[i].keyUtf8 = m_localizationEntries[i].key.c_str();
        outItems[i].valueUtf8 = m_localizationEntries[i].value.c_str();
        outItems[i].keyRange = m_localizationEntries[i].keyRange;
        outItems[i].valueRange = m_localizationEntries[i].valueRange;
    }
    return copyCount;
}

uint32_t ParserSession::getTagEntryCount() const {
    return static_cast<uint32_t>(m_tagEntries.size());
}

uint32_t ParserSession::copyTagEntries(APEHOI4ParserTagEntry* outItems, uint32_t capacity) const {
    if (outItems == nullptr || capacity == 0) {
        return 0;
    }

    const uint32_t copyCount = clampCountToCapacity(static_cast<uint32_t>(m_tagEntries.size()), capacity);
    for (uint32_t i = 0; i < copyCount; ++i) {
        outItems[i].tagUtf8 = m_tagEntries[i].tag.c_str();
        outItems[i].targetPathUtf8 = m_tagEntries[i].targetPath.c_str();
        outItems[i].isDynamic = m_tagEntries[i].isDynamic ? 1u : 0u;
        outItems[i].range = m_tagEntries[i].range;
    }
    return copyCount;
}

uint32_t ParserSession::getFocusEntryCount() const {
    return static_cast<uint32_t>(m_focusEntries.size());
}

uint32_t ParserSession::copyFocusEntries(APEHOI4ParserFocusEntry* outItems, uint32_t capacity) const {
    if (outItems == nullptr || capacity == 0) {
        return 0;
    }

    const uint32_t copyCount = clampCountToCapacity(static_cast<uint32_t>(m_focusEntries.size()), capacity);
    for (uint32_t i = 0; i < copyCount; ++i) {
        outItems[i].idUtf8 = m_focusEntries[i].id.c_str();
        outItems[i].iconUtf8 = m_focusEntries[i].icon.c_str();
        outItems[i].xUtf8 = m_focusEntries[i].x.c_str();
        outItems[i].yUtf8 = m_focusEntries[i].y.c_str();
        outItems[i].idRange = m_focusEntries[i].idRange;
    }
    return copyCount;
}

uint32_t ParserSession::getIdeaEntryCount() const {
    return static_cast<uint32_t>(m_ideaEntries.size());
}

uint32_t ParserSession::copyIdeaEntries(APEHOI4ParserIdeaEntry* outItems, uint32_t capacity) const {
    if (outItems == nullptr || capacity == 0) {
        return 0;
    }

    const uint32_t copyCount = clampCountToCapacity(static_cast<uint32_t>(m_ideaEntries.size()), capacity);
    for (uint32_t i = 0; i < copyCount; ++i) {
        outItems[i].idUtf8 = m_ideaEntries[i].id.c_str();
        outItems[i].categoryUtf8 = m_ideaEntries[i].category.c_str();
        outItems[i].idRange = m_ideaEntries[i].idRange;
    }
    return copyCount;
}

uint32_t ParserSession::getScriptedTriggerEntryCount() const {
    return static_cast<uint32_t>(m_scriptedTriggerEntries.size());
}

uint32_t ParserSession::copyScriptedTriggerEntries(APEHOI4ParserScriptedTriggerEntry* outItems, uint32_t capacity) const {
    if (outItems == nullptr || capacity == 0) {
        return 0;
    }

    const uint32_t copyCount = clampCountToCapacity(static_cast<uint32_t>(m_scriptedTriggerEntries.size()), capacity);
    for (uint32_t i = 0; i < copyCount; ++i) {
        outItems[i].idUtf8 = m_scriptedTriggerEntries[i].id.c_str();
        outItems[i].idRange = m_scriptedTriggerEntries[i].idRange;
    }
    return copyCount;
}

ParseStatsRecord ParserSession::getParseStats() const {
    return m_parseStats;
}

const char* ParserSession::getDebugSyntaxTreeJson() {
    buildDebugSyntaxTreeJson();
    return m_debugSyntaxTreeJson.c_str();
}

const char* ParserSession::getDebugDiagnosticsJson() {
    buildDebugDiagnosticsJson();
    return m_debugDiagnosticsJson.c_str();
}

void ParserSession::clearTransientState() {
    clearDomainState();
    m_diagnostics.clear();
    m_parseStats = ParseStatsRecord{};
    m_debugSyntaxTreeJson.clear();
    m_debugDiagnosticsJson.clear();
}

void ParserSession::clearDomainState() {
    m_localizationEntries.clear();
    m_tagEntries.clear();
    m_focusEntries.clear();
    m_ideaEntries.clear();
    m_scriptedTriggerEntries.clear();
    m_scriptedEffectEntries.clear();
}

void ParserSession::setLastError(std::string message) {
    m_lastError = std::move(message);
}

void ParserSession::buildDebugSyntaxTreeJson() {
    m_debugSyntaxTreeJson = APEHOI4Parser::buildDebugSyntaxTreeJson(*this);
}

void ParserSession::buildDebugDiagnosticsJson() {
    m_debugDiagnosticsJson = APEHOI4Parser::buildDebugDiagnosticsJson(*this);
}

uint32_t ParserSession::clampCountToCapacity(uint32_t count, uint32_t capacity) {
    return std::min(count, capacity);
}

} // namespace APEHOI4Parser