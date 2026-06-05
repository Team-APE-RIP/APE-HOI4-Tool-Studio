#ifndef APE_HOI4_PARSER_DOMAIN_FONTS_FONT_GFX_PARSER_H
#define APE_HOI4_PARSER_DOMAIN_FONTS_FONT_GFX_PARSER_H

#include "../../../APEHOI4ParserBridgeTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace APEHOI4Parser {

struct FontGfxTextColor {
    std::string code;
    int red = 255;
    int green = 255;
    int blue = 255;
};

struct FontGfxDomainEntry {
    std::string name;
    std::string path;
    std::string color;
    std::vector<std::string> fontFiles;
    std::vector<std::string> languages;
    std::vector<FontGfxTextColor> textColors;
    APEHOI4ParserSourceRange nameRange{};
};

std::vector<FontGfxDomainEntry> parseFontGfxDocument(std::string_view text);
std::vector<FontGfxTextColor> parseFontGfxGlobalTextColors(std::string_view text);

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_DOMAIN_FONTS_FONT_GFX_PARSER_H
