#include "ParseResultJson.h"

#include <sstream>
#include <string>
#include <vector>

namespace APEHOI4Parser {
namespace {

static std::string escapeJsonString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);

    for (const char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                escaped += '?';
            } else {
                escaped += ch;
            }
            break;
        }
    }

    return escaped;
}

static void appendRangeJson(std::ostringstream& stream, const APEHOI4ParserSourceRange& range) {
    stream << "{";
    stream << "\"startLine\":" << range.startLine << ",";
    stream << "\"startColumn\":" << range.startColumn << ",";
    stream << "\"endLine\":" << range.endLine << ",";
    stream << "\"endColumn\":" << range.endColumn;
    stream << "}";
}

} // namespace

std::string buildDebugSyntaxTreeJson(const ParserSession& session) {
    const ParseStatsRecord stats = session.getParseStats();

    const uint32_t localizationCount = session.getLocalizationEntryCount();
    const uint32_t tagCount = session.getTagEntryCount();
    const uint32_t focusCount = session.getFocusEntryCount();

    std::vector<APEHOI4ParserLocalizationEntry> localizationEntries(localizationCount);
    std::vector<APEHOI4ParserTagEntry> tagEntries(tagCount);
    std::vector<APEHOI4ParserFocusEntry> focusEntries(focusCount);

    const uint32_t copiedLocalizationCount = localizationCount == 0
        ? 0
        : session.copyLocalizationEntries(localizationEntries.data(), localizationCount);
    const uint32_t copiedTagCount = tagCount == 0
        ? 0
        : session.copyTagEntries(tagEntries.data(), tagCount);
    const uint32_t copiedFocusCount = focusCount == 0
        ? 0
        : session.copyFocusEntries(focusEntries.data(), focusCount);

    const std::vector<IdeaRecord>& ideaEntries = session.m_ideaEntries;
    const std::vector<ScriptedTriggerRecord>& scriptedTriggerEntries = session.m_scriptedTriggerEntries;
    const std::vector<ScriptedEffectRecord>& scriptedEffectEntries = session.m_scriptedEffectEntries;

    std::ostringstream stream;
    stream << "{";
    stream << "\"documentKind\":" << stats.documentKind << ",";
    stream << "\"tokenCount\":" << stats.tokenCount << ",";
    stream << "\"nodeCount\":" << stats.nodeCount << ",";
    stream << "\"diagnosticCount\":" << stats.diagnosticCount << ",";

    stream << "\"localizations\":[";
    for (uint32_t i = 0; i < copiedLocalizationCount; ++i) {
        if (i != 0) {
            stream << ",";
        }

        stream << "{";
        stream << "\"key\":\"" << escapeJsonString(localizationEntries[i].keyUtf8 != nullptr ? localizationEntries[i].keyUtf8 : "") << "\",";
        stream << "\"value\":\"" << escapeJsonString(localizationEntries[i].valueUtf8 != nullptr ? localizationEntries[i].valueUtf8 : "") << "\",";
        stream << "\"keyRange\":";
        appendRangeJson(stream, localizationEntries[i].keyRange);
        stream << ",";
        stream << "\"valueRange\":";
        appendRangeJson(stream, localizationEntries[i].valueRange);
        stream << "}";
    }
    stream << "],";

    stream << "\"tags\":[";
    for (uint32_t i = 0; i < copiedTagCount; ++i) {
        if (i != 0) {
            stream << ",";
        }

        stream << "{";
        stream << "\"tag\":\"" << escapeJsonString(tagEntries[i].tagUtf8 != nullptr ? tagEntries[i].tagUtf8 : "") << "\",";
        stream << "\"targetPath\":\"" << escapeJsonString(tagEntries[i].targetPathUtf8 != nullptr ? tagEntries[i].targetPathUtf8 : "") << "\",";
        stream << "\"range\":";
        appendRangeJson(stream, tagEntries[i].range);
        stream << "}";
    }
    stream << "],";

    stream << "\"focuses\":[";
    for (uint32_t i = 0; i < copiedFocusCount; ++i) {
        if (i != 0) {
            stream << ",";
        }

        stream << "{";
        stream << "\"id\":\"" << escapeJsonString(focusEntries[i].idUtf8 != nullptr ? focusEntries[i].idUtf8 : "") << "\",";
        stream << "\"icon\":\"" << escapeJsonString(focusEntries[i].iconUtf8 != nullptr ? focusEntries[i].iconUtf8 : "") << "\",";
        stream << "\"x\":\"" << escapeJsonString(focusEntries[i].xUtf8 != nullptr ? focusEntries[i].xUtf8 : "") << "\",";
        stream << "\"y\":\"" << escapeJsonString(focusEntries[i].yUtf8 != nullptr ? focusEntries[i].yUtf8 : "") << "\",";
        stream << "\"idRange\":";
        appendRangeJson(stream, focusEntries[i].idRange);
        stream << "}";
    }
    stream << "],";

    stream << "\"ideas\":[";
    for (size_t i = 0; i < ideaEntries.size(); ++i) {
        if (i != 0) {
            stream << ",";
        }

        stream << "{";
        stream << "\"id\":\"" << escapeJsonString(ideaEntries[i].id) << "\",";
        stream << "\"category\":\"" << escapeJsonString(ideaEntries[i].category) << "\",";
        stream << "\"idRange\":";
        appendRangeJson(stream, ideaEntries[i].idRange);
        stream << "}";
    }
    stream << "],";

    stream << "\"scriptedTriggers\":[";
    for (size_t i = 0; i < scriptedTriggerEntries.size(); ++i) {
        if (i != 0) {
            stream << ",";
        }

        stream << "{";
        stream << "\"id\":\"" << escapeJsonString(scriptedTriggerEntries[i].id) << "\",";
        stream << "\"idRange\":";
        appendRangeJson(stream, scriptedTriggerEntries[i].idRange);
        stream << "}";
    }
    stream << "],";

    stream << "\"scriptedEffects\":[";
    for (size_t i = 0; i < scriptedEffectEntries.size(); ++i) {
        if (i != 0) {
            stream << ",";
        }

        stream << "{";
        stream << "\"id\":\"" << escapeJsonString(scriptedEffectEntries[i].id) << "\",";
        stream << "\"idRange\":";
        appendRangeJson(stream, scriptedEffectEntries[i].idRange);
        stream << "}";
    }
    stream << "]";

    stream << "}";
    return stream.str();
}

std::string buildDebugDiagnosticsJson(const ParserSession& session) {
    const uint32_t diagnosticCount = session.getDiagnosticCount();
    std::vector<APEHOI4ParserDiagnostic> diagnostics(diagnosticCount);
    const uint32_t copiedCount = diagnosticCount == 0
        ? 0
        : session.copyDiagnostics(diagnostics.data(), diagnosticCount);

    std::ostringstream stream;
    stream << "[";

    for (uint32_t i = 0; i < copiedCount; ++i) {
        if (i != 0) {
            stream << ",";
        }

        const APEHOI4ParserDiagnostic& diagnostic = diagnostics[i];
        stream << "{";
        stream << "\"severity\":" << diagnostic.severity << ",";
        stream << "\"code\":" << diagnostic.code << ",";
        stream << "\"range\":";
        appendRangeJson(stream, diagnostic.range);
        stream << ",";
        stream << "\"message\":\"" << escapeJsonString(diagnostic.messageUtf8 != nullptr ? diagnostic.messageUtf8 : "") << "\"";
        stream << "}";
    }

    stream << "]";
    return stream.str();
}

} // namespace APEHOI4Parser