#ifndef _DC_PARSER_REGEX_AUTOMATA_DFA_HPP_
#define _DC_PARSER_REGEX_AUTOMATA_DFA_HPP_

#include <vector>
#include <memory>
#include <set>
#include <map>
#include <queue>
#include <sstream>
#include <assert.h>
#include "./regex_char.hpp"
#include "./regex_automata.hpp"


template<typename CharT>
class NodeNFA;

template<typename CharT>
class RegexDFA {
public:
    using DFAState_t = size_t;
    using traits = character_traits<CharT>;
    using char_type = CharT;
    struct DFAEntry {
        char_type low, high;
        DFAState_t state;

        DFAEntry(char_type low, char_type high, DFAState_t state)
            : low(low), high(high), state(state) {}
    };
    using DFATransitionTable = std::vector<std::vector<DFAEntry>>;

private:
    DFAState_t m_start_state;
    std::set<DFAState_t> m_dead_states, m_final_states;
    DFATransitionTable m_transitions;

public:
    RegexDFA() = delete;
    RegexDFA(DFATransitionTable table, DFAState_t start_state, std::set<DFAState_t> dead_states, std::set<DFAState_t> final_states):
        m_transitions(std::move(table)), m_start_state(start_state),
        m_dead_states(std::move(dead_states)), m_final_states(std::move(final_states))
    {}

    DFAState_t start_state() const { return m_start_state; }
    const std::set<DFAState_t>& dead_states() const { return m_dead_states; }
    const std::set<DFAState_t>& final_states() const { return m_final_states; }

    DFAState_t state_transition(DFAState_t state, char_type c) const {
        assert(state < m_transitions.size());
        auto& trans = this->m_transitions[state];
        auto lb = std::lower_bound(trans.begin(), trans.end(), c,
            [](const DFAEntry& entry, char_type c) {
                return entry.high < c;
            });
        assert(lb != trans.end());
        assert(lb->low <= c &&  c <= lb->high);
        return lb->state;
    }

    RegexDFA<char_type> complement() const 
    {
        std::set<DFAState_t> new_finals;
        for (size_t i=0;i<this->m_transitions.size();i++) {
            if (this->m_final_states.count(i) == 0) {
                new_finals.insert(i);
            }
        }

        return RegexDFA(this->m_transitions, this->m_start_state, std::set<DFAState_t>(), new_finals);
    }

    void optimize() 
    {
        std::map<DFAState_t,std::set<DFAState_t>> state_reverse_graph;
        for (size_t i=0;i<this->m_transitions.size();i++) {
            for (auto& entry : this->m_transitions[i]) {
                state_reverse_graph[entry.state].insert(i);
            }
        }

        std::set<size_t> seen_states = this->m_final_states;
        std::queue<size_t> process_queue;
        for (auto s: seen_states)
            process_queue.push(s);

        while (!process_queue.empty()) {
            auto s = process_queue.front();
            process_queue.pop();
            for (auto& t : state_reverse_graph[s]) {
                if (seen_states.find(t) == seen_states.end()) {
                    seen_states.insert(t);
                    process_queue.push(t);
                }
            }
        }

        std::set<size_t> deleted_states;
        for (size_t i=0;i<this->m_transitions.size();i++) {
            if (seen_states.find(i) == seen_states.end()) {
                deleted_states.insert(i);
            }
        }

        std::map<size_t,size_t> state_rewriter;
        const auto n_dead_state = 0;
        size_t state_n = 1;
        for (size_t i=0;i<this->m_transitions.size();i++) {
            if (deleted_states.find(i) == deleted_states.end())
            {
                state_rewriter[i] = state_n;
                state_n++;
            } else {
                state_rewriter[i] = 0;
            }
        }

        decltype(this->m_transitions) new_transitions(state_n);
        for (size_t i=0;i<this->m_transitions.size();i++) {
            if (deleted_states.find(i) != deleted_states.end())
                continue;

            auto m1 = state_rewriter[i];
            assert(new_transitions.size() > m1);
            auto& ntrans = new_transitions[m1];
            bool prev_is_dead = false;
            for (auto& entry : this->m_transitions[i]) {
                auto m2 = state_rewriter[entry.state];
                assert(new_transitions.size() > m2);
                if (prev_is_dead && m2 == n_dead_state) {
                    assert(ntrans.size() > 0);
                    assert(ntrans.back().high + 1 == entry.low);
                    ntrans.back().high = entry.high;
                } else {
                    ntrans.push_back(DFAEntry(entry.low, entry.high, m2));
                }
                prev_is_dead = m2 == n_dead_state;
            }
        }

        this->m_transitions = new_transitions;
        this->m_start_state = state_rewriter[this->m_start_state];
        this->m_dead_states.clear();
        this->m_dead_states.insert(n_dead_state);
        auto old_finals = std::move(this->m_final_states);
        this->m_final_states.clear();
        for (auto f: old_finals) {
            this->m_final_states.insert(state_rewriter[f]);
        }
    }

    NodeNFA<char_type> toNodeNFA() const;

    std::string to_string() const {
        std::stringstream ss;
        ss << "start state: " << m_start_state << std::endl;
        ss << "dead states: ";
        for (auto s : m_dead_states)
            ss << s << " ";
        ss << std::endl;
        ss << "final states: ";
        for (auto s : m_final_states)
            ss << s << " ";
        ss << std::endl;
        ss << "transitions: " << std::endl;
        for (size_t i = 0; i < m_transitions.size(); ++i) {
            ss << "state " << i << ": ";
            for (auto& entry : m_transitions[i]) {
                ss << "[" << char_to_string(entry.low) << "-" << char_to_string(entry.high) << "] -> " << entry.state << " ";
            }
            ss << std::endl;
        }
        return ss.str();
    }
};

template<typename CharT>
class DFAMatcher : public AutomataMatcher<CharT> {
private:
    using traits = character_traits<CharT>;
    using char_type = CharT;
    using entry_type= typename RegexDFA<char_type>::DFAEntry;
    using DFAState_t = typename RegexDFA<char_type>::DFAState_t;
    std::shared_ptr<RegexDFA<char_type>> m_dfa;
    DFAState_t m_current_state;

public:
    DFAMatcher() = delete;
    DFAMatcher(std::shared_ptr<RegexDFA<char_type>> dfa):
        m_dfa(dfa)
    {
        if (this->m_dfa == nullptr)
            throw std::runtime_error("DFA is null");

        this->reset();
    }
    DFAMatcher(const std::vector<char_type>& pattern);

    virtual void feed(char_type c) override {
        assert(traits::MIN <= c && c <= traits::MAX);
        auto& dead_states = this->m_dfa->dead_states();
        if (dead_states.find(this->m_current_state) != dead_states.end())
            return;

        auto cstate = this->m_current_state;
        this->m_current_state = this->m_dfa->state_transition(cstate, c);
    }

    virtual bool match() const override {
        auto& finals = m_dfa->final_states();
        return finals.find(this->m_current_state) != finals.end();
    }
    virtual bool dead() const override {
        auto& dead_states = m_dfa->dead_states();
        return dead_states.find(this->m_current_state) != dead_states.end();
    }
    virtual void reset() override {
        this->m_current_state = this->m_dfa->start_state();
    }
    std::string to_string() const {
        return m_dfa->to_string();
    }
    virtual ~DFAMatcher() = default;
};

#endif // _DC_PARSER_REGEX_AUTOMATA_DFA_HPP_
