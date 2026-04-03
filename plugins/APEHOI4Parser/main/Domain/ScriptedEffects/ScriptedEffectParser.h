#ifndef APE_HOI4_PARSER_DOMAIN_SCRIPTED_EFFECTS_SCRIPTED_EFFECT_PARSER_H
#define APE_HOI4_PARSER_DOMAIN_SCRIPTED_EFFECTS_SCRIPTED_EFFECT_PARSER_H

#include "../../../APEHOI4ParserBridgeTypes.h"

#include <string_view>
#include <vector>

namespace APEHOI4Parser {

struct ScriptedEffectDomainEntry {
    std::string_view id;
    APEHOI4ParserSourceRange idRange{};
};

std::vector<ScriptedEffectDomainEntry> parseScriptedEffectsDocument(std::string_view text);

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_DOMAIN_SCRIPTED_EFFECTS_SCRIPTED_EFFECT_PARSER_H