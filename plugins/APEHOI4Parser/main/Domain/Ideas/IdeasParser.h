#ifndef APE_HOI4_PARSER_DOMAIN_IDEAS_IDEAS_PARSER_H
#define APE_HOI4_PARSER_DOMAIN_IDEAS_IDEAS_PARSER_H

#include "../../../APEHOI4ParserBridgeTypes.h"

#include <string_view>
#include <vector>

namespace APEHOI4Parser {

struct IdeaDomainEntry {
    std::string_view id;
    std::string_view category;
    APEHOI4ParserSourceRange idRange{};
};

std::vector<IdeaDomainEntry> parseIdeasDocument(std::string_view text);

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_DOMAIN_IDEAS_IDEAS_PARSER_H