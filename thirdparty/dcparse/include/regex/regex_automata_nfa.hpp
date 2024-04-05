#ifndef _DC_PARSER_REGEX_AUTOMATA_NFA_HPP_
#define _DC_PARSER_REGEX_AUTOMATA_NFA_HPP_

#include <set>
#include <map>
#include <vector>
#include <sstream>
#include <memory>
#include <assert.h>
#include "./misc.hpp"
#include "./regex_char.hpp"
#include "./regex_automata.hpp"
#include "./regex_automata_dfa.hpp"


template<typename CharT>
class RegexNFA {
public:
    using traits = character_traits<CharT>;
    using char_type = CharT;
    using NFAState_t = size_t;
    using DFAState_t = typename RegexDFA<char_type>::DFAState_t;
    struct NFAEntry {
        char_type low, high;
        std::set<NFAState_t> state;

        NFAEntry() = delete;
        NFAEntry(char_type low, char_type high): low(low), high(high)
        {
            assert(low <= high);
        }
        NFAEntry(char_type low, char_type high, std::set<NFAState_t> states):
            low(low), high(high), state(std::move(states))
        {
            assert(low <= high);
        }
    };
    using NFATransitionTable = std::vector<std::vector<NFAEntry>>;

private:
    NFAState_t m_start_state;
    std::set<NFAState_t> m_final_states;
    NFATransitionTable m_transitions;
    std::vector<std::set<NFAState_t>> m_epsilon_closure;
    std::vector<std::vector<std::pair<char_type,char_type>>> m_range_units;

    std::vector<std::set<size_t>> get_epsilon_closure() const
    {
        std::vector<std::set<size_t>> worklist(this->m_transitions.size());
        for (size_t i=0;i<worklist.size();++i) {
            auto& states = this->m_transitions[i];
            if (states.size() > 0 && states[0].low == traits::EMPTY_CHAR) {
                worklist[i].insert(states[0].state.begin(), states[0].state.end());
            }
        }

        return transitive_closure(worklist);
    }

    std::vector<std::vector<std::pair<char_type,char_type>>> range_units_map() const
    {
        assert(this->m_epsilon_closure.size() == this->m_transitions.size());
        std::vector<std::vector<std::pair<char_type,char_type>>> result;
        result.resize(m_transitions.size());

        for (size_t i = 0; i < m_transitions.size(); ++i) {
            auto& ru = result[i];

            for (auto& ci: this->m_epsilon_closure[i]) {
                assert(ci < m_transitions.size());
                auto& trans = this->m_transitions[ci];
                for (auto& m: trans) {
                    if (m.low == traits::EMPTY_CHAR)
                        continue;

                    ru.push_back(std::make_pair(m.low, m.high));
                }
            }
            ru = split_ranges_to_units(std::move(ru));
        }

        return result;
    }

public:
    RegexNFA() = delete;
    RegexNFA(NFATransitionTable table, NFAState_t start_state, std::set<NFAState_t> final_states):
        m_transitions(std::move(table)), m_start_state(start_state), m_final_states(std::move(final_states))
    {
        this->m_epsilon_closure = this->get_epsilon_closure();
        this->m_range_units = this->range_units_map();
    }

    std::set<NFAState_t> start_closure() const
    {
        return this->m_epsilon_closure[this->m_start_state];
    }

    const std::set<NFAState_t>& final_states() const { return m_final_states; }
    const std::vector<std::set<NFAState_t>>& epsilon_closure() const { return m_epsilon_closure; }

    std::set<NFAState_t> state_transition(std::set<NFAState_t> stateset, char_type c) const
    {
        auto& nfa_epsilon_closure = this->epsilon_closure();
        std::set<NFAState_t> next_states;
        for (auto& state : stateset) {
            auto& transitions = this->m_transitions;
            assert(transitions.size() > state);
            auto& transition = transitions[state];
            auto lb = std::lower_bound(transition.begin(), transition.end(), c,
                [](const NFAEntry& entry, char_type c) {
                    return entry.high < c;
                });

            if (lb != transition.end() && lb->low <= c && c <= lb->high) {
                for (auto k=lb->state.begin(); k!=lb->state.end(); ++k) {
                    assert(*k < nfa_epsilon_closure.size());
                    const auto& cl = nfa_epsilon_closure[*k];
                    next_states.insert(cl.begin(), cl.end());
                }
            }
        }

        return next_states;
    }

    std::string to_string() const {
        std::ostringstream ss;
        ss << "start: " << m_start_state << std::endl;
        ss << "final: ";
        for (auto& s: m_final_states)
            ss << s << " ";
        ss << std::endl;
        ss << "transitions: " << std::endl;
        for (size_t i = 0; i < m_transitions.size(); ++i) {
            ss << i << ": ";
            for (auto& e: m_transitions[i]) {
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

    RegexDFA<char_type> compile() const
    {
        size_t dfa_state_count = 0;
        std::map<std::set<NFAState_t>,DFAState_t> dfa_state_map;
        const auto query_dfa_state = [&](std::set<NFAState_t> states) -> DFAState_t {
            auto it = dfa_state_map.find(states);
            if (it != dfa_state_map.end())
                return it->second;

            const auto val = dfa_state_count++;
            dfa_state_map[states] = val;
            return val;
        };

        const auto sclosure = this->start_closure();
        typename RegexDFA<char_type>::DFATransitionTable transtable;
        const auto start_state = query_dfa_state(sclosure);
        const auto dead_state = query_dfa_state({ });
        transtable.resize(dfa_state_count);
        transtable[dead_state].emplace_back(traits::MIN, traits::MAX, dead_state);

        const auto state_trans = [&](NFAState_t state, std::pair<char_type,char_type> ch) {
            assert(state < this->m_transitions.size());
            auto& trans = this->m_transitions[state];
            auto lb = std::lower_bound(trans.begin(), trans.end(), ch.second,
                                       [](const auto& te, char_type bd) { return te.high < bd; });
            if (lb == trans.end() || lb->low > ch.second)
                return std::set<size_t>();
            
            assert(lb->high >= ch.second);
            assert(ch.first >= lb->low);
            return lb->state;
        };

        const auto state_set_trans = [&](const std::set<NFAState_t>& states, std::pair<char_type,char_type> ch)
        {
            std::set<NFAState_t> next_state;
            for (auto& s: states) {
                auto sn = state_trans(s, ch);
                for (auto& ks: sn) {
                    auto& mm = this->m_epsilon_closure[ks];
                    next_state.insert(mm.begin(), mm.end());
                }
            }

            return next_state;
        };

        std::queue<std::set<NFAState_t>> process_queue;
        process_queue.push(sclosure);
        std::set<std::set<NFAState_t>> processed_states={ sclosure };

        while (!process_queue.empty()) {
            auto stateset = process_queue.front();
            process_queue.pop();
            auto state = query_dfa_state(stateset);
            if (transtable.size() <= state)
                transtable.resize(state + 1);
            auto& trans = transtable[state];

            std::set<std::pair<char_type,char_type>> range_units;
            for (auto& s: stateset) {
                assert(m_range_units.size() > s);
                auto& range_unit = m_range_units[s];
                range_units.insert(range_unit.begin(), range_unit.end());
            }
            auto ranges = split_ranges_to_units(
                    std::vector<std::pair<char_type,char_type>>(range_units.begin(), range_units.end()));

            auto lv = traits::MIN;
            for (auto& r: ranges) {
                auto sn = state_set_trans(stateset, r);
                if (processed_states.find(sn) == processed_states.end()) {
                    process_queue.push(sn);
                    processed_states.insert(sn);
                }

                auto n = query_dfa_state(sn);
                if (lv == traits::MAX) {
                    assert(r.first == r.second);
                    assert(r.first == traits::MAX);
                }

                if (r.first > lv)
                    trans.emplace_back(lv, r.first - 1, dead_state);
                trans.emplace_back(r.first, r.second, n);
                if (r.second < traits::MAX) {
                    lv = r.second + 1;
                } else {
                    assert(r.second == traits::MAX);
                    lv = traits::MAX;
                }
            }

            if (trans.empty()) {
                trans.emplace_back(traits::MIN, traits::MAX, dead_state);
            } else if (trans.back().high < traits::MAX) {
                trans.emplace_back(trans.back().high + 1, traits::MAX, dead_state);
            }
        }

        std::set<DFAState_t> final_states;
        for (auto& s: processed_states) {
            for (auto& f: this->m_final_states) {
                if (s.find(f) != s.end()) {
                    final_states.insert(query_dfa_state(s));
                    break;
                }
            }
        }

        return RegexDFA<char_type>(transtable, start_state, { dead_state }, final_states);
    }
};

template<typename CharT>
class NFAMatcher : public AutomataMatcher<CharT> {
private:
    using traits = character_traits<CharT>;
    using char_type = CharT;
    using NFAEntry = typename RegexNFA<char_type>::NFAEntry;
    using NFAState_t = typename RegexNFA<char_type>::NFAState_t;
    std::shared_ptr<RegexNFA<char_type>> m_nfa;
    std::set<NFAState_t> m_current_states;

public:
    NFAMatcher() = delete;
    NFAMatcher(std::shared_ptr<RegexNFA<char_type>> nfa):
        m_nfa(nfa)
    {
        if (this->m_nfa == nullptr)
            throw std::runtime_error("NFA is null");

        this->reset();
    }
    NFAMatcher(const std::vector<char_type>& pattern);

    std::shared_ptr<RegexNFA<char_type>> get_nfa() { return this->m_nfa; }

    virtual void feed(char_type c) override {
        assert(traits::MIN <= c && c <= traits::MAX);
        if (this->m_current_states.empty())
            return;

        auto next = this->m_nfa->state_transition(this->m_current_states, c);
        this->m_current_states = next;
    }
    virtual bool match() const override {
        auto& finals = m_nfa->final_states();
        return std::any_of(m_current_states.begin(), m_current_states.end(),
            [&finals](NFAState_t state) {
                return finals.find(state) != finals.end();
            });
    }
    virtual bool dead() const override {
        /** by inductive reasoning (for concatenation, union and kleene star)
         *  we can prove any state can reach final state */
        return m_current_states.empty();
    }
    virtual void reset() override {
        m_current_states = m_nfa->start_closure();
    }
    std::string to_string() const { return m_nfa->to_string(); }
    virtual ~NFAMatcher() = default;
};

#endif // _DC_PARSER_REGEX_AUTOMATA_NFA_HPP_
