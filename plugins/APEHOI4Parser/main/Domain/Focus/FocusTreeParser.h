#ifndef APE_HOI4_PARSER_DOMAIN_FOCUS_FOCUS_TREE_PARSER_H
#define APE_HOI4_PARSER_DOMAIN_FOCUS_FOCUS_TREE_PARSER_H

#include "../../../APEHOI4ParserBridgeTypes.h"

#include <string_view>
#include <vector>

namespace APEHOI4Parser {

struct FocusDomainEntry {
    std::string_view id;
    std::string_view icon;
    std::string_view x;
    std::string_view y;
    APEHOI4ParserSourceRange idRange{};
};

std::vector<FocusDomainEntry> parseFocusDocument(std::string_view text);

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_DOMAIN_FOCUS_FOCUS_TREE_PARSER_H