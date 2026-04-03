#ifndef APE_HOI4_PARSER_LEXER_TOKEN_H
#define APE_HOI4_PARSER_LEXER_TOKEN_H

#include <cstdint>
#include <string_view>

namespace APEHOI4Parser {

enum class TokenKind : uint32_t {
    EndOfFile = 0,
    Identifier,
    String,
    Equals,
    Colon,
    OpenBrace,
    CloseBrace,
    Comment,
    Number,
    Unknown
};

struct Token {
    TokenKind kind = TokenKind::Unknown;
    uint32_t startOffset = 0;
    uint32_t endOffset = 0;
    std::string_view text;
};

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_LEXER_TOKEN_H