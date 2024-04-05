#ifndef _DC_PARSER_REGEX_AUTOMATA_NFA_IMPL_HPP_
#define _DC_PARSER_REGEX_AUTOMATA_NFA_IMPL_HPP_

#include "./regex_automata_nfa.hpp"


template<typename CharT>
NFAMatcher<CharT>::NFAMatcher(const std::vector<char_type>& pattern)
{
    auto nfa = NodeNFA<CharT>::from_regex(pattern);
    auto rnfa = nfa.toRegexNFA();
    this->m_nfa = std::make_shared<RegexNFA<char_type>>(std::move(rnfa));
    this->reset();
}

#endif // _DC_PARSER_REGEX_AUTOMATA_NFA_IMPL_HPP_
