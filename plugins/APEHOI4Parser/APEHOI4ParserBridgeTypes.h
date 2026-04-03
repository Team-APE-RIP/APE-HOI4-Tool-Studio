#ifndef APE_HOI4_PARSER_BRIDGE_TYPES_H
#define APE_HOI4_PARSER_BRIDGE_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define APE_HOI4_PARSER_EXPORT extern "C" __declspec(dllexport)
#else
#define APE_HOI4_PARSER_EXPORT extern "C"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Opaque session handle owned by the plugin.
 * The host must only pass it back to exported functions.
 */
typedef void* APEHOI4ParserSessionHandle;

/*
 * Generic status codes returned by exported functions.
 */
enum APEHOI4ParserStatusCode {
    APE_HOI4_PARSER_STATUS_OK = 0,
    APE_HOI4_PARSER_STATUS_INVALID_ARGUMENT = 1,
    APE_HOI4_PARSER_STATUS_NOT_FOUND = 2,
    APE_HOI4_PARSER_STATUS_PARSE_ERROR = 3,
    APE_HOI4_PARSER_STATUS_BUFFER_TOO_SMALL = 4,
    APE_HOI4_PARSER_STATUS_INTERNAL_ERROR = 5
};

/*
 * Source kind of the effective file entry.
 */
enum APEHOI4ParserSourceKind {
    APE_HOI4_PARSER_SOURCE_UNKNOWN = 0,
    APE_HOI4_PARSER_SOURCE_GAME = 1,
    APE_HOI4_PARSER_SOURCE_MOD = 2,
    APE_HOI4_PARSER_SOURCE_DLC = 3
};

/*
 * Diagnostic severity used across parser and domain-specific stages.
 */
enum APEHOI4ParserDiagnosticSeverity {
    APE_HOI4_PARSER_DIAGNOSTIC_INFO = 0,
    APE_HOI4_PARSER_DIAGNOSTIC_WARNING = 1,
    APE_HOI4_PARSER_DIAGNOSTIC_ERROR = 2
};

/*
 * Supported high-level document kinds for HOI4-first parsing.
 */
enum APEHOI4ParserDocumentKind {
    APE_HOI4_PARSER_DOCUMENT_UNKNOWN = 0,
    APE_HOI4_PARSER_DOCUMENT_LOCALIZATION = 1,
    APE_HOI4_PARSER_DOCUMENT_TAGS = 2,
    APE_HOI4_PARSER_DOCUMENT_FOCUS = 3
};

enum APEHOI4ParserCountryTagQueryFlags {
    APE_HOI4_PARSER_COUNTRY_TAG_QUERY_EXCLUDE_DYNAMIC = 0,
    APE_HOI4_PARSER_COUNTRY_TAG_QUERY_INCLUDE_DYNAMIC = 1
};

/*
 * A logical effective file entry supplied by the host. All strings are UTF-8.
 * The plugin does not take ownership of these pointers.
 * Real absolute paths must not be exposed to the plugin.
 */
typedef struct APEHOI4ParserFileEntry {
    const char* logicalPathUtf8;
    uint32_t sourceKind;
} APEHOI4ParserFileEntry;

/*
 * Replace path entry supplied by the host. All strings are UTF-8.
 * The plugin does not take ownership of these pointers.
 */
typedef struct APEHOI4ParserReplacePathEntry {
    const char* pathUtf8;
} APEHOI4ParserReplacePathEntry;

/*
 * A source range represented by byte offsets in the original UTF-8 buffer.
 * End offset is exclusive.
 */
typedef struct APEHOI4ParserSourceRange {
    uint32_t startOffset;
    uint32_t endOffset;
    uint32_t startLine;
    uint32_t startColumn;
    uint32_t endLine;
    uint32_t endColumn;
} APEHOI4ParserSourceRange;

/*
 * A diagnostic item written into a host-provided array.
 * messageUtf8 points to plugin-owned memory valid until the next mutating call
 * on the same session or until the session is destroyed.
 */
typedef struct APEHOI4ParserDiagnostic {
    uint32_t severity;
    uint32_t code;
    APEHOI4ParserSourceRange range;
    const char* messageUtf8;
} APEHOI4ParserDiagnostic;

/*
 * Localization output entry for HOI4 localization files.
 * keyUtf8 and valueUtf8 point to plugin-owned memory valid until the next
 * mutating call on the same session or until the session is destroyed.
 */
typedef struct APEHOI4ParserLocalizationEntry {
    const char* keyUtf8;
    const char* valueUtf8;
    APEHOI4ParserSourceRange keyRange;
    APEHOI4ParserSourceRange valueRange;
} APEHOI4ParserLocalizationEntry;

/*
 * Tag output entry for HOI4 country tag definitions.
 */
typedef struct APEHOI4ParserTagEntry {
    const char* tagUtf8;
    const char* targetPathUtf8;
    uint32_t isDynamic;
    APEHOI4ParserSourceRange range;
} APEHOI4ParserTagEntry;

/*
 * Focus output entry for HOI4 national focus definitions.
 */
typedef struct APEHOI4ParserFocusEntry {
    const char* idUtf8;
    const char* iconUtf8;
    const char* xUtf8;
    const char* yUtf8;
    APEHOI4ParserSourceRange idRange;
} APEHOI4ParserFocusEntry;

/*
 * Idea output entry for HOI4 ideas definitions.
 */
typedef struct APEHOI4ParserIdeaEntry {
    const char* idUtf8;
    const char* categoryUtf8;
    APEHOI4ParserSourceRange idRange;
} APEHOI4ParserIdeaEntry;

/*
 * Scripted trigger output entry for HOI4 scripted trigger definitions.
 */
typedef struct APEHOI4ParserScriptedTriggerEntry {
    const char* idUtf8;
    APEHOI4ParserSourceRange idRange;
} APEHOI4ParserScriptedTriggerEntry;

/*
 * Generic parser statistics for profiling and host-side inspection.
 */
typedef struct APEHOI4ParserParseStats {
    uint32_t tokenCount;
    uint32_t nodeCount;
    uint32_t diagnosticCount;
    uint32_t documentKind;
} APEHOI4ParserParseStats;

#ifdef __cplusplus
}
#endif

#endif // APE_HOI4_PARSER_BRIDGE_TYPES_H