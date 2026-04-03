#include "DiagnosticBag.h"

namespace APEHOI4Parser {

void DiagnosticBag::clear() {
    m_items.clear();
}

void DiagnosticBag::add(Diagnostic diagnostic) {
    m_items.push_back(std::move(diagnostic));
}

const std::vector<Diagnostic>& DiagnosticBag::items() const {
    return m_items;
}

size_t DiagnosticBag::size() const {
    return m_items.size();
}

bool DiagnosticBag::empty() const {
    return m_items.empty();
}

} // namespace APEHOI4Parser