#include "LocalizationParser.h"

#include <cctype>
#include <string>
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

static std::string toLowerAscii(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());

    for (const char ch : value) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    return lowered;
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

static bool isLocalizationHeader(std::string_view trimmedLine) {
    const std::string lowered = toLowerAscii(trimmedLine);
    return lowered.size() > 2 && lowered.rfind("l_", 0) == 0 && !trimmedLine.empty() && trimmedLine.back() == ':';
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

} // namespace

std::vector<LocalizationDomainEntry> parseLocalizationDocument(std::string_view text) {
    std::vector<LocalizationDomainEntry> entries;

    size_t lineStart = 0;
    uint32_t lineIndex = 0;

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

        if (!trimmedLine.empty() && !isLocalizationHeader(trimmedLine)) {
            const size_t colonPosInTrimmed = trimmedLine.find(':');
            if (colonPosInTrimmed != std::string_view::npos && colonPosInTrimmed != 0) {
                std::string_view keyPart = trimView(trimmedLine.substr(0, colonPosInTrimmed));
                std::string_view rawValuePart = trimView(trimmedLine.substr(colonPosInTrimmed + 1));

                if (!keyPart.empty() && !rawValuePart.empty()) {
                    size_t versionDigitsEnd = 0;
                    while (versionDigitsEnd < rawValuePart.size()
                        && std::isdigit(static_cast<unsigned char>(rawValuePart[versionDigitsEnd])) != 0) {
                        ++versionDigitsEnd;
                    }

                    if (versionDigitsEnd > 0) {
                        rawValuePart = trimView(rawValuePart.substr(versionDigitsEnd));
                    }

                    std::string_view valuePart = unquoteView(rawValuePart);

                    LocalizationDomainEntry entry{};
                    entry.key = keyPart;
                    entry.value = valuePart;

                    const size_t keyColumnStart = static_cast<size_t>(keyPart.data() - line.data());
                    const size_t keyColumnEnd = keyColumnStart + keyPart.size();
                    const size_t valueColumnStart = static_cast<size_t>(valuePart.data() - line.data());
                    const size_t valueColumnEnd = valueColumnStart + valuePart.size();

                    setRange(
                        entry.keyRange,
                        lineIndex,
                        static_cast<uint32_t>(lineStart),
                        static_cast<uint32_t>(keyColumnStart),
                        static_cast<uint32_t>(keyColumnEnd)
                    );

                    setRange(
                        entry.valueRange,
                        lineIndex,
                        static_cast<uint32_t>(lineStart),
                        static_cast<uint32_t>(valueColumnStart),
                        static_cast<uint32_t>(valueColumnEnd)
                    );

                    if (valueColumnEnd >= valueColumnStart && keyColumnEnd >= keyColumnStart) {
                        entries.push_back(entry);
                    }
                }
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