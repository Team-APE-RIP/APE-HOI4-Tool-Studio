#include "SourceText.h"

#include <algorithm>

namespace APEHOI4Parser {

SourceText::SourceText(std::string text)
    : m_text(std::move(text)) {
    rebuildLineIndex();
}

void SourceText::reset(std::string text) {
    m_text = std::move(text);
    rebuildLineIndex();
}

const std::string& SourceText::buffer() const {
    return m_text;
}

std::string_view SourceText::view() const {
    return std::string_view(m_text);
}

size_t SourceText::size() const {
    return m_text.size();
}

bool SourceText::empty() const {
    return m_text.empty();
}

uint32_t SourceText::lineCount() const {
    return static_cast<uint32_t>(m_lineStarts.empty() ? 0 : m_lineStarts.size());
}

uint32_t SourceText::lineStart(uint32_t lineIndex) const {
    if (m_lineStarts.empty() || lineIndex >= m_lineStarts.size()) {
        return 0;
    }
    return m_lineStarts[lineIndex];
}

uint32_t SourceText::lineEnd(uint32_t lineIndex) const {
    if (m_lineStarts.empty() || lineIndex >= m_lineStarts.size()) {
        return 0;
    }

    if (lineIndex + 1 < m_lineStarts.size()) {
        uint32_t end = m_lineStarts[lineIndex + 1];
        if (end > 0 && m_text[end - 1] == '\n') {
            --end;
        }
        if (end > 0 && m_text[end - 1] == '\r') {
            --end;
        }
        return end;
    }

    return static_cast<uint32_t>(m_text.size());
}

uint32_t SourceText::findLineIndex(uint32_t offset) const {
    if (m_lineStarts.empty()) {
        return 0;
    }

    const auto it = std::upper_bound(m_lineStarts.begin(), m_lineStarts.end(), offset);
    if (it == m_lineStarts.begin()) {
        return 0;
    }
    return static_cast<uint32_t>((it - m_lineStarts.begin()) - 1);
}

void SourceText::rebuildLineIndex() {
    m_lineStarts.clear();
    m_lineStarts.push_back(0);

    for (uint32_t i = 0; i < m_text.size(); ++i) {
        if (m_text[i] == '\n' && i + 1 < m_text.size()) {
            m_lineStarts.push_back(i + 1);
        }
    }
}

} // namespace APEHOI4Parser