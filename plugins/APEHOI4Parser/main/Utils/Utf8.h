#ifndef APE_HOI4_PARSER_UTILS_UTF8_H
#define APE_HOI4_PARSER_UTILS_UTF8_H

#include <string>
#include <string_view>

namespace APEHOI4Parser {

std::string toOwnedUtf8(std::string_view value);

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_UTILS_UTF8_H