#ifndef _DC_PARSER_REGEX_INTERNAL_HPP_
#define _DC_PARSER_REGEX_INTERNAL_HPP_

#include <algorithm>
#include <vector>
#include <memory>
#include <stdexcept>
#include <string>
#include "./regex_char.hpp"
#include "./regex_automata.hpp"
#include "./regex_automata_dfa.hpp"
#include "./regex_automata_dfa_impl.hpp"
#include "./regex_automata_nfa.hpp"
#include "./regex_automata_nfa_impl.hpp"
#include "./regex_automata_node_nfa.hpp"
#include "./regex_automata_node_nfa_impl.hpp"
#include "./regex_expr_node.hpp"
#include "./regex_expr.hpp"


template<typename CharT>
class SimpleRegExp: public AutomataMatcher<CharT>
{
private:
    using traits = character_traits<CharT>;
    using char_type = CharT;
    std::shared_ptr<AutomataMatcher<char_type>> m_matcher;

public:
    SimpleRegExp() = delete;

    template<typename Iterator>
    SimpleRegExp(Iterator begin, Iterator end)
    {
        std::vector<char_type> regex(begin, end);
        auto nfa = NodeNFA<char_type>::from_regex(regex);
        auto rnfa = nfa.toRegexNFA();
        auto rnfa_ptr = std::make_shared<RegexNFA<char_type>>(std::move(rnfa));
        this->m_matcher = std::make_shared<NFAMatcher<char_type>>(rnfa_ptr);
    }

    SimpleRegExp(const std::vector<char_type>& regex) {
        auto nfa = NodeNFA<char_type>::from_regex(regex);
        auto rnfa = nfa.toRegexNFA();
        auto rnfa_ptr = std::make_shared<RegexNFA<char_type>>(std::move(rnfa));
        this->m_matcher = std::make_shared<NFAMatcher<char_type>>(rnfa_ptr);
    }

    virtual void feed(char_type c) override { this->m_matcher->feed(c); }
    virtual bool match() const override { return this->m_matcher->match(); }
    virtual bool dead() const override { return this->m_matcher->dead(); }
    virtual void reset() override { this->m_matcher->reset(); }
    void compile()
    {
        auto nfa_matcher = std::dynamic_pointer_cast<NFAMatcher<char_type>>(m_matcher);
        if (nfa_matcher == nullptr)
            throw std::runtime_error("unexpected matcher type");

        auto nfa = nfa_matcher->get_nfa();
        auto dfa = std::make_shared<RegexDFA<char_type>>(nfa->compile());
        this->m_matcher = std::make_shared<DFAMatcher<char_type>>(dfa);
    }

    std::string to_string() const
    {
        auto nfa_matcher = std::dynamic_pointer_cast<NFAMatcher<char_type>>(m_matcher);
        auto dfa_matcher = std::dynamic_pointer_cast<DFAMatcher<char_type>>(m_matcher);

        if (nfa_matcher) {
            return nfa_matcher->to_string();
        } else if (dfa_matcher) {
            return dfa_matcher->to_string();
        } else {
            return "bad matcher";
        }
    }
};

#endif // _DC_PARSER_REGEX_INTERNAL_HPP_
