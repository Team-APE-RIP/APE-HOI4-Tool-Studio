#ifndef APE_HOI4_PARSER_DIAGNOSTICS_DIAGNOSTIC_H
#define APE_HOI4_PARSER_DIAGNOSTICS_DIAGNOSTIC_H

#include "../../APEHOI4ParserBridgeTypes.h"

#include <cstdint>
#include <string>

namespace APEHOI4Parser {

struct Diagnostic {
    uint32_t severity = APE_HOI4_PARSER_DIAGNOSTIC_ERROR;
    uint32_t code = 0;
    APEHOI4ParserSourceRange range{};
    std::string message;
};

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_DIAGNOSTICS_DIAGNOSTIC_H