#ifndef APE_HOI4_PARSER_CORE_SOURCE_TEXT_H
#define APE_HOI4_PARSER_CORE_SOURCE_TEXT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace APEHOI4Parser {

class SourceText {
public:
    SourceText() = default;
    explicit SourceText(std::string text);

    void reset(std::string text);

    const std::string& buffer() const;
    std::string_view view() const;
    size_t size() const;
    bool empty() const;

    uint32_t lineCount() const;
    uint32_t lineStart(uint32_t lineIndex) const;
    uint32_t lineEnd(uint32_t lineIndex) const;
    uint32_t findLineIndex(uint32_t offset) const;

private:
    void rebuildLineIndex();

private:
    std::string m_text;
    std::vector<uint32_t> m_lineStarts;
};

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_CORE_SOURCE_TEXT_H