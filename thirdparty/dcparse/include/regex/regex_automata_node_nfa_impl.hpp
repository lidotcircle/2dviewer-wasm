#ifndef _DC_PARSER_REGEX_AUTOMATA_NODE_NFA_IMPL_HPP_
#define _DC_PARSER_REGEX_AUTOMATA_NODE_NFA_IMPL_HPP_

#include "./regex_automata_node_nfa.hpp"
#include "./regex_expr.hpp"
#include "./regex_expr_node.hpp"

/** static */
template<typename CharT>
NodeNFA<CharT> NodeNFA<CharT>::from_basic_regex(const std::vector<char_type>& regex)
{
    auto node = RegexNodeTreeGenerator<CharT>::parse(regex);
    StateAllocator allocator;
    auto starts = allocator.newstate();
    auto finals = allocator.newstate();
    auto nfa = node->to_nfa(allocator, starts, finals);
    return nfa;
}

/** static */
template<typename CharT>
NodeNFA<CharT> NodeNFA<CharT>::from_regex(const std::vector<char_type>& regex)
{
    Regex2BasicConvertor<char_type> conv;
    auto basic_regex = conv.convert(regex);
    return from_basic_regex(basic_regex);
}

#endif // _DC_PARSER_REGEX_AUTOMATA_NODE_NFA_IMPL_HPP_
