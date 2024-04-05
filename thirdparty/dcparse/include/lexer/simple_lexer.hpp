#ifndef _LEXER_SIMPLE_LEXER_HPP_
#define _LEXER_SIMPLE_LEXER_HPP_

#include "lexer.hpp"
#include "lexer_error.h"
#include <string>
#include <memory>
#include <vector>
#include <assert.h>


class ISimpleLexer
{
public:
    using token_t = std::shared_ptr<LexerToken>;

    virtual bool end() const = 0;
    virtual token_t next() = 0;
    virtual void back() = 0;
    virtual ~ISimpleLexer() = default;
};


template<typename T>
class SimpleLexer: public ISimpleLexer {
public:
    using CharType = T;
    using token_t = std::shared_ptr<LexerToken>;

private:
    std::unique_ptr<Lexer<CharType>> lexer;
    std::vector<token_t> token_buffer;
    size_t cur_pos;
    std::vector<CharType> buffer;
    size_t buf_pos;

    void clean_token_buffer()
    {
        while (cur_pos >= 0 && token_buffer.size() > 10)
        {
            token_buffer.erase(token_buffer.begin());
            cur_pos--;
        }
    }

    void fill_one()
    {
        assert(cur_pos <= token_buffer.size());
        assert(buf_pos <= buffer.size());

        if (cur_pos < token_buffer.size() || buf_pos == buffer.size())
            return;

        while(buf_pos<buffer.size()) {
            auto c = buffer[buf_pos];
            buf_pos++;

            auto tokens = lexer->feed_char(c);
            if (!tokens.empty()) {
                token_buffer.insert(token_buffer.end(), tokens.begin(), tokens.end());
                this->clean_token_buffer();
                break;
            }
        }

        if (buf_pos == buffer.size()) {
            auto tokens = lexer->feed_end();
            if (!tokens.empty()) {
                token_buffer.insert(token_buffer.end(), tokens.begin(), tokens.end());
                this->clean_token_buffer();
            }
        }
    }

public:
    SimpleLexer(std::unique_ptr<Lexer<CharType>> lexer, std::vector<CharType> buf):
        lexer(std::move(lexer)), buffer(std::move(buf)), buf_pos(0), cur_pos(0) 
    {
        assert(buffer.size() > 0);
    }

    bool end() const override {
        auto _this = const_cast<SimpleLexer*>(this);
        _this->fill_one();

        assert(cur_pos <= token_buffer.size());
        return cur_pos == this->token_buffer.size();
    }

    token_t next() override {
        if (this->end())
            throw LexerError("No more tokens");

        return token_buffer[cur_pos++];
    }

    void back() override {
        if (this->cur_pos == 0)
            throw LexerError("lexer can't go backward");

        this->cur_pos--;
    }
};

#endif // _LEXER_SIMPLE_LEXER_HPP_
