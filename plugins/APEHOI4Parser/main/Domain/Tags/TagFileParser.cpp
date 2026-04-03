#include "TagFileParser.h"

#include <cctype>
#include <string>
#include <string_view>

namespace APEHOI4Parser {
namespace {

static bool isAsciiWhitespace(char ch) {
    return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

static bool isIdentifierStart(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

static bool isIdentifierPart(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '.';
}

static std::string toLowerAscii(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());

    for (const char ch : value) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    return lowered;
}

static std::string_view trimView(std::string_view value) {
    size_t begin = 0;
    size_t end = value.size();

    while (begin < end && isAsciiWhitespace(value[begin])) {
        ++begin;
    }

    while (end > begin && isAsciiWhitespace(value[end - 1])) {
        --end;
    }

    return value.substr(begin, end - begin);
}

static std::string_view removeComment(std::string_view line) {
    bool inString = false;
    char stringDelimiter = '\0';

    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];

        if (inString) {
            if (ch == '\\' && i + 1 < line.size()) {
                ++i;
                continue;
            }

            if (ch == stringDelimiter) {
                inString = false;
                stringDelimiter = '\0';
            }
            continue;
        }

        if (ch == '"' || ch == '\'') {
            inString = true;
            stringDelimiter = ch;
            continue;
        }

        if (ch == '#') {
            return line.substr(0, i);
        }
    }

    return line;
}

static bool isBooleanYes(std::string_view value) {
    return toLowerAscii(trimView(value)) == "yes";
}

static std::string_view unquoteView(std::string_view value) {
    const std::string_view trimmed = trimView(value);
    if (trimmed.size() >= 2) {
        const char first = trimmed.front();
        const char last = trimmed.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return trimmed.substr(1, trimmed.size() - 2);
        }
    }
    return trimmed;
}

static bool isValidTagKey(std::string_view key) {
    if (key.empty() || !isIdentifierStart(key.front())) {
        return false;
    }

    for (const char ch : key) {
        if (!isIdentifierPart(ch)) {
            return false;
        }
    }

    return true;
}

static void setRangeFromOffsets(
    APEHOI4ParserSourceRange& range,
    std::string_view originalText,
    uint32_t startOffset,
    uint32_t endOffset
) {
    if (endOffset < startOffset) {
        endOffset = startOffset;
    }

    uint32_t startLine = 0;
    uint32_t startColumn = 0;
    uint32_t endLine = 0;
    uint32_t endColumn = 0;

    uint32_t currentLine = 0;
    uint32_t currentColumn = 0;

    for (uint32_t i = 0; i < originalText.size(); ++i) {
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

} // namespace

std::vector<TagDomainEntry> parseTagDocument(std::string_view text) {
    std::vector<TagDomainEntry> entries;

    bool isDynamicDocument = false;
    size_t lineStart = 0;
    int braceDepth = 0;

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

        if (!trimmed.empty() && braceDepth == 0) {
            const size_t equalsPos = trimmed.find('=');
            if (equalsPos != std::string_view::npos) {
                const std::string_view key = trimView(trimmed.substr(0, equalsPos));
                const std::string_view value = trimView(trimmed.substr(equalsPos + 1));

                if (!key.empty() && !value.empty()) {
                    const std::string loweredKey = toLowerAscii(key);
                    if (loweredKey == "dynamic_tags") {
                        if (isBooleanYes(unquoteView(value))) {
                            isDynamicDocument = true;
                        }
                    } else if (isValidTagKey(key)) {
                        TagDomainEntry entry{};
                        entry.tag = key;
                        entry.targetPath = unquoteView(value);
                        entry.isDynamic = isDynamicDocument;

                        const size_t keyPosInLine = static_cast<size_t>(key.data() - line.data());
                        const size_t valuePosInLine = static_cast<size_t>(value.data() - line.data());

                        const uint32_t startOffset = static_cast<uint32_t>(lineStart + keyPosInLine);
                        const uint32_t endOffset = static_cast<uint32_t>(lineStart + valuePosInLine + value.size());

                        setRangeFromOffsets(entry.range, text, startOffset, endOffset);
                        entries.push_back(entry);
                    }
                }
            }
        }

        for (const char ch : uncommented) {
            if (ch == '{') {
                ++braceDepth;
            } else if (ch == '}') {
                if (braceDepth > 0) {
                    --braceDepth;
                }
            }
        }

        if (lineEnd == text.size()) {
            break;
        }

        lineStart = lineEnd + 1;
    }

    return entries;
}

} // namespace APEHOI4Parser