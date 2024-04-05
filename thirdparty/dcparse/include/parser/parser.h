#ifndef _DC_PARSER_HPP_
#define _DC_PARSER_HPP_

#include "../lexer/token.h"
#include "../lexer/simple_lexer.hpp"
#include "../lexer/text_info.h"
#include "parser_error.h"
#include <ostream>
#include <vector>
#include <memory>
#include <tuple>
#include <set>
#include <map>
#include <optional>
#include <functional>

using dchar_t   = std::shared_ptr<DChar>;
using dctoken_t = std::shared_ptr<LexerToken>;

class NonTerminal : public DChar { };
using dnonterm_t = std::shared_ptr<NonTerminal>;

size_t GetEOFChar(); 

struct RuleOption;
enum RuleAssocitive {
    RuleAssocitiveLeft,
    RuleAssocitiveRight,
};
struct PushdownStateMapping;
struct PushdownEntry;

class DCParser {
public:
    class DCParserContext {
    private:
        DCParser* m_parser;

    public:
        inline DCParserContext(): m_parser(nullptr) {};
        inline DCParserContext(DCParser& parser): m_parser(&parser) {}

        DCParserContext& operator=(const DCParserContext&) = delete;
        DCParserContext(const DCParserContext&) = delete;

        inline DCParser* parser() { return this->m_parser; }

        virtual ~DCParserContext() = default;
    };
    using pcontext_t = std::weak_ptr<DCParserContext>;

    class DCParserRulePriority {
    private:
        friend class DCParser;
        size_t m_priority;
        DCParserRulePriority() = delete;
        DCParserRulePriority(const DCParserRulePriority&) = delete;
        DCParserRulePriority& operator=(const DCParserRulePriority&) = delete;
        DCParserRulePriority& operator=(DCParserRulePriority&&) = delete;
        DCParserRulePriority(size_t priority);
        size_t priority() const;

    public:
        DCParserRulePriority(DCParserRulePriority&&) = default;;
        virtual ~DCParserRulePriority();
    };
    using priority_t = std::shared_ptr<DCParserRulePriority>;

    class RuleDecision {
    public:
        virtual bool decide_on_pos(size_t pos) const;
        virtual bool decide_on_end() const;
        virtual bool decide(pcontext_t context, const std::vector<dchar_t>& vx, const std::vector<dchar_t>& charstack) = 0;
        virtual ~RuleDecision() = default;
    };
    using decision_t = std::shared_ptr<RuleDecision>;

    using charid_t = size_t;
    using ruleid_t = size_t;
    using state_t  = size_t;
    using context_t = std::shared_ptr<DCParserContext>;
    using reduce_callback_t = std::function<dnonterm_t(pcontext_t context, std::vector<dchar_t>& children)>;

    class ParserChar {
    private:
        DCharInfo _info;
        bool _optional;

    public:
        ParserChar() = delete;
        inline ParserChar(DCharInfo info): _info(info), _optional(false) { }

        inline const char* name() const { return _info.name; }
        inline charid_t cid()     const { return this->_info.id; }
        inline DCharInfo info()   const { return this->_info; }
        inline bool optional()    const { return this->_optional; }

        static inline ParserChar beOptional(DCharInfo info)
        {
            auto c = ParserChar(info);
            c._optional = true;
            return c; 
        }
        static inline ParserChar beOptional(ParserChar c) { c._optional = true; return c; }
    };


private:
    struct RuleInfo {
        charid_t                    m_lhs;
        std::vector<charid_t>       m_rhs;
        std::vector<bool>           m_rhs_optional;
        reduce_callback_t           m_reduce_callback;
        std::shared_ptr<RuleOption> m_rule_option;
    };
    std::vector<RuleInfo> m_rules;
    context_t m_context;
    std::set<charid_t> m_nonterms;
    std::set<charid_t> m_terms;
    std::set<charid_t> m_symbols;
    std::set<charid_t> m_start_symbols;
    size_t m_priority;

    template<typename T>
    class SetStateAllocator {
    private:
        using setelem = T;
        state_t m_next_state;
        std::map<std::set<setelem>,state_t> m_state_map;

    public:
        SetStateAllocator(): m_next_state(0) {}

        SetStateAllocator(const SetStateAllocator&) = delete;

        SetStateAllocator& operator=(const SetStateAllocator&) = delete;
        SetStateAllocator(SetStateAllocator&&) = delete;
        SetStateAllocator& operator=(SetStateAllocator&&) = delete;

        const std::map<std::set<setelem>,state_t>& themap() const
        {
            return this->m_state_map; 
        }

        state_t query(const std::set<setelem>& states)
        {
            auto it = this->m_state_map.find(states);
            if (it != this->m_state_map.end()) {
                return it->second;
            }
            auto new_state = this->m_next_state++;
            this->m_state_map[states] = new_state;
            return new_state;
        }

        state_t operator()(const std::set<setelem>& states)
        {
            return this->query(states);
        }

        size_t max_state() const { return this->m_next_state; }
    };

    std::shared_ptr<PushdownEntry>
        state_action(std::set<std::pair<ruleid_t,size_t>> state,
                     bool evaluate_decision,
                     SetStateAllocator<std::pair<ruleid_t,size_t>>& state_allocator,
                     std::set<std::set<std::pair<ruleid_t,size_t>>>& new_state_set);

    bool m_lookahead_rule_propagation;
    std::shared_ptr<PushdownStateMapping> m_pds_mapping;
    std::optional<state_t> m_start_state;
    std::vector<std::set<std::pair<ruleid_t,size_t>>> h_state2set;
    std::map<charid_t,DCharInfo> h_charinfo;
    std::shared_ptr<TextInfo> h_textinfo;
    std::ostream* h_debug_stream;
    void      see_dchar(DCharInfo char_);
    DCharInfo get_dchar(charid_t id) const;
    std::string help_rule2str(ruleid_t rule, size_t pos) const;
    std::string help_when_reject_at(state_t state, charid_t token) const;
    std::string help_print_state(state_t state, size_t count, char paddingchar) const;
    void        help_print_unseen_rules_into_debug_stream() const;

    std::vector<state_t> p_state_stack;
    std::vector<dchar_t> p_char_stack;
    std::optional<std::pair<dchar_t,const PushdownEntry*>> p_not_finished;

    std::optional<charid_t> m_real_start_symbol;
    void setup_real_start_symbol();

    void                   do_shift(state_t state, dchar_t char_);
    std::optional<dchar_t> do_reduce(ruleid_t rule_id, dchar_t char_);

    std::optional<dchar_t> handle_lookahead(dctoken_t token);
    void feed_internal(dchar_t char_);

private:
    // Rules for creating that non-terminal
    std::map<charid_t,std::vector<ruleid_t>> u_nonterm_epsilon_closure;
    // no pratical use
    std::map<charid_t,std::set<charid_t>>    u_nonterm_possible_next;
    void ensure_epsilon_closure();
    std::set<std::pair<ruleid_t,size_t>> stateset_epsilon_closure(const std::set<std::pair<ruleid_t,size_t>>& st);

    bool u_possible_prev_next_computed;
    void compute_posible_prev_next();
    std::map<charid_t,std::set<charid_t>> u_prev_possible_token_of;
    std::map<charid_t,std::set<charid_t>> u_next_possible_token_of;

    std::set<std::pair<ruleid_t,size_t>>
    stateset_move(const std::set<std::pair<ruleid_t,size_t>>& stateset,
                  charid_t symbol) const;

    bool is_nonterm(charid_t id) const;
    std::set<std::pair<ruleid_t,size_t>> startState() const;

    int  add_rule_internal(charid_t leftside, 
                           std::vector<charid_t> rightside, std::vector<bool> optional,
                           reduce_callback_t reduce_cb, RuleAssocitive associative,
                           decision_t decision, std::set<size_t> positions, priority_t priority);

    using PreAction = std::function<std::optional<dctoken_t>(const std::vector<dchar_t>& symbolStack, dctoken_t)>;
    PreAction m_preAction;
    void collect_expected_next_chars(state_t state, std::set<charid_t>& expectedNextTokens) const;

    using RecoverFromRejectFn = std::function<std::optional<std::pair<int,size_t>>(const std::vector<dchar_t>& symbolStack, const std::vector<dctoken_t>& nextNTokens)>;
    RecoverFromRejectFn  m_recFn;
    std::vector<dctoken_t> m_prevSave;
    std::optional<ParserRejectTokenError> m_need_recover;
    std::optional<bool> recover_from_reject();

public:
    DCParser(bool lookahead_rule_propagation = true);
    DCParser(const DCParser&) = delete;
    DCParser& operator=(const DCParser&) = delete;
    DCParser(DCParser&&) = delete;
    DCParser& operator=(DCParser&&) = delete;
    virtual ~DCParser();

    priority_t dec_priority();
    inline priority_t __________() { return this->dec_priority(); }

    inline void setContext(context_t context)        { this->m_context = context; }
    inline context_t getContext()             { return this->m_context; }
    inline const context_t getContext() const { return this->m_context; }

    void setDebugStream(std::ostream& stream);
    void SetTextinfo(std::shared_ptr<TextInfo> info);

    void setPreAction(PreAction fn) { m_preAction = fn; }
    void setRecoverFn(RecoverFromRejectFn fn) { m_recFn = fn; }

    std::set<charid_t> get_expected_chars() const;

    void add_rule(DCharInfo leftside, std::vector<ParserChar> rightside,
                  reduce_callback_t reduce_cb,
                  RuleAssocitive associative,
                  decision_t decision,
                  priority_t priority);

    DCParser& operator()(DCharInfo leftside, std::vector<ParserChar> rightside,
                         reduce_callback_t reduce_cb, RuleAssocitive associative = RuleAssocitiveLeft,
                         decision_t decision = nullptr, priority_t priority = nullptr);

    void add_start_symbol(charid_t start);

    void generate_table();

    std::set<charid_t> prev_possible_token_of(charid_t id) const;
    std::set<charid_t> next_possible_token_of(charid_t id) const;
    DCharInfo query_charinfo(charid_t id) const;

    void feed(dctoken_t token);
    dnonterm_t end();

    template<typename Iterator>
    dnonterm_t parse(Iterator begin, Iterator end) {
        assert(m_start_state.has_value());
        assert(this->p_state_stack.empty());

        for (auto it = begin; it != end; ++it) {
            this->feed(*it);
        }
        return this->end();
    }

    dnonterm_t parse(ISimpleLexer& lexer);

    void reset();
};

using ParserChar = DCParser::ParserChar;

class RuleDecisionFunction : public DCParser::RuleDecision 
{
public:
    using pcontext_t = typename DCParser::pcontext_t;
    using decider_t = std::function<bool(pcontext_t context, const std::vector<dchar_t>& vx, const std::vector<dchar_t>& char_stack)>;

private:
    decider_t m_decider;
    std::set<size_t> m_positions;
    bool m_on_end;

public:
    RuleDecisionFunction(decider_t decider, std::set<size_t> positions = {}, bool on_end = true);

    virtual bool decide_on_pos(size_t pos) const override;
    virtual bool decide_on_end() const override;
    virtual bool decide(pcontext_t context, const std::vector<dchar_t>& vx, const std::vector<dchar_t>& char_stack) override;
};

#endif // _DC_PARSER_HPP_
