#ifndef APE_HOI4_PARSER_QUERIES_PARSE_RESULT_JSON_H
#define APE_HOI4_PARSER_QUERIES_PARSE_RESULT_JSON_H

#include "../Core/ParserSession.h"

#include <string>

namespace APEHOI4Parser {

std::string buildDebugSyntaxTreeJson(const ParserSession& session);
std::string buildDebugDiagnosticsJson(const ParserSession& session);

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_QUERIES_PARSE_RESULT_JSON_H