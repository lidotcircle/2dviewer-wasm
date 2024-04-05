#ifndef _DC_PARSER_REGEX_EXPR_NODE_HPP_
#define _DC_PARSER_REGEX_EXPR_NODE_HPP_

#include <string>
#include <vector>
#include <stdexcept>
#include <memory>
#include <algorithm>
#include <assert.h>
#include <optional>
#include "./regex_char.hpp"
#include "./regex_expr.hpp"
#include "./regex_automata_node_nfa.hpp"


enum ExprNodeType {
    ExprNodeType_Empty, ExprNodeType_Group, ExprNodeType_CharRange,
    ExprNodeType_Concatenation, ExprNodeType_Union,  ExprNodeType_KleeneStar
};

template<typename CharT>
class ExprNode {
public:
    using NFAState_t = typename NodeNFA<CharT>::NFAState_t;
    virtual ExprNodeType node_type() const = 0;
    virtual std::string to_string() const = 0;

    virtual NodeNFA<CharT> to_nfa(StateAllocator<>& allocator, NFAState_t starts, NFAState_t finals) const = 0;

    virtual ~ExprNode() = default;
};

template<typename CharT>
class ExprNodeEmpty : public ExprNode<CharT> {
private:
    using traits = character_traits<CharT>;
    using char_type = CharT;
    using NodeNFATransitionTable = typename NodeNFA<CharT>::NodeNFATransitionTable;
    using NodeNFAEntry = typename NodeNFA<CharT>::NodeNFAEntry;
    using NFAState_t = typename NodeNFA<CharT>::NFAState_t;

public:
    virtual ExprNodeType node_type() const override { return ExprNodeType_Empty; }
    virtual std::string to_string() const override {
        return "";
    }

    virtual NodeNFA<CharT> to_nfa(StateAllocator<>& allocator, NFAState_t starts, NFAState_t finals) const override
    {
        NodeNFA<CharT> retval(NodeNFATransitionTable(), starts, finals);;
        retval.add_link(starts, { finals }, traits::EMPTY_CHAR, traits::EMPTY_CHAR);
        return retval;
    }

    virtual ~ExprNodeEmpty() override = default;
};

template<typename CharT>
class ExprNodeGroup : public ExprNode<CharT> {
private:
    using NFAState_t = typename NodeNFA<CharT>::NFAState_t;
    std::shared_ptr<ExprNode<CharT>> _next;
    bool _complemented;

public:
    ExprNodeGroup(std::shared_ptr<ExprNode<CharT>> next, bool complemented) : _next(next), _complemented(complemented) {}

    const std::shared_ptr<ExprNode<CharT>> next() const { return _next; }
    std::shared_ptr<ExprNode<CharT>> next() { return _next; }

    virtual ExprNodeType node_type() const override { return ExprNodeType_Group; }
    virtual std::string to_string() const override {
        return "(" + this->_next->to_string() + ")";
    }

    virtual NodeNFA<CharT> to_nfa(StateAllocator<>& allocator, NFAState_t starts, NFAState_t finals) const override
    {
        auto nfa = this->_next->to_nfa(allocator, starts, finals);
        if (!this->_complemented)
            return nfa;

        auto rnfa = nfa.toRegexNFA();
        auto dfa = rnfa.compile();
        auto complemented_dfa = dfa.complement();
        complemented_dfa.optimize();
        auto complemented_nfa = complemented_dfa.toNodeNFA();
        return complemented_nfa.relocate_state(allocator, starts, finals);;
    }

    virtual ~ExprNodeGroup() override = default;
};

template<typename CharT>
class ExprNodeCharRange : public ExprNode<CharT> {
private:
    using traits = character_traits<CharT>;
    using char_type = CharT;
    using NodeNFATransitionTable = typename NodeNFA<CharT>::NodeNFATransitionTable;
    using NodeNFAEntry = typename NodeNFA<CharT>::NodeNFAEntry;
    using NFAState_t = typename NodeNFA<CharT>::NFAState_t;
    char_type _min, _max;

public:
    ExprNodeCharRange(char_type min, char_type max) : _min(min), _max(max)
    {
        assert(min <= max);
        assert(traits::MIN <= min && min <= traits::MAX);
        assert(traits::MIN <= max && max <= traits::MAX);
    }

    char_type min() const { return _min; }
    char_type max() const { return _max; }

    virtual ExprNodeType node_type() const override { return ExprNodeType_CharRange; }
    virtual std::string to_string() const override {
        if (this->_min == this->_max)
            return char_to_string(this->_min);
        else
            return char_to_string(this->_min) + "-" + char_to_string(this->_max);
    }

    virtual NodeNFA<CharT> to_nfa(StateAllocator<>& allocator, NFAState_t starts, NFAState_t finals) const override
    {
        NodeNFA<CharT> retval(NodeNFATransitionTable(), starts, finals);
        retval.add_link(starts, { finals }, this->min(), this->max());
        return retval;
    }

    virtual ~ExprNodeCharRange() override = default;
};

template<typename CharT>
class ExprNodeConcatenation : public ExprNode<CharT> {
private:
    using traits = character_traits<CharT>;
    using char_type = CharT;
    using NodeNFATransitionTable = typename NodeNFA<CharT>::NodeNFATransitionTable;
    using NodeNFAEntry = typename NodeNFA<CharT>::NodeNFAEntry;
    using NFAState_t = typename NodeNFA<CharT>::NFAState_t;
    std::vector<std::shared_ptr<ExprNode<CharT>>> children;

public:
    ExprNodeConcatenation() {}

    const std::vector<std::shared_ptr<ExprNode<CharT>>>& children_ref() const { return children; }
    const size_t size() const { return children.size(); }

    void add_child(std::shared_ptr<ExprNode<CharT>> child) { children.push_back(child); }
    auto& back() { return children.back(); }
    const auto& back() const { return children.back(); }

    virtual ExprNodeType node_type() const override { return ExprNodeType_Concatenation; }
    virtual std::string to_string() const override {
        std::string result;
        for (auto& child : children)
            result += child->to_string();
        return result;
    }

    virtual NodeNFA<CharT> to_nfa(StateAllocator<>& allocator, NFAState_t starts, NFAState_t finals) const override
    {
        assert(this->size() > 0);
        if (this->size() == 1)
            return this->back()->to_nfa(allocator, starts, finals);

        NodeNFA<CharT> retval(NodeNFATransitionTable(), starts, finals);

        std::vector<std::pair<size_t,size_t>> starts_finals(this->size());
        starts_finals.front().first = starts;
        starts_finals.back().second = finals;
        for (size_t i=0;i<starts_finals.size()-1;i++) {
            auto ns = allocator.newstate();
            starts_finals[i].second = ns;
            starts_finals[i+1].first = ns;
        }

        for (size_t i=0;i<starts_finals.size();i++) {
            auto s = starts_finals[i].first;
            auto f = starts_finals[i].second;
            auto& child = this->children[i];
            auto child_nfa = child->to_nfa(allocator, s, f);
            auto& child_transitions = child_nfa.transitions();
            for (auto& entryp : child_transitions) {
                auto& w = entryp.first;
                auto& entry = entryp.second;

                retval.merge_with(w, entry);
            }
        }

        return retval;
    }

    virtual ~ExprNodeConcatenation() override = default;
};

template<typename CharT>
class ExprNodeUnion : public ExprNode<CharT> {
private:
    using traits = character_traits<CharT>;
    using char_type = CharT;
    using NodeNFATransitionTable = typename NodeNFA<CharT>::NodeNFATransitionTable;
    using NodeNFAEntry = typename NodeNFA<CharT>::NodeNFAEntry;
    using NFAState_t = typename NodeNFA<CharT>::NFAState_t;
    std::vector<std::shared_ptr<ExprNode<CharT>>> children;

public:
    ExprNodeUnion() {}

    const std::vector<std::shared_ptr<ExprNode<CharT>>>& children_ref() const { return children; }
    const size_t size() const { return children.size(); }

    void add_child(std::shared_ptr<ExprNode<CharT>> child) { children.push_back(child); }
    auto& back() { return children.back(); }
    const auto& back() const { return children.back(); }

    virtual ExprNodeType node_type() const override { return ExprNodeType_Union; }
    virtual std::string to_string() const override {
        std::string result;
        for (auto& child : children)
            result += child->to_string() + "|";
        result.erase(result.end() - 1);
        return result;
    }

    virtual NodeNFA<CharT> to_nfa(StateAllocator<>& allocator, NFAState_t starts, NFAState_t finals) const override
    {
        assert(this->size() > 0);
        if (this->size() == 1)
            return this->back()->to_nfa(allocator, starts, finals);

        NodeNFA<CharT> retval(NodeNFATransitionTable(), starts, finals);

        for (auto& child: this->children) {
            auto child_nfa = child->to_nfa(allocator, starts, finals);
            auto& child_transitions = child_nfa.transitions();
            for (auto& entryp : child_transitions) {
                auto& w = entryp.first;
                auto& entry = entryp.second;

                retval.merge_with(w, entry);
            }
        }

        return retval;
    }

    virtual ~ExprNodeUnion() override = default;
};

template<typename CharT>
class ExprNodeKleeneStar : public ExprNode<CharT> {
private:
    using traits = character_traits<CharT>;
    using char_type = CharT;
    using NodeNFATransitionTable = typename NodeNFA<CharT>::NodeNFATransitionTable;
    using NodeNFAEntry = typename NodeNFA<CharT>::NodeNFAEntry;
    using NFAState_t = typename NodeNFA<CharT>::NFAState_t;
    std::shared_ptr<ExprNode<CharT>> _next;

public:
    ExprNodeKleeneStar(std::shared_ptr<ExprNode<CharT>> next) : _next(next) {}

    const std::shared_ptr<ExprNode<CharT>> next() const { return _next; }
    std::shared_ptr<ExprNode<CharT>> next() { return _next; }

    virtual ExprNodeType node_type() const override { return ExprNodeType_KleeneStar; }
    virtual std::string to_string() const override {
        return this->_next->to_string() + "*";
    }

    virtual NodeNFA<CharT> to_nfa(StateAllocator<>& allocator, NFAState_t starts, NFAState_t finals) const override
    {
        auto retval = this->next()->to_nfa(allocator, starts, finals);
        retval.add_link(starts, { finals }, traits::EMPTY_CHAR, traits::EMPTY_CHAR);
        retval.add_link(finals, { starts }, traits::EMPTY_CHAR, traits::EMPTY_CHAR);
        return retval;
    }

    virtual ~ExprNodeKleeneStar() override = default;
};

template<typename CharT>
class RegexNodeTreeGenerator {
private:
    using traits = character_traits<CharT>;
    using char_type = CharT;
    using regex_char = RegexPatternChar<char_type>;
    using node_type = std::shared_ptr<ExprNode<char_type>>;;
    struct StackValueState {
        std::shared_ptr<ExprNode<char_type>> node;
        bool complemented;

        StackValueState(): node(std::make_shared<ExprNodeEmpty<char_type>>()), complemented(false) {}
    };
    std::vector<StackValueState> _stack;
    bool _endding;

    node_type push_node_to_node(node_type oldnode, node_type node)
    {
        assert(oldnode);
        node_type ret;

        switch (oldnode->node_type())
        {
            case ExprNodeType_Concatenation: {
                auto ptr = std::dynamic_pointer_cast<ExprNodeConcatenation<char_type>>(oldnode);
                ptr->add_child(node);
                ret = oldnode;
            } break;
            case ExprNodeType_Union: {
                auto ptr = std::dynamic_pointer_cast<ExprNodeUnion<char_type>>(oldnode);
                auto& last = ptr->back();
                last = push_node_to_node(last, node);
                ret = oldnode;
            } break;
            case ExprNodeType_Group:
            case ExprNodeType_CharRange:
            case ExprNodeType_KleeneStar: {
                auto newnode = std::make_shared<ExprNodeConcatenation<char_type>>();
                newnode->add_child(oldnode);
                newnode->add_child(node);
                ret = newnode;
            } break;
            case ExprNodeType_Empty: {
                ret = node;
            } break;
            default:
                assert("regex: invalid node type");
        }

        return ret;
    }
    void push_node_to_stacktop(node_type node) {
        auto& stacktop = _stack.back();
        stacktop.node = push_node_to_node(stacktop.node, node);
    }
    void push_char(char_type c) {
        auto node = std::make_shared<ExprNodeCharRange<char_type>>(c, c);
        push_node_to_stacktop(node);
    }
    void stacktop_to_union_node() {
        auto& stacktop = _stack.back();
        assert (stacktop.node != nullptr);

        switch (stacktop.node->node_type())
        {
            case ExprNodeType_Empty:
            case ExprNodeType_Group:
            case ExprNodeType_CharRange:
            case ExprNodeType_KleeneStar:
            case ExprNodeType_Concatenation: {
                auto newnode = std::make_shared<ExprNodeUnion<char_type>>();
                newnode->add_child(stacktop.node);
                stacktop.node = newnode;
            } break;
            case ExprNodeType_Union: break;
            default:
                assert("regex: invalid node type");
        }
    }
    node_type node_to_kleen_start_node(node_type node) {
        assert(node != nullptr);
        node_type ret;

        switch (node->node_type())
        {
            case ExprNodeType_Group:
            case ExprNodeType_KleeneStar:
            case ExprNodeType_CharRange: {
                auto newnode = std::make_shared<ExprNodeKleeneStar<char_type>>(node);
                ret = newnode;
            } break;
            case ExprNodeType_Concatenation: {
                auto ptr = std::dynamic_pointer_cast<ExprNodeConcatenation<char_type>>(node);
                assert(ptr->size() >= 1);
                auto& back = ptr->back();
                back = node_to_kleen_start_node(back);
                ret = node;
            } break;
            case ExprNodeType_Union: {
                auto ptr = std::dynamic_pointer_cast<ExprNodeUnion<char_type>>(node);
                assert(ptr->size() >= 1);
                auto& back = ptr->back();
                back = node_to_kleen_start_node(back);
                ret = node;
            } break;
            case ExprNodeType_Empty:
                throw std::runtime_error("regex: invalid kleene star node");
            default:
                assert("regex: invalid node type");
        }

        return ret;
    }
    void stacktop_to_kleene_star_node() {
        auto& stacktop = _stack.back();
        assert(stacktop.node != nullptr);
        stacktop.node = node_to_kleen_start_node(stacktop.node);
        assert(stacktop.node != nullptr);
    }

    bool m_in_bracket_mode;
    bool m_bracket_reversed;
    size_t m_bracket_eat_count;
    enum BracketState {
        BracketState_None, BracketState_One,
        BracketState_Dashed, BracketState_Two,
    } m_bracket_state;
    std::vector<std::pair<char_type, char_type>> m_bracket_ranges;
    char_type m_range_low, m_range_up;

    void enter_bracket_mode() {
        m_in_bracket_mode = true;
        m_bracket_eat_count = 0;
        m_bracket_reversed = false;
        m_bracket_state = BracketState_None;
        m_bracket_ranges.clear();
    }
    void leave_bracket_mode() {
        this->push_bracket_range();
        this->m_bracket_state = BracketState_None;
        m_in_bracket_mode = false;

        std::sort(m_bracket_ranges.begin(), m_bracket_ranges.end());
        auto merged_ranges = merge_sorted_range(m_bracket_ranges);
        if (merged_ranges.empty())
            throw std::runtime_error("Regex: empty bracket");

        if (this->m_bracket_reversed) {
            std::vector<std::pair<char_type,char_type>> reversed_range;
            std::optional<char_type> minval = traits::MIN;
            for(auto r: merged_ranges) {
                assert(minval <= r.first);
                assert(minval.has_value());
                if (minval.value() < r.first) {
                    reversed_range.push_back(std::make_pair(minval.value(), r.first - 1));
                }

                if (r.second < traits::MAX)
                    minval = r.second + 1;
                else
                    minval = std::nullopt;
            }
            if (minval.has_value())
                reversed_range.push_back(std::make_pair(minval.value(), traits::MAX));
            merged_ranges = std::move(reversed_range);
            if(merged_ranges.empty())
                throw std::runtime_error("Regex: empty bracket, exclude everything");
        }

        if (merged_ranges.size() == 1) {
            auto node = std::make_shared<ExprNodeCharRange<char_type>>(merged_ranges[0].first, merged_ranges[0].second);
            this->push_node_to_stacktop(std::make_shared<ExprNodeGroup<char_type>>(node, false));
        } else {
            auto node = std::make_shared<ExprNodeUnion<char_type>>();
            for (auto& range : merged_ranges) {
                auto node_range = std::make_shared<ExprNodeCharRange<char_type>>(range.first, range.second);
                node->add_child(node_range);
            }
            this->push_node_to_stacktop(std::make_shared<ExprNodeGroup<char_type>>(node, false));
        }
    }
    void push_bracket_range() {
        switch (this->m_bracket_state) {
            case BracketState_None:
                break;
            case BracketState_One:
                this->m_bracket_ranges.push_back(std::make_pair(this->m_range_low, this->m_range_low));
                break;
            case BracketState_Dashed:
                this->m_bracket_ranges.push_back(std::make_pair(this->m_range_low, traits::MAX));
                break;
            case BracketState_Two:
                this->m_bracket_ranges.push_back(std::make_pair(this->m_range_low, this->m_range_up));
                break;
            default:
               throw std::runtime_error("invalid bracket state");
        }
    }
    bool handle_bracket_mode(regex_char _c) {
        if (!this->m_in_bracket_mode)
            return false;

        const auto c = _c.get();
        const auto escaped = _c.is_escaped();
        if (this->m_bracket_eat_count++ == 0 &&
            !escaped &&
            c == traits::CARET)
        {
            this->m_bracket_reversed = true;
            return true;
        }

        const bool bracket_end = !escaped && c == traits::RBRACKET;
        if (bracket_end) {
            this->leave_bracket_mode();
            return true;
        }

        if (escaped) {
            if (c != traits::RBRACKET && c != traits::BACKSLASH && c != traits::DASH)
                throw std::runtime_error("unexpected escape seqeuence");
        }

        switch (this->m_bracket_state) {
            case BracketState_None:
            {
                this->m_range_low = c;
                this->m_bracket_state = BracketState_One;
            } break;
            case BracketState_One:
            {
                if (!escaped && c == traits::DASH) {
                    this->m_bracket_state = BracketState_Dashed;
                } else {
                    this->push_bracket_range();
                    this->m_range_low = c;
                }
            } break;
            case BracketState_Dashed:
            {
                this->m_range_up = c;
                this->m_bracket_state = BracketState_Two;
                this->push_bracket_range();
                this->m_bracket_state = BracketState_None;
            } break;
            default:
                throw std::runtime_error("unexpected bracket state");
        }

        return true;
    }

public:
    RegexNodeTreeGenerator():
       _endding(false),
        m_in_bracket_mode(false),
        _stack()
    {
        this->_stack.push_back(StackValueState());
    }

    void feed(regex_char _c) {
        assert(!this->_endding);

        if (this->handle_bracket_mode(_c))
            return;

        auto c = _c.get();
        if (_c.is_escaped()) {
            if (c != traits::LPAREN && c != traits::RPAREN &&
                c != traits::LBRACKET && c != traits::CARET &&
                c != traits::DASH && c != traits::RBRACKET &&
                c != traits::OR && c != traits::STAR &&
                c != traits::EXCLAMATION &&
                c != traits::BACKSLASH)
            {
                throw std::runtime_error("unexpected escape seqeuence");
            }

            this->push_char(c);
            return;
        }

        switch (c) {
            case traits::LPAREN:
            {
                this->_stack.push_back(StackValueState());
            } break;
            case traits::EXCLAMATION:
            {
                auto& back = this->_stack.back();
                auto emptynode = std::dynamic_pointer_cast<ExprNodeEmpty<char_type>>(back.node);
                if (emptynode && !back.complemented) {
                    this->_stack.back().complemented = true;
                } else {
                    this->push_char(c);
                }
            } break;
            case traits::RPAREN:
            {
                if (this->_stack.size() < 2)
                    throw std::runtime_error("unexpected ')'");

                auto top = this->_stack.back();
                this->_stack.pop_back();
                auto group = std::make_shared<ExprNodeGroup<char_type>>(top.node, top.complemented);
                this->push_node_to_stacktop(group);
            } break;
            case traits::LBRACKET:
            {
                this->enter_bracket_mode();
            } break;
            case traits::RBRACKET:
            {
                throw std::runtime_error("unexpected ]");
            } break;
            case traits::OR:
            {
                this->stacktop_to_union_node();
                auto& top = this->_stack.back();
                auto node = std::dynamic_pointer_cast<ExprNodeUnion<char_type>>(top.node);
                assert(node != nullptr);
                node->add_child(std::make_shared<ExprNodeEmpty<char_type>>());
            } break;
            case traits::STAR:
            {
                this->stacktop_to_kleene_star_node();
            } break;
            default:
                this->push_char(c);
        }
    }

    node_type end() {
        if (this->_endding)
            throw std::runtime_error("already endding");

        this->_endding = true;

        if (this->_stack.size() != 1)
            throw std::runtime_error("unexpected end");

        auto top = this->_stack.back();
        this->_stack.pop_back();
        return top.node;
    }

    static node_type parse(const std::vector<regex_char>& pattern) {
        RegexNodeTreeGenerator<char_type> gen;
        for (auto c: pattern) {
            gen.feed(c);
        }
        return gen.end();
    }

    static node_type parse(const std::vector<char_type>& str) {
        auto nx = RegexPatternEscaper<char_type>::convert(str.begin(), str.end());
        return parse(nx);
    }
};

#endif // REGEX_NODE_TREE_GENERATOR_HPP
