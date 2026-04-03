#ifndef APE_HOI4_PARSER_AST_SYNTAX_KIND_H
#define APE_HOI4_PARSER_AST_SYNTAX_KIND_H

#include <cstdint>

namespace APEHOI4Parser {

enum class SyntaxKind : uint32_t {
    Unknown = 0,
    File,
    Assignment,
    Block,
    Value,
    LocalizationEntry,
    TagEntry,
    FocusEntry
};

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_AST_SYNTAX_KIND_H