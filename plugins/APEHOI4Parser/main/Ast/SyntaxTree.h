#ifndef APE_HOI4_PARSER_AST_SYNTAX_TREE_H
#define APE_HOI4_PARSER_AST_SYNTAX_TREE_H

#include "SyntaxKind.h"

#include <cstdint>
#include <vector>

namespace APEHOI4Parser {

struct SyntaxNode {
    SyntaxKind kind = SyntaxKind::Unknown;
    uint32_t startOffset = 0;
    uint32_t endOffset = 0;
};

class SyntaxTree {
public:
    void clear();
    void addNode(SyntaxNode node);

    const std::vector<SyntaxNode>& nodes() const;
    size_t size() const;

private:
    std::vector<SyntaxNode> m_nodes;
};

} // namespace APEHOI4Parser

#endif // APE_HOI4_PARSER_AST_SYNTAX_TREE_H