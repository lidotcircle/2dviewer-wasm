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
    K_ENTRY(let) \
    K_ENTRY(def)

#define K_ENTRY(n) \
    struct TokenKeyword_##n: public LexerToken { \
        inline TokenKeyword_##n(): LexerToken() {} \
    };
GOBJ_KEYWORD_LIST
#undef K_ENTRY


#define GOBJ_PUNCTUATOR_LIST(P_ENTRY) \
    P_ENTRY(LPAREN, "\\(") \
    P_ENTRY(RPAREN, "\\)")

#define GOBJ_BINARY_OPS(P_ENTRY) \
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
GOBJ_PUNCTUATOR_LIST(P_ENTRY)
GOBJ_BINARY_OPS(P_ENTRY)
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
GOBJ_PUNCTUATOR_LIST(P_ENTRY)
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

class ASTExprNode: public ASTNode { };

class ASTExprListNode: public ASTNode {
public:
    ASTExprListNode() {}

    std::vector<std::shared_ptr<ASTExprNode>> m_exprs;
};

class ASTIDListNode: public ASTNode {
public:
    ASTIDListNode() {}

    std::vector<std::string> m_ids;
};

class ASTFuncExprNode: public ASTExprNode {
public:
    ASTFuncExprNode(const std::string& func, std::vector<std::shared_ptr<ASTExprNode>> args):
        m_func(func), m_args(args) {}

private:
    std::string m_func;
    std::vector<std::shared_ptr<ASTExprNode>> m_args;
};

class ASTFuncDefExprNode: public ASTExprNode {
public:
    ASTFuncDefExprNode(const std::string& funcname, std::vector<std::string> parameters, std::vector<std::shared_ptr<ASTExprNode>> exprs):
        m_funcname(), m_parameters(parameters), m_exprs(exprs) {}

private:
    std::string m_funcname;
    std::vector<std::string> m_parameters;
    std::vector<std::shared_ptr<ASTExprNode>> m_exprs;
};

class ASTMinusExprNode: public ASTExprNode {
public:
    ASTMinusExprNode(std::shared_ptr<ASTExprNode> expr):
        m_expr(expr) {}

public:
    std::shared_ptr<ASTExprNode> m_expr;
};

class ASTLetExprNode: public ASTExprNode {
public:
    ASTLetExprNode(const std::string& id, std::shared_ptr<ASTExprNode> expr):
        m_id(id), m_expr(expr) {}

public:
    std::string m_id;
    std::shared_ptr<ASTExprNode> m_expr;
};

class ASTBinaryOpExprNode: public ASTExprNode {
public:
    ASTBinaryOpExprNode(const std::string& op, std::shared_ptr<ASTExprNode> left, std::shared_ptr<ASTExprNode> right):
        m_op(op), m_left(left), m_right(right) {}

public:
    std::string m_op;
    std::shared_ptr<ASTExprNode> m_left, m_right;
};

class ASTIntExprNode: public ASTExprNode {
public:
    ASTIntExprNode(int64_t val): m_value(val) {}

private:
    int64_t m_value;
};

class ASTFloatExprNode: public ASTExprNode {
public:
    ASTFloatExprNode(double val): m_value(val) {}

private:
    double m_value;
};

class ASTStringExprNode: public ASTExprNode {
public:
    ASTStringExprNode(const std::string& val): m_value(val) {}

private:
    std::string m_value;
};

class ASTIDExprNode: public ASTExprNode {
public:
    ASTIDExprNode(const std::string& id): m_id(id) {}

private:
    std::string m_id;
};

struct NonTermBasic: public NonTerminal {
    std::shared_ptr<ASTNode> m_astnode;
    inline NonTermBasic(std::shared_ptr<ASTNode> node): m_astnode(node) {}
};

#define NONTERMS \
    TENTRY(EXPRESSION) \
    TENTRY(ID_LIST) \
    TENTRY(EXPRESSION_LIST) \
    TENTRY(MODULE)

#define TENTRY(n) \
    struct NonTerm##n: public NonTermBasic { \
        inline NonTerm##n(std::shared_ptr<ASTNode> node): NonTermBasic(node) {} \
    };
NONTERMS
#undef TENTRY

#define NI(t) CharInfo<NonTerm##t>()
#define TI(t) CharInfo<Token##t>()
#define KW(t) CharInfo<TokenKeyword_##t>()
#define PT(t) CharInfo<TokenPunc##t>()

static std::unique_ptr<DCParser> createParser()
{
    auto parserPtr = std::make_unique<DCParser>();
    auto& parser = *parserPtr;
    parser(NI(EXPRESSION), {PT(LPAREN), KW(let), TI(ID), NI(EXPRESSION), PT(RPAREN)}, [](auto c, auto ts) {
        assert(ts.size() == 5);
        const std::shared_ptr<TokenID> id = std::dynamic_pointer_cast<TokenID>(ts.at(2));
        const auto expr = std::dynamic_pointer_cast<NonTermEXPRESSION>(ts.at(3));
        const auto exprNode = std::dynamic_pointer_cast<ASTExprNode>(expr->m_astnode);
        return std::make_shared<NonTermEXPRESSION>(std::make_shared<ASTLetExprNode>(id->m_id, exprNode));
    });
    parser(NI(EXPRESSION), {PT(MINUS), NI(EXPRESSION)}, [](auto c, auto ts) {
        assert(ts.size() == 2);
        const auto expr = std::dynamic_pointer_cast<NonTermEXPRESSION>(ts.at(1));
        const auto exprNode = std::dynamic_pointer_cast<ASTExprNode>(expr->m_astnode);
        return std::make_shared<NonTermEXPRESSION>(std::make_shared<ASTMinusExprNode>(exprNode));
    });
    parser(NI(EXPRESSION), {TI(ConstantFloat)}, [](auto c, auto ts) {
        assert(ts.size() == 1);
        const std::shared_ptr<TokenConstantFloat> expr = std::dynamic_pointer_cast<TokenConstantFloat>(ts.at(0));
        return std::make_shared<NonTermEXPRESSION>(std::make_shared<ASTFloatExprNode>(expr->m_value));
    });
    parser(NI(EXPRESSION), {TI(ConstantInteger)}, [](auto c, auto ts) {
        assert(ts.size() == 1);
        const std::shared_ptr<TokenConstantInteger> expr = std::dynamic_pointer_cast<TokenConstantInteger>(ts.at(0));
        return std::make_shared<NonTermEXPRESSION>(std::make_shared<ASTIntExprNode>(expr->m_value));
    });
    parser(NI(EXPRESSION), {TI(StringLiteral)}, [](auto c, auto ts) {
        assert(ts.size() == 1);
        const std::shared_ptr<TokenStringLiteral> expr = std::dynamic_pointer_cast<TokenStringLiteral>(ts.at(0));
        return std::make_shared<NonTermEXPRESSION>(std::make_shared<ASTStringExprNode>(expr->m_value));
    });
    parser(NI(EXPRESSION), {TI(ID)}, [](auto c, auto ts) {
        assert(ts.size() == 1);
        const std::shared_ptr<TokenID> expr = std::dynamic_pointer_cast<TokenID>(ts.at(0));
        return std::make_shared<NonTermEXPRESSION>(std::make_shared<ASTStringExprNode>(expr->m_id));
    });

#define P_ENTRY(n, s) \
    parser(NI(EXPRESSION), {PT(LPAREN), PT(n), NI(EXPRESSION), NI(EXPRESSION), PT(RPAREN)}, [](auto c, auto ts) { \
        assert(ts.size() == 5); \
        const auto expr1 = std::dynamic_pointer_cast<NonTermEXPRESSION>(ts.at(2)); \
        const auto expr2 = std::dynamic_pointer_cast<NonTermEXPRESSION>(ts.at(3)); \
        const auto exprNode1 = std::dynamic_pointer_cast<ASTExprNode>(expr1->m_astnode); \
        const auto exprNode2 = std::dynamic_pointer_cast<ASTExprNode>(expr2->m_astnode); \
        return std::make_shared<NonTermEXPRESSION>(std::make_shared<ASTBinaryOpExprNode>(s, exprNode1, exprNode2)); \
    });
    GOBJ_BINARY_OPS(P_ENTRY)
#undef BINARY_OPS

    parser(NI(EXPRESSION_LIST), {NI(EXPRESSION)}, [](auto c, auto ts) {
        assert(ts.size() == 1);
        const auto expr = std::dynamic_pointer_cast<NonTermEXPRESSION>(ts.at(0));
        const std::shared_ptr<ASTExprNode> exprNode = std::dynamic_pointer_cast<ASTExprNode>(expr->m_astnode);
        auto list = std::make_shared<ASTExprListNode>();
        list->m_exprs.push_back(exprNode);
        return std::make_shared<NonTermEXPRESSION_LIST>(list);
    });

    parser(NI(EXPRESSION_LIST), {NI(EXPRESSION_LIST), NI(EXPRESSION)}, [](auto c, auto ts) {
        assert(ts.size() == 1);
        const auto listt = std::dynamic_pointer_cast<NonTermEXPRESSION_LIST>(ts.at(0));
        const auto list = std::dynamic_pointer_cast<ASTExprListNode>(listt->m_astnode);
        const auto expr = std::dynamic_pointer_cast<NonTermEXPRESSION>(ts.at(1));
        const std::shared_ptr<ASTExprNode> exprNode = std::dynamic_pointer_cast<ASTExprNode>(expr->m_astnode);
        list->m_exprs.push_back(exprNode);
        return std::make_shared<NonTermEXPRESSION_LIST>(list);
    });

    parser(NI(EXPRESSION), {PT(LPAREN), TI(ID), ParserChar::beOptional(NI(EXPRESSION_LIST)), PT(RPAREN)}, [](auto c, auto ts) {
        assert(ts.size() == 4);
        const std::shared_ptr<TokenID> id = std::dynamic_pointer_cast<TokenID>(ts.at(1));
        const auto exprList = std::dynamic_pointer_cast<NonTermEXPRESSION_LIST>(ts.at(2));
        const auto exprListNode = std::dynamic_pointer_cast<ASTExprListNode>(exprList);
        const auto exprs = exprListNode ? exprListNode->m_exprs : std::vector<std::shared_ptr<ASTExprNode>>();
        return std::make_shared<NonTermEXPRESSION>(std::make_shared<ASTFuncExprNode>(id->m_id, exprs));
    });

    parser(NI(ID_LIST), {TI(ID)}, [](auto c, auto ts) {
        assert(ts.size() == 1);
        const auto id = std::dynamic_pointer_cast<TokenID>(ts.at(0));
        auto list = std::make_shared<ASTIDListNode>();
        list->m_ids.push_back(id->m_id);
        return std::make_shared<NonTermID_LIST>(list);
    });

    parser(NI(ID_LIST), {NI(ID_LIST), TI(ID)}, [](auto c, auto ts) {
        assert(ts.size() == 1);
        const auto listt = std::dynamic_pointer_cast<NonTermID_LIST>(ts.at(0));
        const auto list = std::dynamic_pointer_cast<ASTIDListNode>(listt->m_astnode);
        const auto id = std::dynamic_pointer_cast<TokenID>(ts.at(1));
        list->m_ids.push_back(id->m_id);
        return std::make_shared<NonTermEXPRESSION_LIST>(list);
    });

    parser(NI(EXPRESSION), {PT(LPAREN), KW(def), TI(ID), PT(LPAREN), ParserChar::beOptional(NI(ID_LIST)), PT(RPAREN), ParserChar::beOptional(NI(EXPRESSION_LIST)), PT(RPAREN)}, [](auto c, auto ts) {
        assert(ts.size() == 8);
        const std::shared_ptr<TokenID> id = std::dynamic_pointer_cast<TokenID>(ts.at(2));
        const auto parameters = std::dynamic_pointer_cast<NonTermID_LIST>(ts.at(4));
        const auto parametersNode = std::dynamic_pointer_cast<ASTIDListNode>(parameters->m_astnode);
        const auto exprList = std::dynamic_pointer_cast<NonTermEXPRESSION_LIST>(ts.at(6));
        const auto exprListNode = std::dynamic_pointer_cast<ASTExprListNode>(exprList->m_astnode);
        const auto ids = parametersNode ? parametersNode->m_ids : std::vector<std::string>();
        const auto exprs = exprListNode ? exprListNode->m_exprs : std::vector<std::shared_ptr<ASTExprNode>>();
        return std::make_shared<NonTermEXPRESSION>(std::make_shared<ASTFuncDefExprNode>(id->m_id, ids, exprs));
    });

    return parserPtr;
}

GObjectParser::GObjectParser():
    m_lexer(createTokenizer()), m_parser(createParser())
{
}
