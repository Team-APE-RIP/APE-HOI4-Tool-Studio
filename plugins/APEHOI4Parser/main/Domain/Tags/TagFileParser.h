#ifndef APE_HOI4_PARSER_DOMAIN_TAGS_TAG_FILE_PARSER_H
#define APE_HOI4_PARSER_DOMAIN_TAGS_TAG_FILE_PARSER_H

#include "../../../APEHOI4ParserBridgeTypes.h"

#include <string_view>
#include <vector>

namespace APEHOI4Parser {

struct TagDomainEntry {
    std::string_view tag;
    std::string_view targetPath;
    bool isDynamic = false;
    APEHOI4ParserSourceRange range{};
};

std::vector<TagDomainEntry> parseTagDocument(std::string_view text);

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_DOMAIN_TAGS_TAG_FILE_PARSER_H