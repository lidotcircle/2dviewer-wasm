#ifndef _LEXER_LEXER_RULE_CSTRING_LITERAL_HPP_
#define _LEXER_LEXER_RULE_CSTRING_LITERAL_HPP_

#include "./lexer_rule.hpp"
#include "regex/regex_char.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>


template<typename T>
class LexerRuleCStringLiteral: public LexerRule<T> {
public:
    using CharType = T;
    using token_factory_t = std::function<std::shared_ptr<LexerToken>(std::vector<CharType> str, TextRange)>;

private:
    using traits = character_traits<CharType>;
    std::vector<CharType> _literal;
    size_t _length_in_bytes;
    enum MatchState {
        MATCH_STATE_NONE,
        MATCH_STATE_NORMAL,
        MATCH_STATE_ESCAPING,
        MATCH_STATE_END,
        MATCH_STATE_DEAD,
    } _state;
    TextRange _token_info;
    token_factory_t _token_factory;

    static const std::map<CharType,CharType> _escape_map;

public:
    LexerRuleCStringLiteral() = delete;
    LexerRuleCStringLiteral(token_factory_t factory):
        _length_in_bytes(0),
        _state(MATCH_STATE_NONE), _token_factory(factory)
    {
    }

    virtual void feed(CharType c, size_t length_in_bytes) override
    {
        if (this->_state == MATCH_STATE_END)
            return;

        switch (this->_state) {
        case MATCH_STATE_NONE:
            if (c == traits::DQUOTE) {
                this->_state = MATCH_STATE_NORMAL;
            } else {
                this->_state = MATCH_STATE_DEAD;
            }
            this->_token_info.second += length_in_bytes;
            break;
        case MATCH_STATE_NORMAL:
            if (c == traits::DQUOTE) {
                this->_state = MATCH_STATE_END;
            } else if (c == traits::BACKSLASH) {
                this->_state = MATCH_STATE_ESCAPING;
            } else if (c == traits::NEWLINE) {
                this->_state = MATCH_STATE_DEAD;
            } else {
                this->_literal.push_back(c);
            }
            this->_token_info.second += length_in_bytes;
            break;
        case MATCH_STATE_ESCAPING:
            if (_escape_map.find(c) == _escape_map.end()) {
                this->_state = MATCH_STATE_DEAD;
            } else {
                this->_literal.push_back(_escape_map.at(c));
                this->_state = MATCH_STATE_NORMAL;
            }
            this->_token_info.second += length_in_bytes;
            break;
        case MATCH_STATE_END:
        case MATCH_STATE_DEAD:
            this->_state = MATCH_STATE_DEAD;
        }
    }
    virtual bool dead() override {
        return this->_state == MATCH_STATE_DEAD;
    }
    virtual bool match() override {
        return this->_state == MATCH_STATE_END;
    }

    virtual void reset(size_t pos, std::optional<std::shared_ptr<LexerToken>> last) override
    {
        this->_state = MATCH_STATE_NONE;
        this->_literal.clear();
        this->_token_info.first = pos;
        this->_token_info.second = pos;
    }

    virtual std::shared_ptr<LexerToken> token(std::vector<CharType> str) override
    {
        return this->_token_factory(
                std::vector<CharType>(
                    this->_literal.begin(),
                    this->_literal.end()),
                this->_token_info);
    }
};

template<typename T>
const std::map<T,T> LexerRuleCStringLiteral<T>::_escape_map = {
    { traits::DQUOTE, traits::DQUOTE },
    { traits::BACKSLASH, traits::BACKSLASH },
    { traits::LOWER_N, traits::NEWLINE },
    { traits::LOWER_R, traits::RETURN },
    { traits::LOWER_T, traits::TAB },
};

#endif // _LEXER_LEXER_RULE_CSTRING_LITERAL_HPP_
