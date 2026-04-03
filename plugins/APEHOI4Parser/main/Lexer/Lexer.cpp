#include "Lexer.h"

#include <cctype>

namespace APEHOI4Parser {
namespace {

static bool isIdentifierChar(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '.' || ch == '/' || ch == '-';
}

}

Lexer::Lexer(const SourceText& sourceText)
    : m_sourceText(sourceText) {
}

std::vector<Token> Lexer::lexAll() const {
    const std::string_view text = m_sourceText.view();
    std::vector<Token> tokens;
    tokens.reserve(text.size() / 2 + 1);

    size_t i = 0;
    while (i < text.size()) {
        const char ch = text[i];

        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            ++i;
            continue;
        }

        if (ch == '#') {
            size_t end = i;
            while (end < text.size() && text[end] != '\n') {
                ++end;
            }
            tokens.push_back({TokenKind::Comment, static_cast<uint32_t>(i), static_cast<uint32_t>(end), text.substr(i, end - i)});
            i = end;
            continue;
        }

        if (ch == '=') {
            tokens.push_back({TokenKind::Equals, static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1), text.substr(i, 1)});
            ++i;
            continue;
        }

        if (ch == ':') {
            tokens.push_back({TokenKind::Colon, static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1), text.substr(i, 1)});
            ++i;
            continue;
        }

        if (ch == '{') {
            tokens.push_back({TokenKind::OpenBrace, static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1), text.substr(i, 1)});
            ++i;
            continue;
        }

        if (ch == '}') {
            tokens.push_back({TokenKind::CloseBrace, static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1), text.substr(i, 1)});
            ++i;
            continue;
        }

        if (ch == '"' || ch == '\'') {
            const char quote = ch;
            size_t end = i + 1;
            while (end < text.size() && text[end] != quote) {
                ++end;
            }
            if (end < text.size()) {
                ++end;
            }
            tokens.push_back({TokenKind::String, static_cast<uint32_t>(i), static_cast<uint32_t>(end), text.substr(i, end - i)});
            i = end;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            size_t end = i + 1;
            while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])) != 0) {
                ++end;
            }
            tokens.push_back({TokenKind::Number, static_cast<uint32_t>(i), static_cast<uint32_t>(end), text.substr(i, end - i)});
            i = end;
            continue;
        }

        if (isIdentifierChar(ch)) {
            size_t end = i + 1;
            while (end < text.size() && isIdentifierChar(text[end])) {
                ++end;
            }
            tokens.push_back({TokenKind::Identifier, static_cast<uint32_t>(i), static_cast<uint32_t>(end), text.substr(i, end - i)});
            i = end;
            continue;
        }

        tokens.push_back({TokenKind::Unknown, static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1), text.substr(i, 1)});
        ++i;
    }

    tokens.push_back({TokenKind::EndOfFile, static_cast<uint32_t>(text.size()), static_cast<uint32_t>(text.size()), std::string_view()});
    return tokens;
}

} // namespace APEHOI4Parser