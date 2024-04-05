#ifndef _DC_PARSER_REGEX_EXPR_HPP_
#define _DC_PARSER_REGEX_EXPR_HPP_

#include <vector>
#include <stdexcept>
#include <assert.h>
#include "./regex_char.hpp"


template<typename CharT>
class RegexPatternEscaper {
private:
    using traits = character_traits<CharT>;
    using char_type = CharT;
    using regex_char = RegexPatternChar<char_type>;

public:
    RegexPatternEscaper() = delete;

    template<typename Iterator>
    static std::vector<regex_char> convert(Iterator first, Iterator last)
    {
        std::vector<regex_char> result;
        bool escaped = false;

        for (; first != last; ++first) {
            if (escaped) {
                escaped = false;
                result.push_back(regex_char::escape(*first));
            } else {
                if (*first == traits::BACKSLASH) {
                    escaped = true;
                } else {
                    result.push_back(regex_char::unescape(*first));
                }
            }
        }

        if (escaped)
            throw std::invalid_argument("unterminated escape sequence");

        return result;
    }

    template<typename Iterator>
    static std::vector<char_type> convertback(Iterator first, Iterator last)
    {
        std::vector<char_type> result;

        for (; first != last; ++first) {
            auto c = *first;

            if (c.is_escaped()) {
                result.push_back(traits::BACKSLASH);
            }

            result.push_back(c.get());
        }

        return result;
    }
};

template<typename CharT>
class Regex2BasicConvertor {
private:
    using traits = character_traits<CharT>;
    using char_type = CharT;
    using regex_char = RegexPatternChar<char_type>;
    std::vector<size_t> m_lparen_pos;
    std::vector<regex_char> __result;
    bool endding;
    size_t m_last_group_pos;
    static constexpr size_t npos = std::numeric_limits<size_t>::max();
    bool m_bracket_mode;

    bool m_in_brace_mode;
    bool m_brace_got_comma;
    int  m_brace_count_low;
    int  m_brace_count_up;
    static constexpr size_t brace_infinity = std::numeric_limits<decltype(m_brace_count_up)>::max();
    std::vector<regex_char> __copy_content;

    std::vector<regex_char> get_last_node(size_t last_group_pos)
    {
        if (this->__result.empty())
            throw std::runtime_error("unexpected {} / + / ?");

        if (last_group_pos == npos) {
            return std::vector<regex_char>({  this->__result.back() });
        } else {
            assert(this->__result.back().get() == traits::RPAREN || this->__result.back().get() == traits::RBRACKET);
            std::vector<regex_char> ans;
            std::copy(this->__result.begin() + last_group_pos, this->__result.end(), std::back_inserter(ans));
            return ans;
        }
    }

    void enter_brace_mode() {
        this->m_brace_count_low = 0;
        this->m_brace_count_up = 0;
        this->m_in_brace_mode = true;
        this->m_brace_got_comma = false;
    }

    bool handle_brace_mode(regex_char _c) {
        if (!this->m_in_brace_mode)
            return false;

        if (_c.is_escaped())
            throw std::runtime_error("unexpected escape sequence in brace");
        auto c =  _c.get();

        if (c == traits::COMMA) {
            if (this->m_brace_got_comma)
                throw std::runtime_error("unexpected , in brace");
            this->m_brace_got_comma = true;
            this->m_brace_count_up=brace_infinity;
            return true;
        } else if (c == traits::RBRACE) {
            this->__result.resize(this->__result.size() - this->__copy_content.size());
            for (size_t i=0; i<this->m_brace_count_low; i++) {
                this->__result.insert(this->__result.end(), this->__copy_content.begin(), this->__copy_content.end());
            }

            if (this->m_brace_got_comma) {
                if (this->m_brace_count_up < this->m_brace_count_low)
                    throw std::runtime_error("range error");

                if (this->m_brace_count_up == brace_infinity) {
                    this->__result.insert(this->__result.end(), this->__copy_content.begin(), this->__copy_content.end());
                    this->__result.push_back(regex_char::unescape(traits::STAR));
                } else {
                    const auto opt_count = this->m_brace_count_up - this->m_brace_count_low;
                    for (size_t i=0; i<opt_count; i++) {
                        this->__result.push_back(regex_char::unescape(traits::LPAREN));
                        this->__result.push_back(regex_char::unescape(traits::OR));
                        this->__result.insert(this->__result.end(), this->__copy_content.begin(), this->__copy_content.end());
                    }
                    std::vector<regex_char> nrparen(opt_count, regex_char::unescape(traits::RPAREN));
                    this->__result.insert(this->__result.end(), nrparen.begin(), nrparen.end());
                }
            }

            this->m_in_brace_mode = false;
            return true;
        }

        if (c < traits::ZERO || c > traits::NINE)
            throw std::runtime_error("brace mode: expect number, bug get '" + char_to_string(c) + "'");

        if (this->m_brace_got_comma) {
            if (this->m_brace_count_up == brace_infinity)
                this->m_brace_count_up = 0;

            this->m_brace_count_up *= 10;
            this->m_brace_count_up += c - traits::ZERO;
        } else {
            this->m_brace_count_low *= 10;
            this->m_brace_count_low += c - traits::ZERO;
        }

        return true;
    }

public:
    Regex2BasicConvertor():
        endding(false),
        m_bracket_mode(false),
        m_last_group_pos(npos),
        m_in_brace_mode(false) {}

    void feed(regex_char c) {
        assert(!this->endding);
        auto last_group_pos = this->m_last_group_pos;
        this->m_last_group_pos = npos;

        if (this->handle_brace_mode(c))
            return;

        auto _c = c.get();
        if (c.is_escaped()) {

            switch (_c) {
                case traits::LBRACE:
                case traits::COMMA:
                case traits::RBRACE:
                case traits::QUESTION:
                case traits::PLUS:
                case traits::DOT:
                    this->__result.push_back(regex_char::unescape(_c));
                    break;
                default:
                    this->__result.push_back(c);
                    break;
            }

            return;
        }

        if (this->m_bracket_mode && _c != traits::RBRACKET) {
            this->__result.push_back(c);
            return;
        }

        switch (_c) {
            case traits::LBRACKET:
                this->m_bracket_mode = true;
            case traits::LPAREN:
                this->m_lparen_pos.push_back(this->__result.size());
                this->__result.push_back(c);
                break;
            case traits::RBRACKET:
                this->m_bracket_mode = false;
            case traits::RPAREN:
                if (this->m_lparen_pos.empty())
                    throw std::runtime_error("unmatched right parenthese");
                this->__result.push_back(c);
                this->m_last_group_pos = this->m_lparen_pos.back();
                this->m_lparen_pos.pop_back();
                break;
            case traits::LBRACE:
                this->enter_brace_mode();
                this->__copy_content = this->get_last_node(last_group_pos);
                break;
            case traits::RBRACE:
                throw std::runtime_error("unexpected rbrace");
                break;
            case traits::QUESTION:
            {
                auto last_node = this->get_last_node(last_group_pos);
                assert(this->__result.size() >= last_node.size());
                this->__result.resize(this->__result.size() - last_node.size());
                this->__result.push_back(regex_char::unescape(traits::LPAREN));
                this->__result.push_back(regex_char::unescape(traits::OR));
                this->__result.insert(this->__result.end(), last_node.begin(), last_node.end());
                this->__result.push_back(regex_char::unescape(traits::RPAREN));
            } break;
            case traits::PLUS: 
            {
                auto last_node = this->get_last_node(last_group_pos);
                this->__result.insert(this->__result.end(), last_node.begin(), last_node.end());
                this->__result.push_back(regex_char::unescape(traits::STAR));
            } break;
            case traits::DOT:
            {
                this->__result.push_back(regex_char::unescape(traits::LBRACKET));
                this->__result.push_back(regex_char::unescape(traits::MIN));
                this->__result.push_back(regex_char::unescape(traits::DASH));
                this->__result.push_back(regex_char::unescape(traits::MAX));
                this->__result.push_back(regex_char::unescape(traits::RBRACKET));
            } break;
            default:
                this->__result.push_back(c);
                break;
        }
    }

    std::vector<regex_char>& end() {
        assert(!this->endding);
        this->endding = true;

        return __result;
    }

    static std::vector<regex_char> convert(const std::vector<regex_char>& regex) {
        Regex2BasicConvertor conv;
        for (auto c : regex)
            conv.feed(c);
        return conv.end();
    }

    static std::vector<char_type> convert(const std::vector<char_type>& regex) {
        auto f1 = RegexPatternEscaper<char_type>::convert(regex.begin(), regex.end());
        auto f2 = Regex2BasicConvertor::convert(f1);
        return RegexPatternEscaper<char_type>::convertback(f2.begin(), f2.end());
    }
};

#endif // _DC_PARSER_REGEX_EXPR_H_
