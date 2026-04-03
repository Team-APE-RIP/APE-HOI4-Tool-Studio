#ifndef APE_HOI4_PARSER_DIAGNOSTICS_DIAGNOSTIC_BAG_H
#define APE_HOI4_PARSER_DIAGNOSTICS_DIAGNOSTIC_BAG_H

#include "Diagnostic.h"

#include <vector>

namespace APEHOI4Parser {

class DiagnosticBag {
public:
    void clear();
    void add(Diagnostic diagnostic);

    const std::vector<Diagnostic>& items() const;
    size_t size() const;
    bool empty() const;

private:
    std::vector<Diagnostic> m_items;
};

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_DIAGNOSTICS_DIAGNOSTIC_BAG_H