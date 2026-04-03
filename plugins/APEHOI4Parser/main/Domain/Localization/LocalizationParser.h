#ifndef APE_HOI4_PARSER_DOMAIN_LOCALIZATION_PARSER_H
#define APE_HOI4_PARSER_DOMAIN_LOCALIZATION_PARSER_H

#include "../../../APEHOI4ParserBridgeTypes.h"

#include <string_view>
#include <vector>

namespace APEHOI4Parser {

struct LocalizationDomainEntry {
    std::string_view key;
    std::string_view value;
    APEHOI4ParserSourceRange keyRange{};
    APEHOI4ParserSourceRange valueRange{};
};

std::vector<LocalizationDomainEntry> parseLocalizationDocument(std::string_view text);

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_DOMAIN_LOCALIZATION_PARSER_H