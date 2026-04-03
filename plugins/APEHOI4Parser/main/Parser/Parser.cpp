#include "Parser.h"

namespace APEHOI4Parser {

Parser::Parser(const std::vector<Token>& tokens)
    : m_tokens(tokens) {
}

SyntaxTree Parser::buildSyntaxTree(uint32_t documentKind) const {
    SyntaxTree tree;
    for (const Token& token : m_tokens) {
        if (token.kind == TokenKind::EndOfFile || token.kind == TokenKind::Comment) {
            continue;
        }

        SyntaxNode node;
        switch (documentKind) {
        case 1:
            node.kind = SyntaxKind::LocalizationEntry;
            break;
        case 2:
            node.kind = SyntaxKind::TagEntry;
            break;
        case 3:
            node.kind = SyntaxKind::FocusEntry;
            break;
        default:
            node.kind = SyntaxKind::Value;
            break;
        }
        node.startOffset = token.startOffset;
        node.endOffset = token.endOffset;
        tree.addNode(node);
    }
    return tree;
}

} // namespace APEHOI4Parser