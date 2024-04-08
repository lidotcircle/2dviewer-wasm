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

class ASTModuleNode: public ASTNode {
public:
    ASTModuleNode() {}

    void PushExpression(std::shared_ptr<ASTExprNode> expr)
    {
        m_exprs.push_back(expr);
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

private:
    std::unique_ptr<Lexer<int>> m_lexer;
    std::unique_ptr<DCParser>   m_parser;
};

}
