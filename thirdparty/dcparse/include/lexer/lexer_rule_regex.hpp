#ifndef _LEXER_LEXER_RULE_REGEX_HPP_
#define _LEXER_LEXER_RULE_REGEX_HPP_

#include "./token.h"
#include "./lexer_rule.hpp"
#include "../regex/regex.hpp"
#include <functional>
#include <vector>


template<typename T>
class LexerRuleRegex: public LexerRule<T> {
public:
    using CharType = T;
    using DeterType = std::function<bool(std::optional<std::shared_ptr<LexerToken>>)>;

private:
    using string_t = std::vector<CharType>;
    using token_factory_t = std::function<std::shared_ptr<LexerToken>(std::vector<CharType> str, TextRange)>;
    bool m_resetted;
    TextRange m_range;
    string_t m_string;
    SimpleRegExp<CharType> m_regex;
    token_factory_t m_token_factory;
    DeterType m_deter;

    bool _opt_compile, _opt_first_match;
    bool match_dead;
    void apply_options(bool compile, bool first_match)
    {
        this->_opt_compile = compile;
        this->_opt_first_match = first_match;
        this->match_dead = false;

        if (this->_opt_compile)
            this->m_regex.compile();
    }

public:
    LexerRuleRegex() = delete;
    template<typename Iterator>
    LexerRuleRegex(
            Iterator begin, Iterator end, token_factory_t factory,
            bool compile = true, bool first_match = false,
            DeterType deter = nullptr): 
        m_resetted(false), m_regex(begin, end), m_token_factory(factory), m_deter(deter)
    {
        this->apply_options(compile, first_match);
    }

    LexerRuleRegex(
            const std::basic_string<CharType>& regex, token_factory_t factory,
            bool compile = true, bool first_match = false,
            DeterType deter = nullptr): 
        m_regex(std::vector<CharType>(regex.begin(), regex.end())), m_token_factory(factory),
        m_deter(deter)
    {
        this->apply_options(compile, first_match);
    }

    LexerRuleRegex(
            const std::vector<CharType>& regex, token_factory_t factory,
            bool compile = true, bool first_match = false,
            DeterType deter = nullptr): 
        m_regex(std::vector<CharType>(regex.begin(), regex.end())), m_token_factory(factory),
        m_deter(deter)
    {
        this->apply_options(compile, first_match);
    }

    virtual void feed(CharType c, size_t length_in_bytes) override {
        if (this->_opt_first_match && this->m_regex.match()) {
            this->match_dead = true;
            return;
        }

        this->m_regex.feed(c);
        if (!this->dead()) {
            this->m_string.push_back(c);
            this->m_range.second += length_in_bytes;
        }
    }
    virtual bool dead() override {
        return this->match_dead || this->m_regex.dead();
    }
    virtual bool match() override {
        return !this->match_dead && this->m_regex.match();
    };

    virtual void reset(size_t pos, std::optional<std::shared_ptr<LexerToken>> last) override
    {
        this->m_resetted = true;
        this->m_range.first = pos;
        this->m_range.second = pos;
        this->m_regex.reset();
        this->m_string.clear();
        this->match_dead = false;

        if (this->m_deter && !this->m_deter(last))
            this->match_dead = true;
    }

    virtual std::shared_ptr<LexerToken> token(std::vector<CharType> str) override {
        assert(this->m_resetted);
        return this->m_token_factory(this->m_string, this->m_range);
    }
};

#endif // _LEXER_LEXER_RULE_REGEX_HPP_
