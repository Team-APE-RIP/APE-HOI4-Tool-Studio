#include "FocusTreeParser.h"

#include <cctype>
#include <string_view>

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

static std::string_view unquoteView(std::string_view value) {
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return value.substr(1, value.size() - 2);
        }
    }
    return value;
}

static void setRange(
    APEHOI4ParserSourceRange& range,
    uint32_t lineIndex,
    uint32_t lineStartOffset,
    uint32_t startColumn,
    uint32_t endColumn
) {
    range.startLine = lineIndex;
    range.endLine = lineIndex;
    range.startColumn = startColumn;
    range.endColumn = endColumn;
    range.startOffset = lineStartOffset + startColumn;
    range.endOffset = lineStartOffset + endColumn;
}

static bool startsFocusBlock(std::string_view trimmedLine) {
    if (trimmedLine.find("focus") == std::string_view::npos) {
        return false;
    }

    const size_t focusPos = trimmedLine.find("focus");
    if (focusPos != 0) {
        return false;
    }

    const size_t bracePos = trimmedLine.find('{');
    if (bracePos == std::string_view::npos) {
        return false;
    }

    return true;
}

static int countBraceDelta(std::string_view text) {
    int delta = 0;
    for (const char ch : text) {
        if (ch == '{') {
            ++delta;
        } else if (ch == '}') {
            --delta;
        }
    }
    return delta;
}

} // namespace

std::vector<FocusDomainEntry> parseFocusDocument(std::string_view text) {
    std::vector<FocusDomainEntry> entries;

    size_t lineStart = 0;
    uint32_t lineIndex = 0;

    bool inFocus = false;
    int braceDepth = 0;
    FocusDomainEntry currentFocus{};

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
        const std::string_view trimmedLine = trimView(uncommented);

        if (!trimmedLine.empty()) {
            if (!inFocus && startsFocusBlock(trimmedLine)) {
                inFocus = true;
                braceDepth = countBraceDelta(trimmedLine);
                currentFocus = FocusDomainEntry{};
            } else if (inFocus) {
                const size_t equalsPos = trimmedLine.find('=');
                if (equalsPos != std::string_view::npos && equalsPos != 0) {
                    const std::string_view key = trimView(trimmedLine.substr(0, equalsPos));
                    const std::string_view rawValue = trimView(trimmedLine.substr(equalsPos + 1));
                    const std::string_view value = unquoteView(rawValue);

                    if (key == "id" && !value.empty()) {
                        currentFocus.id = value;

                        const size_t valueColumnStart = static_cast<size_t>(value.data() - line.data());
                        const size_t valueColumnEnd = valueColumnStart + value.size();
                        setRange(
                            currentFocus.idRange,
                            lineIndex,
                            static_cast<uint32_t>(lineStart),
                            static_cast<uint32_t>(valueColumnStart),
                            static_cast<uint32_t>(valueColumnEnd)
                        );
                    } else if (key == "icon") {
                        currentFocus.icon = value;
                    } else if (key == "x") {
                        currentFocus.x = value;
                    } else if (key == "y") {
                        currentFocus.y = value;
                    }
                }

                braceDepth += countBraceDelta(trimmedLine);
                if (braceDepth <= 0) {
                    if (!currentFocus.id.empty()) {
                        entries.push_back(currentFocus);
                    }
                    currentFocus = FocusDomainEntry{};
                    inFocus = false;
                    braceDepth = 0;
                }
            }
        }

        if (lineEnd == text.size()) {
            break;
        }

        lineStart = lineEnd + 1;
        ++lineIndex;
    }

    if (inFocus && !currentFocus.id.empty()) {
        entries.push_back(currentFocus);
    }

    return entries;
}

} // namespace APEHOI4Parser