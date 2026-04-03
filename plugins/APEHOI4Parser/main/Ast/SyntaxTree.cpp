#include "SyntaxTree.h"

namespace APEHOI4Parser {

void SyntaxTree::clear() {
    m_nodes.clear();
}

void SyntaxTree::addNode(SyntaxNode node) {
    m_nodes.push_back(node);
}

const std::vector<SyntaxNode>& SyntaxTree::nodes() const {
    return m_nodes;
}

size_t SyntaxTree::size() const {
    return m_nodes.size();
}

} // namespace APEHOI4Parser