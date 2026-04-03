#include "IdeasParser.h"

#include <cctype>
#include <string_view>
#include <vector>

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

static bool looksLikeAssignmentBlock(std::string_view trimmedLine, size_t& equalsPos, size_t& bracePos) {
    equalsPos = trimmedLine.find('=');
    if (equalsPos == std::string_view::npos) {
        return false;
    }

    bracePos = trimmedLine.find('{', equalsPos);
    return bracePos != std::string_view::npos;
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

std::vector<IdeaDomainEntry> parseIdeasDocument(std::string_view text) {
    std::vector<IdeaDomainEntry> entries;

    size_t lineStart = 0;
    uint32_t lineIndex = 0;

    std::string_view currentCategory;
    int categoryBraceDepth = 0;

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
            size_t equalsPos = std::string_view::npos;
            size_t bracePos = std::string_view::npos;

            if (looksLikeAssignmentBlock(trimmedLine, equalsPos, bracePos)) {
                const std::string_view left = trimView(trimmedLine.substr(0, equalsPos));

                if (categoryBraceDepth == 0) {
                    currentCategory = left;
                    categoryBraceDepth = countBraceDelta(trimmedLine);
                } else {
                    IdeaDomainEntry entry{};
                    entry.id = left;
                    entry.category = currentCategory;

                    const size_t idColumnStart = static_cast<size_t>(left.data() - line.data());
                    const size_t idColumnEnd = idColumnStart + left.size();
                    setRange(
                        entry.idRange,
                        lineIndex,
                        static_cast<uint32_t>(lineStart),
                        static_cast<uint32_t>(idColumnStart),
                        static_cast<uint32_t>(idColumnEnd)
                    );

                    entries.push_back(entry);
                    categoryBraceDepth += countBraceDelta(trimmedLine);
                }
            } else if (categoryBraceDepth > 0) {
                categoryBraceDepth += countBraceDelta(trimmedLine);
            }

            if (categoryBraceDepth <= 0) {
                currentCategory = std::string_view{};
                categoryBraceDepth = 0;
            }
        }

        if (lineEnd == text.size()) {
            break;
        }

        lineStart = lineEnd + 1;
        ++lineIndex;
    }

    return entries;
}

} // namespace APEHOI4Parser