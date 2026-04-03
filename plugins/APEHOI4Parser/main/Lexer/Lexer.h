#ifndef APE_HOI4_PARSER_LEXER_LEXER_H
#define APE_HOI4_PARSER_LEXER_LEXER_H

#include "../Core/SourceText.h"
#include "Token.h"

#include <vector>

namespace APEHOI4Parser {

class Lexer {
public:
    explicit Lexer(const SourceText& sourceText);

    std::vector<Token> lexAll() const;

private:
    const SourceText& m_sourceText;
};

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_LEXER_LEXER_H