#pragma once
#include <vector>
#include <memory>
#include <string>


class DCParser;
template<typename T>
class Lexer;

namespace M2V {

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual std::string format() = 0;
};

class ASTExprNode: public ASTNode { };

class ASTExprListNode: public ASTNode {
public:
    ASTExprListNode() {}

    std::string format() override {
        std::string ans;
        for (auto& expr: m_exprs) {
            ans += expr->format() + " ";
        }
        if (ans.back() == ' ') ans.pop_back();
        return ans;
    }

    std::vector<std::shared_ptr<ASTExprNode>> m_exprs;
};

class ASTIDListNode: public ASTNode {
public:
    ASTIDListNode() {}

    std::string format() override {
        std::string ans;
        for (auto& id: m_ids) {
            ans += id + " ";
        }
        if (ans.back() == ' ') ans.pop_back();
        return ans;
    }

    std::vector<std::string> m_ids;
};

class ASTFuncExprNode: public ASTExprNode {
public:
    ASTFuncExprNode(const std::string& func, std::vector<std::shared_ptr<ASTExprNode>> args):
        m_func(func), m_args(args) {}

    std::string format() override {
        std::string ans = "(" + m_func;
        for (auto& arg: m_args) {
            ans += " " + arg->format();
        }
        ans += ")";
        return ans;
    }

private:
    std::string m_func;
    std::vector<std::shared_ptr<ASTExprNode>> m_args;
};

class ASTFuncDefExprNode: public ASTExprNode {
public:
    ASTFuncDefExprNode(const std::string& funcname, std::vector<std::string> parameters, std::vector<std::shared_ptr<ASTExprNode>> exprs):
        m_funcname(funcname), m_parameters(parameters), m_exprs(exprs) {}

    std::string format() override {
        std::string ans = "(def " + m_funcname + " (";
        for (auto& p: m_parameters) {
            ans += p + " ";
        }
        if (ans.back() == ' ') ans.pop_back();
        ans += ") ";
        for (auto& expr: m_exprs) {
            ans += expr->format() + " ";
        }
        if (ans.back() == ' ') ans.pop_back();
        ans += ")";
        return ans;
    }

private:
    std::string m_funcname;
    std::vector<std::string> m_parameters;
    std::vector<std::shared_ptr<ASTExprNode>> m_exprs;
};

class ASTMinusExprNode: public ASTExprNode {
public:
    ASTMinusExprNode(std::shared_ptr<ASTExprNode> expr):
        m_expr(expr) {}

    std::string format() override {
        return "-" + m_expr->format();
    }

public:
    std::shared_ptr<ASTExprNode> m_expr;
};

class ASTLetExprNode: public ASTExprNode {
public:
    ASTLetExprNode(const std::string& id, std::shared_ptr<ASTExprNode> expr):
        m_id(id), m_expr(expr) {}

    std::string format() override {
        return "(let " + m_id + " " + m_expr->format() + ")";
    }

public:
    std::string m_id;
    std::shared_ptr<ASTExprNode> m_expr;
};

inline std::string RemoveSlash(const std::string& str)
{
    std::string ans;
    for (auto& c: str) if (c != '\\') ans.push_back(c);
    return ans;
}

class ASTBinaryOpExprNode: public ASTExprNode {
public:
    ASTBinaryOpExprNode(const std::string& op, std::shared_ptr<ASTExprNode> left, std::shared_ptr<ASTExprNode> right):
        m_op(RemoveSlash(op)), m_left(left), m_right(right) {}

    std::string format() override {
        return "(" + m_op + " " + m_left->format() + " " + m_right->format() + ")";
    }

public:
    std::string m_op;
    std::shared_ptr<ASTExprNode> m_left, m_right;
};

class ASTIntExprNode: public ASTExprNode {
public:
    ASTIntExprNode(int64_t val): m_value(val) {}

    std::string format() override { return std::to_string(m_value); }

private:
    int64_t m_value;
};

class ASTFloatExprNode: public ASTExprNode {
public:
    ASTFloatExprNode(double val): m_value(val) {}

    std::string format() override { return std::to_string(m_value); }

private:
    double m_value;
};

class ASTStringExprNode: public ASTExprNode {
public:
    ASTStringExprNode(const std::string& val): m_value(val) {}

    std::string format() override { return "\"" + m_value + "\""; }

private:
    std::string m_value;
};

class ASTIDExprNode: public ASTExprNode {
public:
    ASTIDExprNode(const std::string& id): m_id(id) {}

    std::string format() override { return m_id; }

private:
    std::string m_id;
};

class ASTModuleNode: public ASTNode {
public:
    ASTModuleNode() {}

    void PushExpression(std::shared_ptr<ASTExprNode> expr)
    {
        m_exprs.push_back(expr);
    }

    std::string format() override {
        std::string ans;
        for (auto& expr: m_exprs) {
            ans += expr->format() + " ";
        }
        if (ans.back() == ' ') ans.pop_back();
        return ans;
    }

private:
    std::vector<std::shared_ptr<ASTExprNode>> m_exprs;
};


class GObjectParser {
public:
    GObjectParser();

    std::shared_ptr<ASTModuleNode>
    parse(const std::string& str);

    void reset();

    ~GObjectParser();

private:
    std::unique_ptr<Lexer<int>> m_lexer;
    std::unique_ptr<DCParser>   m_parser;
};

}
