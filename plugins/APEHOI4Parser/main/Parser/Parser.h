#ifndef APE_HOI4_PARSER_PARSER_PARSER_H
#define APE_HOI4_PARSER_PARSER_PARSER_H

#include "../Ast/SyntaxTree.h"
#include "../Lexer/Token.h"

#include <vector>

namespace APEHOI4Parser {

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);

    SyntaxTree buildSyntaxTree(uint32_t documentKind) const;

private:
    const std::vector<Token>& m_tokens;
};

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_PARSER_PARSER_H