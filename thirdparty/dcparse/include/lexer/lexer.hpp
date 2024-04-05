#ifndef _LEXER_LEXER_HPP_
#define _LEXER_LEXER_HPP_

#include <vector>
#include <optional>
#include <string>
#include <assert.h>
#include <algorithm>
#include <functional>
#include "text_info.h"
#include "lexer_rule.hpp"
#include "lexer_error.h"
#include "regex/regex_char.hpp"


template<typename T>
class Lexer {
public:
    using CharType = T;
    using traits = character_traits<CharType>;
    using encoder_t = std::function<std::string(CharType)>;

private:
    class KLexerPositionInfo: public TextInfo
    {
    private:
        std::string _buffer;
        std::string _filename;
        std::vector<size_t> _linfo;
        using PInfo = TextInfo::PInfo;

    public:
        KLexerPositionInfo(std::string fn): _filename(fn) , _linfo({ 0 }) {}
        size_t push_str(const std::string& str) { _buffer += str; return this->len(); }
        void newline() { this->_linfo.push_back(this->_buffer.size()); }

        virtual const std::string& filename() const override { return _filename; }
        virtual size_t len() const override { return _buffer.size(); }
        virtual PInfo query(size_t pos) const override
        {
            if (pos >= _buffer.size())
                throw LexerError("query position out of range");

            auto up = std::upper_bound(this->_linfo.begin(), this->_linfo.end(), pos);
            auto line = this->_linfo.size();
            auto col = pos + 1;

            if (up != this->_linfo.end()) {
                line = std::distance(this->_linfo.begin(), up);
                assert(up != this->_linfo.begin());
                up--;
            } else {
                up = this->_linfo.end() - 1;
            }

            col = pos - *up + 1;
            return PInfo{.line = line, .column = col};
        }
        virtual std::pair<size_t,size_t> line_range(size_t line) const override {
            if (line > this->_linfo.size())
                throw LexerError("query line out of range");

            auto begin = this->_linfo[line - 1];
            auto end = this->_buffer.size();
            if (line < this->_linfo.size())
                end = this->_linfo[line];
            return std::make_pair(begin, end);
        }
        virtual std::string query_string(size_t from, size_t to) const override {
            if (from > to)
                throw LexerError("query string out of range");
            if (to > this->_buffer.size())
                throw LexerError("query string out of range");
            return this->_buffer.substr(from, to - from);
        }
    };
    std::shared_ptr<KLexerPositionInfo> m_textinfo;
    encoder_t m_encoder;

    static constexpr auto npos = std::string::npos;
    struct RuleInfo
    {
        std::unique_ptr<LexerRule<CharType>> rule;
        size_t feed_len, match_len;

        RuleInfo(std::unique_ptr<LexerRule<CharType>> rule)
            : rule(std::move(rule)), feed_len(0), match_len(0) {}

        void reset(size_t pos, std::optional<std::shared_ptr<LexerToken>> last)
        {
            this->rule->reset(pos, last);
            this->match_len = 0;
            this->feed_len = 0;
        }
    };
    // major priority => minor priority => rules
    std::vector<std::vector<std::vector<RuleInfo>>> m_rules;
    std::optional<size_t> m_match_major_priority;

    struct CharInfo {
        CharType char_val;
        size_t pos, len_in_bytes;

        CharInfo(CharType c, size_t pos, size_t len)
            : char_val(c), pos(pos), len_in_bytes(len) {}
    };
    std::vector<CharInfo> m_cache;
    size_t m_pos;
    std::string m_filename;

    std::vector<CharType> getcachestr(size_t len)
    {
        assert(len <= m_cache.size());
        std::vector<CharType> ret;
        for (size_t i = 0; i < len; ++i) {
            ret.push_back(m_cache[i].char_val);
        }
        return ret;
    }

    std::vector<std::shared_ptr<LexerToken>> push_cache_to_end(size_t cache_pos)
    {
        assert(cache_pos > 0 && cache_pos <= this->m_cache.size());
        std::vector<std::shared_ptr<LexerToken>> tokens;

        for (CharInfo ci = this->m_cache[cache_pos - 1];
             cache_pos <= this->m_cache.size();
               cache_pos++, 
               ci=(cache_pos <= this->m_cache.size()) ? this->m_cache[cache_pos-1] : ci)
        {
            auto [token, len] = this->feed_char_internal(ci);
            assert(len <= cache_pos);
            assert(token.has_value() || len == 0);

            if (token.has_value())
            {
                assert(len > 0);
                auto val = token.value();
                if (val != nullptr)
                    tokens.push_back(val);

                size_t reset_pos = this->m_pos;
                if (this->m_cache.size() > len)
                    reset_pos = this->m_cache[len].pos;
                this->reset_rules(reset_pos, val);
                this->m_cache.erase(this->m_cache.begin(),
                                    this->m_cache.begin() + len);
                cache_pos = 0;
            }
        }

        return tokens;
    }

    std::pair<std::optional<std::shared_ptr<LexerToken>>,size_t>
    feed_char_internal(const CharInfo& c)
    {
        bool prevs_is_dead = true;
        for (size_t i=0;i<this->m_rules.size();i++) {
            if (this->m_match_major_priority.has_value() && 
                this->m_match_major_priority.value() < i)
            {
                continue;
            }
            auto& r1 = this->m_rules[i];
            bool current_is_dead = true;
            std::vector<std::tuple<size_t,size_t,size_t>> finished_rules;

            for (size_t j=0;j<r1.size();j++) {
                auto& r2 = r1[j];

                for (size_t k=0;k<r2.size();k++) {
                    auto& ri = r2[k];
                    auto& r = *ri.rule;
                    if (r.dead()) {
                        if (ri.match_len > 0)
                            finished_rules.push_back(std::make_tuple(ri.match_len, j, k));
                        continue;
                    }

                    ri.feed_len++;
                    r.feed(c.char_val, c.len_in_bytes);
                    if (r.dead()) {
                        if (ri.match_len > 0)
                            finished_rules.push_back(std::make_tuple(ri.match_len, j, k));
                    } else {
                        current_is_dead = false;
                    }

                    if (r.match()) {
                        if (this->m_match_major_priority.has_value()) {
                            if (i < this->m_match_major_priority.value()) {
                                this->m_match_major_priority = i;
                            }
                        } else {
                            this->m_match_major_priority = i;
                        }

                        ri.match_len = ri.feed_len;
                    }
                }
            }
            prevs_is_dead = prevs_is_dead && current_is_dead;

            if (!prevs_is_dead || finished_rules.empty())
                continue;

            return this->get_token_by_candidates(i, finished_rules);
        }

        if (prevs_is_dead)
        {
            assert(!this->m_match_major_priority.has_value());
            throw std::runtime_error("no rule match '" + char_to_string(c.char_val) + "' at " + this->m_textinfo->row_col_str(c.pos));
        }

        return std::make_pair(std::nullopt, 0);
    }

    std::pair<std::optional<std::shared_ptr<LexerToken>>,size_t>
    feed_end_internal()
    {
        if (this->m_cache.empty())
            return std::make_pair(std::nullopt,0);

        for (size_t i=0;i<this->m_rules.size();i++) {
            if (this->m_match_major_priority.has_value() && 
                this->m_match_major_priority.value() > i)
            {
                continue;
            }
            auto& r1= this->m_rules[i];
            std::vector<std::tuple<size_t,size_t,size_t>> matchs;

            for (size_t j=0;j<r1.size();j++) {
                auto& r2 = r1[j];

                for (size_t k=0;k<r2.size();k++) {
                    auto& r = r2[k];
                    if (r.match_len > 0) {
                        matchs.push_back(std::make_tuple(r.match_len,j,k));
                    }
                }
            }

            if (matchs.empty())
                continue;

            return this->get_token_by_candidates(i, matchs);
        }

        throw LexerError(
                "unexpected end of file, unprocessed tokens: " +
                std::to_string(this->m_cache.size()));
    }

    std::pair<std::optional<std::shared_ptr<LexerToken>>,size_t>
    get_token_by_candidates(size_t major_index, std::vector<std::tuple<size_t,size_t,size_t>>& candidates)
    {
        assert(candidates.size() > 0);
        std::sort(candidates.rbegin(), candidates.rend());
        auto f1 = candidates.front();
        for (auto& a: candidates) {
            if (std::get<0>(a) != std::get<0>(f1))
                break;

            if (std::get<1>(a) == std::get<1>(f1) &&
                std::get<1>(a) == std::get<1>(f1))
            {
                continue;
            }

            if (std::get<1>(a) == std::get<1>(f1))
            {
                throw LexerError("conflict rule");
            } else if (std::get<1>(a) < std::get<1>(f1))
            {
                f1 = a;
            }
        }

        assert(major_index < this->m_rules.size());
        auto& r1 = this->m_rules[major_index];
        assert(r1.size() > std::get<1>(f1));
        auto& ra = r1[std::get<1>(f1)];
        assert(ra.size() > std::get<2>(f1));
        auto& ri = ra[std::get<2>(f1)];
        assert(ri.rule != nullptr);
        auto& rule = *ri.rule;
        auto str = this->getcachestr(std::get<0>(f1));
        return std::make_pair(rule.token(str), std::get<0>(f1));
    }

    std::optional<std::shared_ptr<LexerToken>> m_notnull_last_token;
    void reset_rules(size_t pos, std::optional<std::shared_ptr<LexerToken>> last)
    {
        if (last.has_value() && last.value() != nullptr)
            this->m_notnull_last_token = last;

        for (auto& r1: this->m_rules) {
            for (auto& r2: r1) {
                for (auto& ri: r2) {
                    ri.reset(pos, this->m_notnull_last_token);
                }
            }
        }
        this->m_match_major_priority = std::nullopt;
    }

    virtual void update_position_info(CharType c)
    {
        std::string str;
        if (this->m_encoder) {
            str = this->m_encoder(c);
        } else {
            str.push_back((char)c);
        }
        this->m_pos = this->m_textinfo->push_str(str);

        if (c == traits::NEWLINE)
            this->m_textinfo->newline();
    }

    void setup_rules_set()
    {
        this->m_rules.resize(1);
        this->m_rules.back().resize(1);
    }


public:
    Lexer(encoder_t encoder = nullptr)
        : m_encoder(encoder)
    {
        this->setup_rules_set();
        this->reset();
    }
    Lexer(const std::string& fn, encoder_t encoder = nullptr)
        : m_encoder(encoder)
    {
        this->setup_rules_set();
        this->reset(fn);
    }
    ~Lexer() = default;

    Lexer(const Lexer&) = delete;
    Lexer(Lexer&&) = default;

    void reset(const std::string& fn)
    {
        this->m_filename = fn;
        this->m_cache.clear();
        this->m_match_major_priority = std::nullopt;
        this->m_pos = 0;
        this->reset_rules(0, std::nullopt);
        this->m_textinfo = std::make_shared<KLexerPositionInfo>(this->m_filename);
    }

    void reset()
    {
        this->reset(this->m_filename);
    }

    void dec_priority_major()
    {
        this->m_rules.resize(this->m_rules.size() + 1);
        this->m_rules.back().resize(1);
    }

    void dec_priority_minor()
    {
        assert(!this->m_rules.empty());
        auto& back = this->m_rules.back();
        back.resize(back.size() + 1);
    }

    void add_rule(std::unique_ptr<LexerRule<CharType>> rule) {
        assert(!this->m_rules.empty());
        auto& back = this->m_rules.back();
        assert(!back.empty());
        auto& ruleset = back.back();

        ruleset.push_back(RuleInfo(std::move(rule)));
    }

    Lexer& operator()(std::unique_ptr<LexerRule<CharType>> rule) {
        this->add_rule(std::move(rule));
        return *this;
    }

    std::vector<std::shared_ptr<LexerToken>> feed_char(CharType c)
    {
        if (this->m_pos == 0)
            this->reset_rules(0, std::nullopt);

        const auto old_pos = this->m_pos;
        this->update_position_info(c);
        assert(this->m_pos > old_pos);
        this->m_cache.push_back(CharInfo(c, old_pos, this->m_pos - old_pos));
        return this->push_cache_to_end(this->m_cache.size());
    }

    template<typename Iterator>
    std::vector<std::shared_ptr<LexerToken>> feed_char(Iterator begin, Iterator end)
    {
        std::vector<std::shared_ptr<LexerToken>> ret;

        for (;begin != end;begin++) {
            auto r = this->feed_char(*begin);
            ret.insert(ret.end(), r.begin(), r.end());
        }

        return ret;
    }

    template<typename Container>
    std::vector<std::shared_ptr<LexerToken>> feed_char(const Container& c)
    {
        return this->feed_char(c.begin(), c.end());
    }

    std::vector<std::shared_ptr<LexerToken>> feed_end()
    {
        std::vector<std::shared_ptr<LexerToken>> tokens;

        while (!this->m_cache.empty()) {
            auto [token, len] = this->feed_end_internal();
            assert(len <= this->m_cache.size());
            assert(token.has_value() || len == 0);

            if (token.has_value()) {
                assert(len > 0);
                auto val = token.value();
                if (val != nullptr)
                    tokens.push_back(val);

                size_t reset_pos = this->m_pos;
                if (this->m_cache.size() > len)
                    reset_pos = this->m_cache[len].pos;
                this->reset_rules(reset_pos, val);
                this->m_cache.erase(this->m_cache.begin(),
                                    this->m_cache.begin() + len);
                
                if (!this->m_cache.empty()) {
                    auto nx = this->push_cache_to_end(1);
                    for (auto t: nx)
                        tokens.push_back(t);
                }
            }
        }

        return tokens;
    }

    std::shared_ptr<TextInfo> position_info() const { return this->m_textinfo; }
};

#endif // _LEXER_LEXER_HPP_
