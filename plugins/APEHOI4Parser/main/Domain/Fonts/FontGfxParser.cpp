#include "FontGfxParser.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>

namespace APEHOI4Parser {
namespace {

enum class TokenKind {
    Identifier,
    String,
    Equals,
    OpenBrace,
    CloseBrace,
    End
};

struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
    std::size_t start = 0;
    std::size_t end = 0;
};

using TextColorMap = std::map<std::string, FontGfxTextColor>;

struct ParsedFontBlock {
    FontGfxDomainEntry entry;
    TextColorMap localTextColors;
    bool hasLocalTextColors = false;
};

static bool isIdentifierChar(char ch) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    return std::isalnum(uch) != 0 || ch == '_' || ch == '-' || ch == '.' || ch == '/' || ch == ':';
}

static std::string toLowerAscii(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); }
    );
    return value;
}

static void setRangeFromOffsets(
    APEHOI4ParserSourceRange& range,
    std::string_view originalText,
    std::uint32_t startOffset,
    std::uint32_t endOffset
) {
    std::uint32_t startLine = 0;
    std::uint32_t startColumn = 0;
    std::uint32_t endLine = 0;
    std::uint32_t endColumn = 0;
    std::uint32_t currentLine = 0;
    std::uint32_t currentColumn = 0;

    for (std::uint32_t i = 0; i < originalText.size(); ++i) {
        if (i == startOffset) {
            startLine = currentLine;
            startColumn = currentColumn;
        }
        if (i == endOffset) {
            endLine = currentLine;
            endColumn = currentColumn;
            break;
        }
        if (originalText[i] == '\n') {
            ++currentLine;
            currentColumn = 0;
        } else {
            ++currentColumn;
        }
    }

    if (endOffset >= originalText.size()) {
        endLine = currentLine;
        endColumn = currentColumn;
    }

    range.startOffset = startOffset;
    range.endOffset = endOffset;
    range.startLine = startLine;
    range.startColumn = startColumn;
    range.endLine = endLine;
    range.endColumn = endColumn;
}

class TokenStream {
public:
    explicit TokenStream(std::string_view text) {
        tokenize(text);
    }

    const Token& peek(std::size_t offset = 0) const {
        const std::size_t index = m_index + offset;
        if (index < m_tokens.size()) {
            return m_tokens[index];
        }
        return m_end;
    }

    Token take() {
        const Token token = peek();
        if (m_index < m_tokens.size()) {
            ++m_index;
        }
        return token;
    }

    bool atEnd() const {
        return peek().kind == TokenKind::End;
    }

private:
    void tokenize(std::string_view text) {
        std::size_t i = 0;
        while (i < text.size()) {
            const char ch = text[i];
            if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                ++i;
                continue;
            }
            if (ch == '#') {
                while (i < text.size() && text[i] != '\n') {
                    ++i;
                }
                continue;
            }
            if (ch == '=') {
                m_tokens.push_back({TokenKind::Equals, "=", i, i + 1});
                ++i;
                continue;
            }
            if (ch == '{') {
                m_tokens.push_back({TokenKind::OpenBrace, "{", i, i + 1});
                ++i;
                continue;
            }
            if (ch == '}') {
                m_tokens.push_back({TokenKind::CloseBrace, "}", i, i + 1});
                ++i;
                continue;
            }
            if (ch == '"' || ch == '\'') {
                const char quote = ch;
                const std::size_t start = i;
                ++i;
                std::string value;
                while (i < text.size()) {
                    if (text[i] == '\\' && i + 1 < text.size()) {
                        value.push_back(text[i + 1]);
                        i += 2;
                        continue;
                    }
                    if (text[i] == quote) {
                        ++i;
                        break;
                    }
                    value.push_back(text[i]);
                    ++i;
                }
                m_tokens.push_back({TokenKind::String, std::move(value), start, i});
                continue;
            }
            if (isIdentifierChar(ch)) {
                const std::size_t start = i;
                while (i < text.size() && isIdentifierChar(text[i])) {
                    ++i;
                }
                m_tokens.push_back({TokenKind::Identifier, std::string(text.substr(start, i - start)), start, i});
                continue;
            }
            ++i;
        }
    }

    std::vector<Token> m_tokens;
    std::size_t m_index = 0;
    Token m_end{TokenKind::End, {}, 0, 0};
};

static bool isValueToken(const Token& token) {
    return token.kind == TokenKind::Identifier || token.kind == TokenKind::String;
}

static bool parseByteValue(const Token& token, int& outValue) {
    if (!isValueToken(token) || token.text.empty()) {
        return false;
    }

    char* endPtr = nullptr;
    const long parsed = std::strtol(token.text.c_str(), &endPtr, 10);
    if (endPtr == token.text.c_str()) {
        return false;
    }

    outValue = std::clamp(static_cast<int>(parsed), 0, 255);
    return true;
}

static void skipValue(TokenStream& stream) {
    if (stream.peek().kind == TokenKind::OpenBrace) {
        int depth = 0;
        while (!stream.atEnd()) {
            const Token token = stream.take();
            if (token.kind == TokenKind::OpenBrace) {
                ++depth;
            } else if (token.kind == TokenKind::CloseBrace) {
                --depth;
                if (depth <= 0) {
                    break;
                }
            }
        }
        return;
    }
    if (!stream.atEnd()) {
        stream.take();
    }
}

static bool parseRgbTriplet(TokenStream& stream, FontGfxTextColor& outColor) {
    if (stream.peek().kind != TokenKind::OpenBrace) {
        return false;
    }

    stream.take();
    int depth = 1;
    std::vector<int> values;
    while (!stream.atEnd() && depth > 0) {
        const Token token = stream.take();
        if (token.kind == TokenKind::OpenBrace) {
            ++depth;
            continue;
        }
        if (token.kind == TokenKind::CloseBrace) {
            --depth;
            continue;
        }
        if (depth == 1 && values.size() < 3) {
            int component = 0;
            if (parseByteValue(token, component)) {
                values.push_back(component);
            }
        }
    }

    if (values.size() < 3) {
        return false;
    }

    outColor.red = values[0];
    outColor.green = values[1];
    outColor.blue = values[2];
    return true;
}

static TextColorMap parseTextColors(TokenStream& stream) {
    TextColorMap colors;
    if (stream.peek().kind != TokenKind::OpenBrace) {
        return colors;
    }

    stream.take();
    int depth = 1;
    while (!stream.atEnd() && depth > 0) {
        const Token key = stream.take();
        if (key.kind == TokenKind::OpenBrace) {
            ++depth;
            continue;
        }
        if (key.kind == TokenKind::CloseBrace) {
            --depth;
            continue;
        }
        if (depth != 1 || !isValueToken(key) || key.text.empty()) {
            continue;
        }
        if (stream.peek().kind != TokenKind::Equals) {
            continue;
        }
        stream.take();

        FontGfxTextColor color;
        color.code = key.text;
        if (parseRgbTriplet(stream, color)) {
            const std::string code = color.code;
            colors[code] = std::move(color);
        } else {
            skipValue(stream);
        }
    }
    return colors;
}

static void mergeTextColors(TextColorMap& target, const TextColorMap& source) {
    for (const auto& [code, color] : source) {
        target[code] = color;
    }
}

static std::vector<FontGfxTextColor> textColorVectorFromMap(const TextColorMap& colors) {
    std::vector<FontGfxTextColor> result;
    result.reserve(colors.size());
    for (const auto& [code, color] : colors) {
        result.push_back(color);
    }
    return result;
}

static TextColorMap textColorMapFromVector(const std::vector<FontGfxTextColor>& colors) {
    TextColorMap result;
    for (const FontGfxTextColor& color : colors) {
        if (!color.code.empty()) {
            result[color.code] = color;
        }
    }
    return result;
}

static void parseGlobalTextColorsInScope(TokenStream& stream, TextColorMap& colors, bool stopAtCloseBrace) {
    while (!stream.atEnd()) {
        const Token key = stream.take();
        if (key.kind == TokenKind::CloseBrace) {
            if (stopAtCloseBrace) {
                return;
            }
            continue;
        }
        if (key.kind != TokenKind::Identifier) {
            continue;
        }
        const std::string loweredKey = toLowerAscii(key.text);
        if (stream.peek().kind != TokenKind::Equals) {
            continue;
        }
        stream.take();

        if (loweredKey == "bitmapfonts") {
            if (stream.peek().kind == TokenKind::OpenBrace) {
                stream.take();
                parseGlobalTextColorsInScope(stream, colors, true);
            } else {
                skipValue(stream);
            }
            continue;
        }
        if (loweredKey == "bitmapfont" || loweredKey == "bitmapfont_override") {
            skipValue(stream);
            continue;
        }
        if (loweredKey == "textcolors") {
            mergeTextColors(colors, parseTextColors(stream));
        } else {
            skipValue(stream);
        }
    }
}

static TextColorMap parseGlobalTextColors(std::string_view text) {
    TextColorMap colors;
    TokenStream stream(text);
    parseGlobalTextColorsInScope(stream, colors, false);
    return colors;
}

static std::vector<std::string> parseStringArray(TokenStream& stream) {
    std::vector<std::string> values;
    if (isValueToken(stream.peek())) {
        values.push_back(stream.take().text);
        return values;
    }
    if (stream.peek().kind != TokenKind::OpenBrace) {
        return values;
    }
    stream.take();
    int depth = 1;
    while (!stream.atEnd() && depth > 0) {
        const Token token = stream.take();
        if (token.kind == TokenKind::OpenBrace) {
            ++depth;
            continue;
        }
        if (token.kind == TokenKind::CloseBrace) {
            --depth;
            continue;
        }
        if (depth == 1 && isValueToken(token) && !token.text.empty()) {
            values.push_back(token.text);
        }
    }
    return values;
}

static ParsedFontBlock parseBitmapFont(TokenStream& stream, std::string_view originalText, const TextColorMap& globalTextColors) {
    ParsedFontBlock block;
    block.entry.textColors = textColorVectorFromMap(globalTextColors);
    if (stream.peek().kind != TokenKind::OpenBrace) {
        return block;
    }
    stream.take();
    int depth = 1;
    while (!stream.atEnd() && depth > 0) {
        Token key = stream.take();
        if (key.kind == TokenKind::OpenBrace) {
            ++depth;
            continue;
        }
        if (key.kind == TokenKind::CloseBrace) {
            --depth;
            continue;
        }
        if (depth != 1 || key.kind != TokenKind::Identifier) {
            continue;
        }
        if (stream.peek().kind != TokenKind::Equals) {
            continue;
        }
        stream.take();
        const std::string loweredKey = toLowerAscii(key.text);
        if (loweredKey == "name" && isValueToken(stream.peek())) {
            const Token value = stream.take();
            block.entry.name = value.text;
            setRangeFromOffsets(
                block.entry.nameRange,
                originalText,
                static_cast<std::uint32_t>(value.start),
                static_cast<std::uint32_t>(value.end)
            );
        } else if (loweredKey == "path" && isValueToken(stream.peek())) {
            block.entry.path = stream.take().text;
        } else if (loweredKey == "color" && isValueToken(stream.peek())) {
            block.entry.color = stream.take().text;
        } else if (loweredKey == "fontfiles" || loweredKey == "fontfile") {
            block.entry.fontFiles = parseStringArray(stream);
        } else if (loweredKey == "languages" || loweredKey == "language") {
            block.entry.languages = parseStringArray(stream);
        } else if (loweredKey == "textcolors") {
            block.localTextColors = parseTextColors(stream);
            block.hasLocalTextColors = !block.localTextColors.empty();
            TextColorMap mergedColors = globalTextColors;
            mergeTextColors(mergedColors, block.localTextColors);
            block.entry.textColors = textColorVectorFromMap(mergedColors);
        } else {
            skipValue(stream);
        }
    }
    return block;
}

static void appendParsedFontBlock(std::vector<FontGfxDomainEntry>& entries,
                                  std::map<std::string, FontGfxDomainEntry>& baseFontsByName,
                                  const std::string& blockKey,
                                  ParsedFontBlock block) {
    FontGfxDomainEntry& entry = block.entry;
    if (blockKey == "bitmapfont_override") {
        const auto baseIt = baseFontsByName.find(entry.name);
        if (baseIt != baseFontsByName.end()) {
            const FontGfxDomainEntry& baseEntry = baseIt->second;
            if (entry.color.empty()) {
                entry.color = baseEntry.color;
            }
            TextColorMap mergedColors = textColorMapFromVector(baseEntry.textColors);
            if (block.hasLocalTextColors) {
                mergeTextColors(mergedColors, block.localTextColors);
            }
            entry.textColors = textColorVectorFromMap(mergedColors);
        }
    }
    if (!entry.name.empty() && (!entry.path.empty() || !entry.fontFiles.empty() || !entry.languages.empty())) {
        if (blockKey == "bitmapfont") {
            baseFontsByName[entry.name] = entry;
        }
        entries.push_back(std::move(entry));
    }
}

static void parseFontScope(TokenStream& stream,
                           std::string_view originalText,
                           TextColorMap& globalTextColors,
                           std::map<std::string, FontGfxDomainEntry>& baseFontsByName,
                           std::vector<FontGfxDomainEntry>& entries,
                           bool stopAtCloseBrace) {
    while (!stream.atEnd()) {
        const Token key = stream.take();
        if (key.kind == TokenKind::CloseBrace) {
            if (stopAtCloseBrace) {
                return;
            }
            continue;
        }
        if (key.kind != TokenKind::Identifier) {
            continue;
        }
        const std::string loweredKey = toLowerAscii(key.text);
        if (stream.peek().kind != TokenKind::Equals) {
            continue;
        }
        stream.take();

        if (loweredKey == "bitmapfonts") {
            if (stream.peek().kind == TokenKind::OpenBrace) {
                stream.take();
                parseFontScope(stream, originalText, globalTextColors, baseFontsByName, entries, true);
            } else {
                skipValue(stream);
            }
            continue;
        }
        if (loweredKey == "textcolors") {
            mergeTextColors(globalTextColors, parseTextColors(stream));
            continue;
        }
        if (loweredKey == "bitmapfont" || loweredKey == "bitmapfont_override") {
            appendParsedFontBlock(
                entries,
                baseFontsByName,
                loweredKey,
                parseBitmapFont(stream, originalText, globalTextColors)
            );
        } else {
            skipValue(stream);
        }
    }
}

} // namespace

std::vector<FontGfxDomainEntry> parseFontGfxDocument(std::string_view text) {
    std::vector<FontGfxDomainEntry> entries;
    TextColorMap globalTextColors = parseGlobalTextColors(text);
    std::map<std::string, FontGfxDomainEntry> baseFontsByName;
    TokenStream stream(text);
    parseFontScope(stream, text, globalTextColors, baseFontsByName, entries, false);
    return entries;
}

std::vector<FontGfxTextColor> parseFontGfxGlobalTextColors(std::string_view text) {
    return textColorVectorFromMap(parseGlobalTextColors(text));
}

} // namespace APEHOI4Parser
