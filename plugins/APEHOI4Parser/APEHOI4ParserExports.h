#ifndef APE_HOI4_PARSER_EXPORTS_H
#define APE_HOI4_PARSER_EXPORTS_H

#include "APEHOI4ParserBridgeTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

APE_HOI4_PARSER_EXPORT const char* APE_HOI4Parser_GetPluginName();
APE_HOI4_PARSER_EXPORT const char* APE_HOI4Parser_GetLastError();
APE_HOI4_PARSER_EXPORT const char* APE_HOI4Parser_GetCountryTagsJson();
APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetCountryTagEntryCount(uint32_t queryFlags);
APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyCountryTagEntries(
    APEHOI4ParserTagEntry* outItems,
    uint32_t capacity,
    uint32_t queryFlags
);

APE_HOI4_PARSER_EXPORT APEHOI4ParserSessionHandle APE_HOI4Parser_CreateSession();
APE_HOI4_PARSER_EXPORT void APE_HOI4Parser_DestroySession(APEHOI4ParserSessionHandle handle);

APE_HOI4_PARSER_EXPORT int APE_HOI4Parser_SetEffectiveFiles(
    APEHOI4ParserSessionHandle handle,
    const APEHOI4ParserFileEntry* entries,
    int count
);

APE_HOI4_PARSER_EXPORT int APE_HOI4Parser_SetReplacePaths(
    APEHOI4ParserSessionHandle handle,
    const APEHOI4ParserReplacePathEntry* entries,
    int count
);

APE_HOI4_PARSER_EXPORT int APE_HOI4Parser_ParseBuffer(
    APEHOI4ParserSessionHandle handle,
    const char* logicalPathUtf8,
    const char* textUtf8,
    uint32_t documentKind
);

APE_HOI4_PARSER_EXPORT int APE_HOI4Parser_ParseEffectiveFile(
    APEHOI4ParserSessionHandle handle,
    const char* logicalPathUtf8,
    uint32_t documentKind
);

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetDiagnosticCount(
    APEHOI4ParserSessionHandle handle
);

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyDiagnostics(
    APEHOI4ParserSessionHandle handle,
    APEHOI4ParserDiagnostic* outItems,
    uint32_t capacity
);

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetLocalizationEntryCount(
    APEHOI4ParserSessionHandle handle
);

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyLocalizationEntries(
    APEHOI4ParserSessionHandle handle,
    APEHOI4ParserLocalizationEntry* outItems,
    uint32_t capacity
);

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetTagEntryCount(
    APEHOI4ParserSessionHandle handle
);

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyTagEntries(
    APEHOI4ParserSessionHandle handle,
    APEHOI4ParserTagEntry* outItems,
    uint32_t capacity
);

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetFocusEntryCount(
    APEHOI4ParserSessionHandle handle
);

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyFocusEntries(
    APEHOI4ParserSessionHandle handle,
    APEHOI4ParserFocusEntry* outItems,
    uint32_t capacity
);

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetIdeaEntryCount(
    APEHOI4ParserSessionHandle handle
);

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyIdeaEntries(
    APEHOI4ParserSessionHandle handle,
    APEHOI4ParserIdeaEntry* outItems,
    uint32_t capacity
);

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_GetScriptedTriggerEntryCount(
    APEHOI4ParserSessionHandle handle
);

APE_HOI4_PARSER_EXPORT uint32_t APE_HOI4Parser_CopyScriptedTriggerEntries(
    APEHOI4ParserSessionHandle handle,
    APEHOI4ParserScriptedTriggerEntry* outItems,
    uint32_t capacity
);

APE_HOI4_PARSER_EXPORT int APE_HOI4Parser_GetParseStats(
    APEHOI4ParserSessionHandle handle,
    APEHOI4ParserParseStats* outStats
);

APE_HOI4_PARSER_EXPORT const char* APE_HOI4Parser_GetDebugSyntaxTreeJson(
    APEHOI4ParserSessionHandle handle
);

APE_HOI4_PARSER_EXPORT const char* APE_HOI4Parser_GetDebugDiagnosticsJson(
    APEHOI4ParserSessionHandle handle
);

#ifdef __cplusplus
}
#endif

#endif // APE_HOI4_PARSER_EXPORTS_H