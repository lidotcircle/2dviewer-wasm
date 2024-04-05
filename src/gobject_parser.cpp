#include "gobject_parser.h"
#include <dcparse.hpp>
#include <lexer/lexer_rule_regex.hpp>
using namespace M2V;


inline static auto s2u(const std::string& str)
{
    return UTF8Decoder::strdecode(str);
}
inline static auto u2s(const std::vector<int>& cps)
{
    return UTF8Encoder::strencode(cps.begin(), cps.end());
}

struct TokenID: public LexerToken {
    std::string m_id;

    inline TokenID(std::string id): m_id(id) {}
};

struct TokenConstantInteger: public LexerToken {
    int64_t m_value;

    inline TokenConstantInteger(int64_t val): m_value(val) {}
};

struct TokenConstantFloat: public LexerToken {
    double m_value;

    inline TokenConstantFloat(double val): m_value(val) {}
};

struct TokenStringLiteral: public LexerToken {
    std::string m_value;

    inline TokenStringLiteral(std::string val): m_value(std::move(val)) {}
};

#define GOBJ_KEYWORD_LIST \
    K_ENTRY(if) \
    K_ENTRY(let) \
    K_ENTRY(loop)

#define K_ENTRY(n) \
    struct TokenKeyword_##n: public LexerToken { \
        inline TokenKeyword_##n(): LexerToken() {} \
    };
GOBJ_KEYWORD_LIST
#undef K_ENTRY


#define GOBJ_PUNCTUATOR_LIST \
    P_ENTRY(LPAREN, "\\(") \
    P_ENTRY(RPAREN, "\\)") \
    P_ENTRY(PLUS, "\\+") \
    P_ENTRY(MINUS, "\\-") \
    P_ENTRY(MULTIPLY, "\\*") \
    P_ENTRY(DIVISION, "/") \
    P_ENTRY(REMAINDER, "%") \
    P_ENTRY(LEFT_SHIFT, "<<") \
    P_ENTRY(RIGHT_SHIFT, ">>") \
    P_ENTRY(LESS_THAN, "<") \
    P_ENTRY(GREATER_THAN, ">") \
    P_ENTRY(LESS_EQUAL, "<=") \
    P_ENTRY(GREATER_EQUAL, ">=") \
    P_ENTRY(EQUAL, "==") \
    P_ENTRY(NOT_EQUAL, "\\!=") \
    P_ENTRY(BIT_XOR, "\\^") \
    P_ENTRY(BIT_OR, "\\|") \
    P_ENTRY(BIT_NOT, "~") \
    P_ENTRY(BIT_AND, "&") \
    P_ENTRY(LOGIC_AND, "&&") \
    P_ENTRY(LOGIC_OR, "\\|\\|")

#define P_ENTRY(n, regex) \
    struct TokenPunc##n: public LexerToken { \
        inline TokenPunc##n(): \
            LexerToken() {} \
    };
GOBJ_PUNCTUATOR_LIST
#undef P_ENTRY

static bool string_start_with(const std::string& str, const std::string& prefix)
{
    return str.compare(0, prefix.size(), prefix) == 0;
}
static TokenConstantInteger handle_integer_str(std::string str)
{
    unsigned long long value = 0;
    const bool is_hex = 
        string_start_with(str, "0x") 
        || string_start_with(str, "0X")
        || std::any_of(
            str.begin(), str.end(), 
            [](char c) { return ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F'); });
    assert(str.size() > 0);

    if (is_hex) 
    {
        if (string_start_with(str, "0x") ||
                string_start_with(str, "0X"))
        {
            str = str.substr(2);
            assert(str.size() > 0);
        }

        for (auto it = str.begin();it != str.end();it++) {
            const auto old_value = value;
            char c = *it;
            if (('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F')) {
                value = value * 16 + (c <= '9' ? c - '0' : (c <= 'F' ? c - 'A' + 10 : c - 'a' + 10));
            } else {
                assert(false && "bad hex string");
            }
            if (value < old_value)
                throw std::runtime_error("integer literal overflow");
        }
    }
    else if (string_start_with(str, "0b"))
    {
        str = str.substr(2);
        assert(str.size() > 0);

        for (auto it = str.begin();it != str.end();it++) {
            const auto old_value = value;
            char c = *it;
            if (c == '0' || c == '1') {
                value = value * 2 + (c - '0');
            } else {
                assert(false && "bad binary string");
            }
            if (value < old_value)
                throw std::runtime_error("integer literal overflow");
        }
    }
    else if (string_start_with(str, "0"))
    {
        if (str.size() > 1)
            str = str.substr(1);
        assert(str.size() > 0);

        for (auto it = str.begin();it != str.end();it++) {
            const auto old_value = value;
            char c = *it;
            if (c >= '0' && c <= '7') {
                value = value * 8 + (c - '0');
            } else {
                assert(false && "bad octal string");
            }
            if (value < old_value)
                throw std::runtime_error("integer literal overflow");
        }
    }
    else
    {
        for (auto it = str.begin();it != str.end();it++) {
            const auto old_value = value;
            char c = *it;
            if (c >= '0' && c <= '9') {
                value = value * 10 + (c - '0');
            } else {
                assert(false && "bad decimal string");
            }
            if (value < old_value)
                throw std::runtime_error("integer literal overflow");
        }
    }

     TokenConstantInteger ret(value);
     return ret;
}

static std::unique_ptr<Lexer<int>> createTokenizer()
{
    auto lexer = std::make_unique<Lexer<int>>();
    lexer->add_rule(
        std::make_unique<LexerRuleRegex<int>>(
            s2u("\"([^\\\\\"\n]|(\\\\[^\n]))*\""),
            [](auto str, auto info) {
            return std::make_shared<TokenStringLiteral>(
                    std::string(u2s(str)));
        }));

// keywords
#define K_ENTRY(kw) \
    lexer->add_rule( \
        std::make_unique<LexerRuleRegex<int>>( \
            s2u(#kw), \
            [](auto str, auto info) { \
                return std::make_shared<TokenKeyword_##kw>(); \
            }) \
    );
GOBJ_KEYWORD_LIST
#undef K_ENTRY

    lexer->dec_priority_minor();

    // identifier
    lexer->add_rule(
        std::make_unique<LexerRuleRegex<int>>(
            s2u("([a-zA-Z_]|\\\\0[uU][0-9a-fA-F]{4})([a-zA-Z0-9_]|\\\\0[uU][0-9a-fA-F]{4})*"),
            [](auto str, auto info) {
            return std::make_shared<TokenID>(
                    std::string(u2s(str)));
        })
    );


    lexer->dec_priority_major();


// punctuator
#define P_ENTRY(n, regex) \
    lexer->add_rule( \
        std::make_unique<LexerRuleRegex<int>>( \
            s2u(regex), \
            [](auto str, auto info) { \
                return std::make_shared<TokenPunc##n>(); \
            }) \
    );
GOBJ_PUNCTUATOR_LIST
#undef P_ENTRY

    lexer->dec_priority_minor();

    // integer literal
    lexer->add_rule(
        std::make_unique<LexerRuleRegex<int>>(
            s2u("0[0-7]*"),
            [](auto str, auto info) {
            return std::make_shared<TokenConstantInteger>(
                    handle_integer_str(u2s(str)));
        })
    );
    lexer->add_rule(
        std::make_unique<LexerRuleRegex<int>>(
            s2u("0b[01]+"),
            [](auto str, auto info) {
            return std::make_shared<TokenConstantInteger>(
                    handle_integer_str(u2s(str)));
        })
    );
    lexer->add_rule(
        std::make_unique<LexerRuleRegex<int>>(
            s2u("[1-9][0-9]*"),
            [](auto str, auto info) {
            return std::make_shared<TokenConstantInteger>(
                    handle_integer_str(u2s(str)));
        })
    );
    lexer->dec_priority_minor();
    lexer->add_rule(
        std::make_unique<LexerRuleRegex<int>>(
            s2u("(0[xX])?[0-9a-fA-F]+"),
            [](auto str, auto info) {
            return std::make_shared<TokenConstantInteger>(
                    handle_integer_str(u2s(str)));
        })
    );
    lexer->dec_priority_minor();
    lexer->add_rule(
        std::make_unique<LexerRuleRegex<int>>(
            s2u("[0-9]+[eE][\\+\\-]?[0-9]+[flFL]?"),
            [](auto str, auto info) {
            const long double value = std::stold(u2s(str));
            return std::make_shared<TokenConstantFloat>(value);
        })
    );
    lexer->add_rule(
        std::make_unique<LexerRuleRegex<int>>(
            s2u("((([0-9]+)?\\.[0-9]+)|[0-9]+\\.)([eE][\\+\\-]?[0-9]+)?[flFL]?"),
            [](auto str, auto info) {
            const long double value = std::stold(u2s(str));
            return std::make_shared<TokenConstantFloat>(value);
        })
    );

    return lexer;
}

class ASTNode {
public:
    virtual ~ASTNode() = default;
};

struct NonTermBasic: public NonTerminal {
    std::shared_ptr<ASTNode> m_astnode;
    inline NonTermBasic(std::shared_ptr<ASTNode> node): m_astnode(node) {}
};

static std::unique_ptr<DCParser> createParser()
{
    auto parser = std::make_unique<DCParser>();
    return parser;
}

GObjectParser::GObjectParser():
    m_lexer(createTokenizer()), m_parser(createParser())
{
}
