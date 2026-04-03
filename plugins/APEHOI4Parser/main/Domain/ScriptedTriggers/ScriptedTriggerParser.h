#ifndef APE_HOI4_PARSER_DOMAIN_SCRIPTED_TRIGGERS_SCRIPTED_TRIGGER_PARSER_H
#define APE_HOI4_PARSER_DOMAIN_SCRIPTED_TRIGGERS_SCRIPTED_TRIGGER_PARSER_H

#include "../../../APEHOI4ParserBridgeTypes.h"

#include <string_view>
#include <vector>

namespace APEHOI4Parser {

struct ScriptedTriggerDomainEntry {
    std::string_view id;
    APEHOI4ParserSourceRange idRange{};
};

std::vector<ScriptedTriggerDomainEntry> parseScriptedTriggersDocument(std::string_view text);

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_DOMAIN_SCRIPTED_TRIGGERS_SCRIPTED_TRIGGER_PARSER_H