#ifndef _DC_PARSER_REGEX_AUTOMATA_NODE_NFA_HPP_
#define _DC_PARSER_REGEX_AUTOMATA_NODE_NFA_HPP_

#include <map>
#include <set>
#include <vector>
#include <assert.h>
#include "./regex_char.hpp"
#include "./regex_automata_nfa.hpp"


template<typename CharT>
class NodeNFA {
public:
    using traits = character_traits<CharT>;
    using char_type = CharT;
    using NodeNFAEntry = typename RegexNFA<char_type>::NFAEntry;
    using NFAState_t = typename RegexNFA<char_type>::NFAState_t;
    using NodeNFATransitionTable = std::map<size_t,std::vector<NodeNFAEntry>>;

private:
    NFAState_t m_start_state, m_final_state;
    NodeNFATransitionTable m_transitions;

    static std::vector<NodeNFAEntry> merge_two_transitions(
            std::vector<NodeNFAEntry> first,
            std::vector<NodeNFAEntry> second)
    {
        std::vector<NodeNFAEntry> result;
        result.reserve(first.size() + second.size());

        auto first_it = first.begin();
        auto second_it = second.begin();
        while (first_it != first.end() && second_it != second.end()) {
            if (first_it->high < second_it->low) {
                result.push_back(std::move(*first_it));
                ++first_it;
            } else if (first_it->low > second_it->high) {
                result.push_back(std::move(*second_it));
                ++second_it;
            } else {
                auto& sm = first_it->low <  second_it->low ? first_it : second_it;
                auto& bg = first_it->low >= second_it->low ? first_it : second_it;
                
                if (sm->low < bg->low) {
                    result.push_back(NodeNFAEntry(sm->low, (char_type)(bg->low - 1), sm->state));
                    sm->low = bg->low;
                    assert(sm->low <= sm->high);
                }

                auto& sm2 = sm->high <  bg->high ? sm : bg;
                auto& bg2 = sm->high >= bg->high ? sm : bg;
                result.push_back(NodeNFAEntry(sm2->low, sm2->high, std::move(sm2->state)));
                result.back().state.insert(bg2->state.begin(), bg2->state.end());

                if (sm2->high + 1 <= bg2->high) {
                    bg2->low = (char_type)(sm->high + 1);
                } else {
                    bg2++;
                }
                sm2++;
            }
        }

        if (first_it != first.end()) {
            for (; first_it != first.end(); ++first_it)
                result.push_back(std::move(*first_it));
        }
        if (second_it != second.end()) {
            for (; second_it != second.end(); ++second_it)
                result.push_back(std::move(*second_it));
        }

        return result;
    }


public:
    NodeNFA() = delete;
    NodeNFA(NodeNFATransitionTable transitions, NFAState_t start_state, NFAState_t final_state) :
        m_transitions(std::move(transitions)), m_start_state(start_state), m_final_state(final_state) {}

    NodeNFATransitionTable& transitions() { return m_transitions; }
    const NodeNFATransitionTable& transitions() const { return m_transitions; }
    NFAState_t start_state() { return m_start_state; }
    NFAState_t final_state() { return m_final_state; }

    RegexNFA<char_type> toRegexNFA()
    {
        typename RegexNFA<char_type>::NFATransitionTable transitions;
        transitions.resize(this->m_transitions.size());
        assert(transitions.size() == this->m_transitions.size());

        std::map<size_t,size_t> state_rewriter;
        size_t state_size = 0;
        const auto query_state = [&](size_t state) {
            if (state_rewriter.find(state) == state_rewriter.end())
                state_rewriter[state] = state_size++;
            return state_rewriter[state];
        };
        const auto query_state_set = [&](std::set<size_t> states) {
            std::set<size_t> result;
            for (auto state : states)
                result.insert(query_state(state));
            return result;
        };

        for (auto& m: this->m_transitions) {
            auto state = query_state(m.first);
            if (transitions.size() < state_size)
                transitions.resize(state_size);
            assert(state < transitions.size());

            auto& entries = m.second;
            auto& new_entries = transitions[state];
            for (auto& entry : entries) {
                assert(entry.low <= entry.high);
                auto new_state = query_state_set(entry.state);
                new_entries.emplace_back(entry.low, entry.high, new_state);
            }
        }

        transitions.resize(state_size);
        auto start_state = query_state(this->m_start_state);
        auto finals = query_state(m_final_state);
        return RegexNFA<char_type>(transitions, start_state, { finals });
    }

    std::string to_string() const
    {
        std::ostringstream ss;
        ss << "start: " << m_start_state << std::endl;
        ss << "final: " << m_final_state << std::endl;
        ss << "transitions: " << std::endl;
        for (auto& trans: this->m_transitions) {
            ss << trans.first << ": ";
            for (auto& e: trans.second) {
                if (e.low == traits::EMPTY_CHAR) {
                    ss << "ε -> {";
                } else if (e.low == e.high) {
                    ss << char_to_string(e.low) << " -> {";
                } else {
                    ss << "[" << char_to_string(e.low) << "-" << char_to_string(e.high) << "] -> {";
                }
                for (auto& s: e.state)
                    ss << s << " ";
                ss << "} ";
            }
            ss << std::endl;
        }
        return ss.str();
    }

    void merge_with(NFAState_t state, std::vector<NodeNFAEntry> entries)
    {
        auto& transitions = this->m_transitions;

        if (transitions.find(state) == transitions.end()) {
            transitions[state] = std::move(entries);
        } else {
            transitions[state] = NodeNFA<char_type>::merge_two_transitions(
                    std::move(entries),
                    std::move(transitions[state]));
        }
    }

    void add_link(NFAState_t from, std::set<NFAState_t> to, char_type low, char_type high)
    {
        assert(low <= high);
        this->merge_with(from, { NodeNFAEntry(low, high, to) });
    }

    NodeNFA<char_type> relocate_state(StateAllocator<>& allocator, NFAState_t starts, NFAState_t finals)
    {
        NodeNFATransitionTable new_transitions;
        std::map<NFAState_t,NFAState_t> state_rewriter({ {this->m_start_state, starts}, {this->m_final_state, finals} });
        for (auto& transp: this->m_transitions) {
            if (state_rewriter.find(transp.first) == state_rewriter.end())
                state_rewriter[transp.first] = allocator.newstate();

            auto& new_transp = new_transitions[state_rewriter[transp.first]];
            for (auto& entry: transp.second) {
                std::set<NFAState_t> new_states;
                for (auto s: entry.state) {
                    if (state_rewriter.find(s) == state_rewriter.end())
                        state_rewriter[s] = allocator.newstate();
                    new_states.insert(state_rewriter[s]);
                }

                new_transp.emplace_back(entry.low, entry.high, new_states);
            }
        }

        return NodeNFA<char_type>(std::move(new_transitions), starts, finals);
    }

    static NodeNFA<char_type> from_basic_regex(const std::vector<char_type>& regex);
    static NodeNFA<char_type> from_regex(const std::vector<char_type>& regex);
};

#endif // _DC_PARSER_REGEX_AUTOMATA_NODE_NFA_HPP_
