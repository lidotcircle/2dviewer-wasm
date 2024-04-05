#ifndef _DC_PARSER_REGEX_AUTOMATA_DFA_IMPL_HPP_
#define _DC_PARSER_REGEX_AUTOMATA_DFA_IMPL_HPP_

#include "./regex_automata_dfa.hpp"
#include "./regex_automata_node_nfa.hpp"


template<typename CharT>
DFAMatcher<CharT>::DFAMatcher(const std::vector<char_type>& pattern)
{
    auto nfa = NodeNFA<CharT>::from_regex(pattern);
    auto rnfa = nfa.toRegexNFA();
    auto dfa = rnfa.compile();
    this->m_dfa = std::make_shared<RegexDFA<char_type>>(std::move(dfa));
    this->reset();
}

template<typename CharT>
NodeNFA<CharT> RegexDFA<CharT>::toNodeNFA() const
{
    using NodeNFATransitionTable = typename NodeNFA<CharT>::NodeNFATransitionTable;
    using NodeNFAEntry = typename NodeNFA<CharT>::NodeNFAEntry;
    using NFAState_t = typename NodeNFA<CharT>::NFAState_t;

    const auto epsilon_symbol = traits::EMPTY_CHAR;
    const NFAState_t startstate = this->m_start_state;
    const NFAState_t nfinals = this->m_transitions.size();
    NodeNFATransitionTable table;
    for (size_t s=0;s<this->m_transitions.size();s++) {
        if (this->m_final_states.find(s) != this->m_final_states.end()) {
            table[s].push_back(NodeNFAEntry(epsilon_symbol, epsilon_symbol, { nfinals }));
        }

        auto& t = this->m_transitions[s];
        for (auto& e: t) {
            if (this->m_dead_states.find(e.state) != this->m_dead_states.end())
                continue;

            NodeNFAEntry ne(e.low, e.high, {e.state});
            table[s].push_back(std::move(ne));
        }
    }

    return NodeNFA<CharT>(table, startstate, nfinals);
}

#endif // _DC_PARSER_REGEX_AUTOMATA_DFA_IMPL_HPP_
