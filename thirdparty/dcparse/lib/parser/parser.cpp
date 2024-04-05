#include "parser/parser.h"
#include "parser/parser_error.h"
#include "algo.hpp"
#include <assert.h>
#include <sstream>
#include <iomanip>
#include <map>
#include <queue>
#include <stdexcept>
using namespace std;

using charid_t = DCParser::charid_t;
using ruleid_t = DCParser::ruleid_t;
using state_t  = DCParser::state_t;
using reduce_callback_t = DCParser::reduce_callback_t;
using decision_t = DCParser::decision_t;
using priority_t = DCParser::priority_t;

struct EOFChar: public LexerToken {};
charid_t GetEOFChar() { return CharID<EOFChar>(); }

struct RealStartSymbol: public NonTerminal {
    shared_ptr<NonTerminal> sym;

    RealStartSymbol() = delete;
    RealStartSymbol(shared_ptr<NonTerminal> sym): sym(sym) {}
};

DCParser::DCParserRulePriority::DCParserRulePriority(size_t priority): m_priority(priority) {}
DCParser::DCParserRulePriority::~DCParserRulePriority() {}
size_t DCParser::DCParserRulePriority::priority() const { return m_priority; }

bool DCParser::RuleDecision::decide_on_pos(size_t pos) const { return false; }
bool DCParser::RuleDecision::decide_on_end() const { return true; }

struct RuleOption {
    size_t         priority;
    RuleAssocitive associtive;
    decision_t     decision;
    set<size_t>    decision_pos;
    bool           seen;
    int            ruleid;
};

struct PushdownEntry;
using  PushdownStateLookup = map<state_t, PushdownEntry>;
using  PushdownStateMappingTX = vector<PushdownStateLookup>;
struct PushdownStateMapping {
    PushdownStateMappingTX val; 

    PushdownStateMapping() = delete;

    template<typename T>
    PushdownStateMapping(T v) : val(std::forward<T>(v)) { }
};

struct PushdownEntry
{
public:
    enum PushdownType {
        STATE_TYPE_SHIFT,
        STATE_TYPE_REDUCE,
        STATE_TYPE_LOOKAHEAD,
        STATE_TYPE_REJECT,
        STATE_TYPE_DECISION,
    };

    struct decision_info_t {
        vector<pair<ruleid_t,size_t>> evals;
        map<set<pair<ruleid_t,size_t>>,shared_ptr<PushdownEntry>> action;
    };

private:
    PushdownType _type;
    union MM {
        state_t state;
        ruleid_t rule;
        shared_ptr<PushdownStateLookup> next;
        shared_ptr<decision_info_t> decision_info;

        MM(): state(0) {}
        ~MM() {}
    } _u;

    class StateTypeH {};
    class RuleTypeH {};

    PushdownEntry(state_t state, StateTypeH): _type(STATE_TYPE_SHIFT) { _u.state = state; }
    PushdownEntry(ruleid_t rule, RuleTypeH): _type(STATE_TYPE_REDUCE) { _u.rule = rule; }
    PushdownEntry(shared_ptr<PushdownStateLookup> next)
        : _type(STATE_TYPE_LOOKAHEAD)
    {
        new(&_u.next) shared_ptr<PushdownStateLookup>(next);
    }
    PushdownEntry(shared_ptr<decision_info_t> decision_info)
        : _type(STATE_TYPE_DECISION)
    {
        new(&_u.decision_info) shared_ptr<decision_info_t>(decision_info);
    }


public:
    PushdownEntry(): _type(STATE_TYPE_REJECT) { }
    PushdownType type() const { return this->_type; }

    PushdownEntry(const PushdownEntry& other)
        : _type(other._type)
    {
        switch (this->_type) {
        case STATE_TYPE_SHIFT:
            this->_u.state = other._u.state;
            break;
        case STATE_TYPE_REDUCE:
            this->_u.rule = other._u.rule;
            break;
        case STATE_TYPE_LOOKAHEAD:
            new(&this->_u.next) shared_ptr<PushdownStateLookup>(other._u.next);
            break;
        case STATE_TYPE_REJECT:
            break;
        case STATE_TYPE_DECISION:
            new(&this->_u.decision_info) shared_ptr<decision_info_t>(other._u.decision_info);
            break;
        }
    }

    PushdownEntry& operator=(const PushdownEntry& other)
    {
        if (this->type() == STATE_TYPE_LOOKAHEAD) {
            this->_u.next.~shared_ptr<PushdownStateLookup>();
        } else if (this->type() == STATE_TYPE_DECISION) {
            this->_u.decision_info.~shared_ptr<decision_info_t>();
        }

        this->_type = other._type;
        switch (this->_type) {
        case STATE_TYPE_SHIFT:
            this->_u.state = other._u.state;
            break;
        case STATE_TYPE_REDUCE:
            this->_u.rule = other._u.rule;
            break;
        case STATE_TYPE_LOOKAHEAD:
            new(&this->_u.next) shared_ptr<PushdownStateLookup>(other._u.next);
            break;
        case STATE_TYPE_REJECT:
            break;
        case STATE_TYPE_DECISION:
            new(&this->_u.decision_info) shared_ptr<decision_info_t>(other._u.decision_info);
            break;
        }

        return *this;
    }

    state_t state() const {
        assert(this->type() == STATE_TYPE_SHIFT);
        return this->_u.state;
    }

    ruleid_t rule() const {
        assert(this->type() == STATE_TYPE_REDUCE);
        return this->_u.rule;
    }

    const shared_ptr<PushdownStateLookup> lookup() const {
        assert(this->type() == STATE_TYPE_LOOKAHEAD);
        return this->_u.next;
    }

    shared_ptr<PushdownStateLookup> lookup() {
        assert(this->type() == STATE_TYPE_LOOKAHEAD);
        return this->_u.next;
    }

    const shared_ptr<decision_info_t> decision() const {
        assert(this->type() == STATE_TYPE_DECISION);
        return this->_u.decision_info;
    }

    shared_ptr<decision_info_t> decision() {
        assert(this->type() == STATE_TYPE_DECISION);
        return this->_u.decision_info;
    }

    static shared_ptr<PushdownEntry> shift(state_t state)
    {
        return shared_ptr<PushdownEntry>(new PushdownEntry(state, StateTypeH()));
    }
    static shared_ptr<PushdownEntry> reduce(ruleid_t rule)
    {
        return shared_ptr<PushdownEntry>(new PushdownEntry(rule, RuleTypeH()));
    }
    static shared_ptr<PushdownEntry> lookahead(shared_ptr<PushdownStateLookup> next)
    {
        return shared_ptr<PushdownEntry>(new PushdownEntry(next));
    }
    static shared_ptr<PushdownEntry> reject()
    {
        return shared_ptr<PushdownEntry>(new PushdownEntry());
    }
    static shared_ptr<PushdownEntry> decide(decision_info_t decision_info)
    {
        return shared_ptr<PushdownEntry>(new PushdownEntry(make_shared<decision_info_t>(std::move(decision_info))));
    }

    ~PushdownEntry()
    {
        if (this->type() == STATE_TYPE_LOOKAHEAD) {
            this->_u.next.~shared_ptr<PushdownStateLookup>();
        } else if (this->type() == STATE_TYPE_DECISION) {
            this->_u.decision_info.~shared_ptr<decision_info_t>();
        }
    }
};

void DCParser::ensure_epsilon_closure()
{
    assert(this->m_real_start_symbol.has_value());

    if (!this->u_nonterm_epsilon_closure.empty())
        return;

    assert(this->u_nonterm_possible_next.empty());

    std::map<charid_t,std::set<ruleid_t>> pvpv;
    for (size_t i=0;i<this->m_rules.size();i++) {
        auto& r = this->m_rules[i];
        pvpv[r.m_lhs].insert(i);
    }

    bool changed = true;
    while (changed) {
        changed = false;

        for (auto& rs: pvpv) {
            const auto rx = rs.second;

            for (const auto r: rx) {
                assert(r < this->m_rules.size());
                const auto& rr = this->m_rules[r];

                if (rr.m_rhs.size() > 0) {
                    auto cx = rr.m_rhs[0];

                    if (pvpv.find(cx) != pvpv.end()) {
                        auto& tx = pvpv[cx];
                        const auto old_size = rs.second.size();
                        rs.second.insert(tx.begin(), tx.end());
                        changed = changed || rs.second.size() != old_size;
                    }
                }
            }
        }
    }

    for (auto& rs: pvpv) {
        auto cr = rs.first;
        auto& rx = rs.second;

        for (auto r: rx) {
            this->u_nonterm_epsilon_closure[cr].push_back(r);
            assert(r < this->m_rules.size());
            auto& rr = this->m_rules[r];

            if (rr.m_rhs.size() > 0) {
                auto cx = rr.m_rhs[0];
                this->u_nonterm_possible_next[cr].insert(cx);
            }
        }
    }
}

set<pair<ruleid_t,size_t>> 
DCParser::stateset_epsilon_closure(const set<pair<ruleid_t,size_t>>& st)
{
    auto retset = st;

    for (auto& s: st) {
        auto& r = this->m_rules[s.first];
        assert(s.second < r.m_rhs.size());
        auto cx = r.m_rhs[s.second];

        if (this->u_nonterm_epsilon_closure.find(cx) != this->u_nonterm_epsilon_closure.end()) {
            for (auto t: this->u_nonterm_epsilon_closure.at(cx)) {
                retset.insert(make_pair(t, 0));
            }
        }
    }

    return retset;
}

set<pair<ruleid_t,size_t>> DCParser::stateset_move(const set<pair<ruleid_t,size_t>>& old, charid_t ch) const
{
    set<pair<ruleid_t,size_t>> ret;
    auto _this = const_cast<DCParser*>(this);
    _this->ensure_epsilon_closure();

    for (auto& p: old) {
        const auto& r = this->m_rules[p.first];
        const auto pos = p.second;

        assert(pos < r.m_rhs.size() || r.m_rhs.empty());

        if (r.m_rhs[pos] == ch) {
            ret.insert(make_pair(p.first, pos + 1));

            if (pos + 1 < r.m_rhs.size()) {
                auto cx = r.m_rhs[pos + 1];
                if (this->u_nonterm_epsilon_closure.find(cx) != this->u_nonterm_epsilon_closure.end()) {
                    for (auto rr: this->u_nonterm_epsilon_closure.at(cx)) {
                        ret.insert(make_pair(rr, 0));
                    }
                }
            }
        }
    }

    return ret;
}

bool DCParser::is_nonterm(charid_t id) const {
    return this->m_nonterms.find(id) != this->m_nonterms.end();
}

set<pair<ruleid_t,size_t>> DCParser::startState() const {
    assert(this->m_real_start_symbol.has_value());

    set<pair<ruleid_t,size_t>> ret;
    auto _this = const_cast<DCParser*>(this);
    _this->ensure_epsilon_closure();

    assert(!this->m_start_symbols.empty() && "must add at least one start symbol");
    const auto ssym = this->m_real_start_symbol.value();
    assert(this->u_nonterm_epsilon_closure.find(ssym) != this->u_nonterm_epsilon_closure.end());
    const auto& start_rules = _this->u_nonterm_epsilon_closure.at(ssym);
    for (auto& start_rule: start_rules) {
        ret.insert(make_pair(start_rule, 0));
    }

    return ret;
}


DCParser::DCParser(bool lookahead_rule_propagation):
    m_lookahead_rule_propagation(lookahead_rule_propagation),
    m_priority(0), 
    m_context(make_unique<DCParserContext>(*this)),
    h_debug_stream(nullptr),
    u_possible_prev_next_computed(false)
{
}

DCParser::~DCParser()
{
}

priority_t DCParser::dec_priority() { return make_shared<DCParserRulePriority>(DCParserRulePriority(++this->m_priority)); }

int DCParser::add_rule_internal(
        charid_t lh, vector<charid_t> rh, vector<bool> rhop,
        reduce_callback_t cb,
        RuleAssocitive associtive,
        decision_t decision, set<size_t> positions,
        priority_t priority)
{
    assert(!this->m_real_start_symbol.has_value());

    this->m_nonterms.insert(lh);
    this->m_symbols.insert(lh);
    if (this->m_terms.find(lh) != this->m_terms.end())
        this->m_terms.erase(lh);

    for (auto r: rh) {
        this->m_symbols.insert(r);
        if (this->m_nonterms.find(r) == this->m_nonterms.end())
            this->m_terms.insert(r);
    }

    size_t ntoken = 0;
    for (auto b: rhop) {
        if (!b)
            ntoken++;
    }
    assert(ntoken == rh.size());

    RuleInfo ri;
    ri.m_lhs = lh;
    ri.m_rhs = rh;
    ri.m_rhs_optional = std::move(rhop);
    ri.m_reduce_callback = cb;
    ri.m_rule_option = std::make_shared<RuleOption>();
    auto ruleopt = ri.m_rule_option;

    ruleopt->associtive = associtive;
    ruleopt->priority = this->m_priority;
    if (priority) ruleopt->priority = priority->priority();
    ruleopt->decision = decision;
    ruleopt->decision_pos = std::move(positions);
    ruleopt->seen = false;
    ruleopt->ruleid = this->m_rules.size() + 0x999;

    this->m_rules.push_back(ri);
    return ruleopt->ruleid;
}

void DCParser::see_dchar(DCharInfo char_)
{
    if (this->h_charinfo.find(char_.id) == this->h_charinfo.end()) {
        this->h_charinfo[char_.id] = char_;
    } else {
        assert(this->h_charinfo[char_.id].id == char_.id);
    }
}

DCharInfo DCParser::get_dchar(charid_t id) const
{
    assert(this->h_charinfo.find(id) != this->h_charinfo.end());
    return this->h_charinfo.at(id);
}

string DCParser::help_rule2str(ruleid_t rule, size_t pos) const
{
    ostringstream oss;
    const auto& r = this->m_rules[rule];
    oss << this->get_dchar(r.m_lhs).name << " -> ";
    for (size_t i = 0; i < r.m_rhs.size(); i++) {
        if (i == pos)
            oss << "* ";
        oss << this->get_dchar(r.m_rhs[i]).name;
        if (i != r.m_rhs.size() - 1)
            oss << " ";
    }

    if (r.m_rule_option->decision)
        oss << " -- " << "?";

    return oss.str();
}

string DCParser::help_print_state(state_t state, size_t count, char paddingchar) const
{
    ostringstream oss;
    assert(state < this->h_state2set.size());
    const auto& ss = this->h_state2set[state];

    assert(ss.size() > 0);
    auto prev_priority = this->m_rules[ss.begin()->first].m_rule_option->priority;
    for (auto& s: ss) {
        assert(this->m_rules.size() > s.first);
        auto& rule = this->m_rules[s.first];

        const auto priority = rule.m_rule_option->priority;
        const auto rulestr = this->help_rule2str(s.first, s.second);
        assert(prev_priority <= priority);
        if (priority != prev_priority) {
            *this->h_debug_stream 
                << string(count, paddingchar)
                << string(rulestr.size(), '-') << endl;;
            prev_priority = priority;
        }

        *this->h_debug_stream
            << string(count, paddingchar)
            << rulestr << endl;
    }

    return oss.str();
}

void DCParser::collect_expected_next_chars(state_t state, std::set<charid_t>& expectedNextTokens) const
{
    const auto& statesets = this->h_state2set.at(state);
    for (auto& p: statesets) {
        const auto& r = this->m_rules[p.first];
        assert(r.m_rhs.size() > p.second);
        auto rx = r.m_rhs.at(p.second);
        if (m_terms.count(rx)) expectedNextTokens.insert(rx);
    }
}

std::set<charid_t> DCParser::get_expected_chars() const
{
    assert(p_state_stack.size() > 0);
    std::set<charid_t> expected;

    if (p_not_finished.has_value()) {
        std::set<state_t> nextStates;

        std::function<void(size_t,const PushdownStateLookup&)> handle_la;
        std::function<void(size_t,charid_t)> handle_feed;

        handle_la = [&](size_t rm, const PushdownStateLookup& lookup) {
            std::set<ruleid_t> reduce_cases;
            for (auto& n: lookup) {
                if (h_charinfo.count(n.first) == 0) continue;
                const auto type = n.second.type();
                assert(type == PushdownEntry::STATE_TYPE_REDUCE || type == PushdownEntry::STATE_TYPE_SHIFT);

                if (type == PushdownEntry::STATE_TYPE_REDUCE) {
                    reduce_cases.insert(n.second.rule());
                } else {
                    nextStates.insert(n.second.state());
                }
            }
            for (auto& ruleid: reduce_cases) {
                const auto& r = m_rules.at(ruleid);
                const auto rn = r.m_rhs.size();
                handle_feed(rm + rn - 1, r.m_lhs);
            }
        };

        handle_feed = [&](size_t rm, charid_t cid) {
            assert(rm <= p_state_stack.size());
            auto& ee = this->m_pds_mapping->val.at(*(p_state_stack.end() - rm - 1)).at(cid);
            if (ee.type() == PushdownEntry::STATE_TYPE_SHIFT) {
                nextStates.insert(ee.state());
            } else if (ee.type() == PushdownEntry::STATE_TYPE_REDUCE) {
                const auto& r = m_rules.at(ee.rule());
                const auto rn = r.m_rhs.size();
                handle_feed(rm + rn - 1, r.m_lhs);
            } else if (ee.type() == PushdownEntry::STATE_TYPE_LOOKAHEAD) {
                handle_la(rm, *ee.lookup());
            }
        };

        handle_la(0, *p_not_finished.value().second->lookup());
        for (auto& s: nextStates) this->collect_expected_next_chars(s, expected);
    } else {
        this->collect_expected_next_chars(p_state_stack.back(), expected);
    }
    return expected;
}

string DCParser::help_when_reject_at(state_t state, charid_t char_) const
{
    ostringstream oss;
    assert(this->h_state2set.size() > state);
    assert(p_state_stack.back() == state);

    oss << endl;
    auto& statesets = this->h_state2set[state];
    set<string> expected;
    for (auto& p: statesets) {
        const auto& r = this->m_rules[p.first];
        assert(r.m_rhs.size() > p.second);

        oss << "    Rule [ " << this->help_rule2str(p.first, p.second) << " ]" << endl;
        auto rx = this->m_rules[p.first].m_rhs[p.second];
        expected.insert(this->get_dchar(rx).name);
    }
    oss << "Expected: ";
    for (auto& e: expected) {
        oss << e << " ";
    }
    oss << endl;
    oss << "But Get Char '" << this->get_dchar(char_).name << "'" << endl;

    return oss.str();
}

void DCParser::help_print_unseen_rules_into_debug_stream() const
{
    set<size_t> unseen_rules;
    for (size_t i=0;i<this->m_rules.size();i++) {
        if (!this->m_rules[i].m_rule_option->seen)
            unseen_rules.insert(i);
    }
    if (unseen_rules.size() > 0 && this->h_debug_stream) {
        *this->h_debug_stream << "WARNING unseen rules("
                              << unseen_rules.size() << "):" << endl;
        *this->h_debug_stream << "total = " << this->m_rules.size() << endl;
        for (auto& r: unseen_rules)
            *this->h_debug_stream << setw(3) << std::setfill('0') << r 
                                  <<  "    " << this->help_rule2str(r, 0) << endl;
        *this->h_debug_stream << endl;
    }
}

void DCParser::setDebugStream(ostream& os)
{
    this->h_debug_stream = &os;
    this->help_print_unseen_rules_into_debug_stream();
}

void DCParser::SetTextinfo(shared_ptr<TextInfo> textinfo)
{
    this->h_textinfo = textinfo;
}

void DCParser::add_rule(
        DCharInfo leftside, std::vector<ParserChar> rightside,
        reduce_callback_t reduce_cb, RuleAssocitive associative,
        decision_t decision, priority_t priority)
{
    this->see_dchar(leftside);
    for (auto& rh: rightside)
        this->see_dchar(rh.info());

    vector<pair<vector<charid_t>,vector<bool>>> _rightsides = { { {}, {} } };
    const auto doubleit = [&]() {
        const auto size =_rightsides.size();

        for (size_t i=0;i<size;i++)
            _rightsides.push_back(_rightsides[i]);
    };

    for (auto& rt: rightside) {
        if (rt.optional()) {
            doubleit();
            assert(_rightsides.size() % 2 == 0);

            const auto half = _rightsides.size() / 2;
            for (size_t i=0;i<half;i++) {
                _rightsides[i].first.push_back(rt.cid());
                _rightsides[i].second.push_back(false);
            }
            for (size_t i=half;i<_rightsides.size();i++) {
                _rightsides[i].second.push_back(true);
            }
        } else {
            for (auto& rset: _rightsides) {
                rset.first.push_back(rt.cid());
                rset.second.push_back(false);
            }
        }
    }

    set<size_t> pos;
    if (decision) {
        for (size_t i=0;i<rightside.size();i++) {
            if (decision->decide_on_pos(i))
                pos.insert(i);
        }

        if (decision->decide_on_end())
            pos.insert(rightside.size());
    }

    for (auto& rset: _rightsides)
    {
        set<size_t> lpos;

        for (auto& p: pos) {
            if (p == 0) {
                lpos.insert(p);
                continue;
            }

            assert(p <= rset.second.size());
            if (rset.second[p-1])
                continue;

            size_t u = 0;
            for (size_t i=0;i<p;i++) {
                if (!rset.second[i])
                    u++;
            }
            assert(u > 0);
            lpos.insert(u);
        }

        this->add_rule_internal(
                leftside.id, rset.first, rset.second,
                reduce_cb, associative, decision, lpos, priority);
    }
}


DCParser& DCParser::operator()(DCharInfo lh, vector<ParserChar> rh,
                               reduce_callback_t cb,
                               RuleAssocitive associtive,
                               decision_t decision, priority_t priority)
{
    this->add_rule(lh, rh, cb, associtive, decision, priority);
    return *this;
}

void DCParser::add_start_symbol(charid_t id) {
    assert(!this->m_real_start_symbol.has_value());

    if (this->m_start_symbols.find(id) != this->m_start_symbols.end()) {
        throw std::runtime_error("start symbol already exists");
    }

    if (!this->is_nonterm(id)) {
        throw std::runtime_error("start symbol is not a nonterminal");
    }

    this->m_start_symbols.insert(id);
}

void DCParser::setup_real_start_symbol()
{
    assert(!this->m_real_start_symbol.has_value());
    const auto start_sym = CharInfo<RealStartSymbol>();

    this->dec_priority();
    for (auto sym: this->m_start_symbols) {
        auto info = this->get_dchar(sym);
        this->add_rule( start_sym, { info }, 
                        [](auto,auto& rn) {
                            assert(rn.size() == 1);
                            auto val = dynamic_pointer_cast<NonTerminal>(rn[0]);
                            assert(val);
                            return make_shared<RealStartSymbol>(val);
                        }, RuleAssocitiveLeft, nullptr, nullptr);
    }

    this->m_real_start_symbol = start_sym.id;
}

void DCParser::generate_table()
{
    this->setup_real_start_symbol();

    std::sort(this->m_rules.begin(), this->m_rules.end(), 
              [](const auto& lhs, const auto& rhs) {
                  return lhs.m_rule_option->priority < rhs.m_rule_option->priority;
              });

    SetStateAllocator<pair<ruleid_t,size_t>> sallocator;
    const auto s_start_state = this->startState();

    const auto start_state = sallocator(s_start_state);
    PushdownStateMappingTX mapping;

    queue<set<pair<ruleid_t,size_t>>> q;
    q.push(s_start_state);
    set<set<pair<ruleid_t,size_t>>> visited( {s_start_state} );

    while(!q.empty()) {
        auto s = q.front();
        q.pop();

        auto state = sallocator(s);
        if (mapping.size() <= state)
            mapping.resize(state+1);
        auto& state_mapping = mapping[state];

        for (auto ch: this->m_symbols) {
            set<set<pair<ruleid_t,size_t>>> next_states;
            auto s_next = this->stateset_move(s, ch);
            auto action = this->state_action(s_next, true, sallocator, next_states);
            assert(action);
            state_mapping[ch] = std::move(*action);

            for (auto& s: next_states) {
                if (visited.find(s) == visited.end()) {
                    q.push(s);
                    visited.insert(s);
                }
            }
        }
    }

    assert(mapping.size() == sallocator.max_state());

    this->m_start_state = start_state;
    this->m_pds_mapping = std::make_shared<PushdownStateMapping>(std::move(mapping));

    this->h_state2set.clear();
    this->h_state2set.resize(sallocator.max_state());
    for (auto& s: sallocator.themap())
        this->h_state2set[s.second] = s.first;

    this->help_print_unseen_rules_into_debug_stream();
}

shared_ptr<PushdownEntry> 
DCParser::state_action(set<pair<ruleid_t,size_t>> s_next,
                       bool evaluate_decision,
                       SetStateAllocator<pair<ruleid_t,size_t>>& sallocator,
                       set<set<pair<ruleid_t,size_t>>>& next_states)
{
    assert(next_states.empty());
    vector<pair<ruleid_t,size_t>> require_eval;
    for (const auto& s: s_next) {
        const auto& rule = this->m_rules[s.first];
        const auto& pos = rule.m_rule_option->decision_pos;
        if (pos.find(s.second) != pos.end())
            require_eval.push_back(s);
    }

    // Eval Action
    if (evaluate_decision && !require_eval.empty())
    {
        assert(require_eval.size() <= 12 && "too many conditions to evaluate");
        PushdownEntry::decision_info_t decision_info;
        decision_info.evals = require_eval;
        auto& eval_action = decision_info.action;

        SubsetOf<pair<ruleid_t,size_t>> 
            e_require_eval(set<pair<ruleid_t,size_t>>(require_eval.begin(), require_eval.end()));

        for (auto s=e_require_eval();s.has_value();s=e_require_eval())
        {
            auto snn = s_next;
            for (auto& r: s.value())
                snn.erase(r);

            set<set<pair<ruleid_t,size_t>>> nx;
            auto action = this->state_action(snn, false, sallocator, nx);
            next_states.insert(nx.begin(), nx.end());
            eval_action[s.value()] = action;
        }

        return PushdownEntry::decide(std::move(decision_info));
    }

    // REJECT
    if (s_next.empty())
        return PushdownEntry::reject();

    set<ruleid_t> completed_rules;
    for (auto& s: s_next) {
        const auto ruleid  = s.first;
        const auto rulepos = s.second;
        assert(this->m_rules.size() > ruleid);
        const auto& rule = this->m_rules[ruleid];

        if (rule.m_rhs.size() == rulepos)
            completed_rules.insert(ruleid);
    }

    // SHIFT
    if (completed_rules.empty()) {
        auto next_state = sallocator(s_next);
        next_states.insert(s_next);
        return PushdownEntry::shift(next_state);
    }

    // ----- PRIORITY -----
    // 1. IF THERE ARE MULTIPLE RULES WITH HIGHEST PRIORITY ( IN COMPLETED CANDIATE SET ) ARE COMPLETED,
    //    THEN REPORT A GRAMMAR ERROR. THE REASON OF THAT IS IF WE CONTINUE BY LOOKAHEAD, IT'S HIGHLY 
    //    POSSIBLE TO GET AT LEAST ONE REDUCE, WHICH IS AN GRAMMAR ERROR, SO WE SIMPLIFIED THIS PROCESS.
    //    BECAUSE OF THAT IN FOLLOWING ANALYSIS, WE CAN ASSERT THERE IS ONE AND ONLY ONE COMPLETED RULE 
    //    WHICH PRIORITIZE OTHER COMPLETED RULES.
    // 2. IF THE HIGHEST PRIORITY COMPLETED RULE IS HIGHEST PRIORITY (CONSIDER PRIORITY AND ASSOCIATIVE)
    //    IN CANDIDATE LIST, THEN JUST REDUCE
    // 3. OTHERWISE, LOOKAHEAD TABLE IS REQUIRED.


    // ----- ASSOCIATIVE ------
    // a + b + c;  => (a + b) + c
    // LEFT LEFT   => reduce
    //
    // a = b = c   => a = (b = c);
    // RIGHT RIGHT => shift
    //
    // LEFT RIGHT  => ?
    // RIGHT LEFT  => ?
    // shold be resolve be priority, so these two cases are not
    // considered, if which presents raising a grammar error.

    // ----- LOOKAHEAD -----
    // COMPUTE THE SET OF CANDIDATES WHICH PRIORITIZE HIGHEST PRIORITY COMPLETED RULE,
    // THEN FOREACH SYMBOL WE CAN DO A STATE TRANSITION ON THE SET.
    // IF THE STATE SET OF RESULT IS NOT EMPTY THEN SHIFT, OTHERWISE REDUCE.

    vector<ruleid_t> v_completed_candidates(
            completed_rules.begin(), completed_rules.end());
    std::sort(v_completed_candidates.begin(), v_completed_candidates.end(),
            [this](ruleid_t a, ruleid_t b) {
            assert(this->m_rules.size() > a);
            assert(this->m_rules.size() > b);
            return this->m_rules[a].m_rule_option->priority <
            this->m_rules[b].m_rule_option->priority;
            });

    const auto completed_highest_priority_rule = v_completed_candidates.front();
    const auto completed_highest_priority = 
        this->m_rules[completed_highest_priority_rule].m_rule_option->priority;
    const auto completed_highest_associtive = 
        this->m_rules[completed_highest_priority_rule].m_rule_option->associtive;

    if (v_completed_candidates.size() > 1) {
        auto  second_pri = v_completed_candidates[1];
        auto& second_pri_rule = this->m_rules[second_pri];

        if (second_pri_rule.m_rule_option->priority == completed_highest_priority) {
            const auto c1 = this->help_rule2str(second_pri, second_pri_rule.m_rhs.size());
            const auto& hr = this->m_rules[completed_highest_priority_rule];
            const auto c2 = this->help_rule2str(completed_highest_priority_rule, hr.m_rhs.size());
            throw ParserGrammarError("conflict rule: \n    " + c1 + " and \n    " + c2);
        }
    }

    set<pair<ruleid_t,size_t>> v_incompleted_candidates;
    for (auto& r: s_next) {
        assert(this->m_rules.size() > r.first);
        auto& rule = this->m_rules[r.first];
        // TODO
        if (rule.m_rhs.size() > r.second && 
                (rule.m_rule_option->priority < completed_highest_priority || 
                 (rule.m_rule_option->priority == completed_highest_priority &&
                  completed_highest_associtive == RuleAssocitiveRight &&
                  rule.m_rule_option->associtive == RuleAssocitiveRight)))
        {
            v_incompleted_candidates.insert(r);
        }
    }

    {
        auto rx = this->m_rules[completed_highest_priority_rule];
        rx.m_rule_option->seen = true;
    }

    // REDUCE
    if (v_incompleted_candidates.empty())
        return PushdownEntry::reduce(completed_highest_priority_rule);

    if (this->m_lookahead_rule_propagation)
        v_incompleted_candidates = this->stateset_epsilon_closure(v_incompleted_candidates);

    // LOOKAHEAD
    PushdownStateLookup lookahead_table;
    lookahead_table[GetEOFChar()] = *PushdownEntry::reduce(completed_highest_priority_rule);
    for (auto& s: this->m_terms) {
        auto s_next = this->stateset_move(v_incompleted_candidates, s);

        if (s_next.empty()) {
            // REDUCE
            lookahead_table[s] = *PushdownEntry::reduce(completed_highest_priority_rule);
        } else {
            // SHIFT
            const auto lookahead_shift_state = sallocator(v_incompleted_candidates);
            lookahead_table[s] = *PushdownEntry::shift(lookahead_shift_state);
            next_states.insert(v_incompleted_candidates);
        }
    }

    return PushdownEntry::lookahead(std::make_shared<PushdownStateLookup>(std::move(lookahead_table)));
}

void DCParser::compute_posible_prev_next()
{
    // ensure transition table is computed
    assert(this->m_real_start_symbol.has_value());

    map<charid_t,set<ruleid_t>> lhschar2rules;
    for (size_t i=0;i<this->m_rules.size();i++) {
        auto& rule = this->m_rules[i];
        lhschar2rules[rule.m_lhs].insert(i);
    }

    map<charid_t,set<charid_t>> last_chars_to_create_this_char;
    map<charid_t,set<charid_t>> first_chars_to_create_this_char;
    for (auto _char: this->m_symbols) {
        last_chars_to_create_this_char[_char].insert(_char);
        first_chars_to_create_this_char[_char].insert(_char);

        for (auto ruleid: lhschar2rules[_char]) {
            auto& rule = this->m_rules[ruleid];
            auto& rhs = rule.m_rhs;
            assert(rhs.size() > 0);

            auto first_char = rhs.front();
            auto last_char = rhs.back();

            last_chars_to_create_this_char[_char].insert(last_char);
            first_chars_to_create_this_char[_char].insert(first_char);
        }
    }
    last_chars_to_create_this_char = 
    transitive_closure(last_chars_to_create_this_char);
    first_chars_to_create_this_char =
    transitive_closure(first_chars_to_create_this_char);

    map<charid_t,set<charid_t>> prev, next;
    for (auto& rule: this->m_rules) {
        auto& rhs = rule.m_rhs;
        assert(rhs.size() > 0);

        auto p = rhs.front();
        for (size_t i=1;i<rhs.size();p=rhs[i++]) {
            auto c = rhs[i];

            for (auto cx: first_chars_to_create_this_char[c]) {
                auto& kn = last_chars_to_create_this_char[p];
                prev[cx].insert(kn.begin(), kn.end());
            }

            for (auto bx: last_chars_to_create_this_char[p]) {
                auto& kn = first_chars_to_create_this_char[c];
                next[bx].insert(kn.begin(), kn.end());
            }
        }
    }

    assert(this->u_next_possible_token_of.empty());
    assert(this->u_prev_possible_token_of.empty());

    for (auto& tx: prev) {
        if (this->is_nonterm(tx.first))
            continue;

        for (auto& nx: tx.second) {
            if (this->is_nonterm(nx))
                continue;

            this->u_prev_possible_token_of[tx.first].insert(nx);
        }
    }

    for (auto& tx: next) {
        if (this->is_nonterm(tx.first))
            continue;

        for (auto& nx: tx.second) {
            if (this->is_nonterm(nx))
                continue;

            this->u_next_possible_token_of[tx.first].insert(nx);
        }
    }
}

set<charid_t> DCParser::next_possible_token_of(charid_t cid) const
{
    if (this->m_symbols.find(cid) == this->m_symbols.end())
        throw ParserError("unknown symbol");

    if (this->u_possible_prev_next_computed) {
        if (this->u_next_possible_token_of.find(cid) != this->u_next_possible_token_of.end())
            return this->u_next_possible_token_of.at(cid);
        else
            return set<charid_t>();
    }

    auto _this= const_cast<DCParser*>(this);
    _this->compute_posible_prev_next();

    if (this->u_next_possible_token_of.find(cid) != this->u_next_possible_token_of.end())
        return this->u_next_possible_token_of.at(cid);
    else
        return set<charid_t>();
}

set<charid_t> DCParser::prev_possible_token_of(charid_t cid) const
{
    if (this->m_symbols.find(cid) == this->m_symbols.end())
        throw ParserError("unknown symbol");

    if (this->u_possible_prev_next_computed) {
        if (this->u_prev_possible_token_of.find(cid) != this->u_prev_possible_token_of.end())
            return this->u_prev_possible_token_of.at(cid);
        else
            return set<charid_t>();
    }

    auto _this= const_cast<DCParser*>(this);
    _this->compute_posible_prev_next();

    if (this->u_prev_possible_token_of.find(cid) != this->u_prev_possible_token_of.end())
        return this->u_prev_possible_token_of.at(cid);
    else
        return set<charid_t>();
}

DCharInfo DCParser::query_charinfo(charid_t id) const
{
    if (this->h_charinfo.find(id) != this->h_charinfo.end())
        return this->h_charinfo.at(id);

    throw std::runtime_error("charinfo not found");
}

void DCParser::do_shift(state_t state, dchar_t char_)
{
    this->p_state_stack.push_back(state);
    this->p_char_stack.push_back(char_);
    if (this->h_debug_stream) {
        assert(this->h_state2set.size() > state);
        auto& ss = this->h_state2set[state];
        *this->h_debug_stream 
            << "    do_shift, newstate = " << state <<  endl
            << "      stateset(" << ss.size() << "):" << endl
            << this->help_print_state(state, 8, ' ');
    }
}

optional<dchar_t> DCParser::do_reduce(ruleid_t ruleid, dchar_t char_)
{
    assert(!this->p_state_stack.empty());
    assert(this->m_rules.size() > ruleid);

    auto& rule = this->m_rules[ruleid];
    this->p_char_stack.push_back(char_);
    this->p_state_stack.resize(this->p_state_stack.size() + 1);

    auto& rn = rule.m_rhs;
    assert(rn.size() > 0);
    assert(rn.size() <= p_char_stack.size());
    assert(rn.size() <= p_state_stack.size());

    p_state_stack.resize(p_state_stack.size() - rn.size());
    auto ei = p_char_stack.end();
    std::vector<dchar_t> rhs_tokens(ei - rn.size(), ei);
    p_char_stack.resize(p_char_stack.size() - rn.size());

    assert(rn.size() == rhs_tokens.size());
    vector<dchar_t> rhs_tokens_with_optional;
    size_t ni = 0;
    for (auto opt: rule.m_rhs_optional) {
        if (opt) {
            rhs_tokens_with_optional.push_back(nullptr);
        } else {
            rhs_tokens_with_optional.push_back(rhs_tokens[ni++]);
        }
    }
    assert(ni == rhs_tokens.size());

    for (size_t i=0; i<rn.size(); ++i) {
        auto cid = rn[i];
        auto c   = rhs_tokens[i];
        assert(cid == c->charid());
    }

    auto nonterm = rule.m_reduce_callback(this->m_context, rhs_tokens_with_optional);
    const string expect = this->get_dchar(rule.m_lhs).name;
    if (nonterm == nullptr)
        throw ParserError("ReduceCallback: expect a valid token, but get nullptr, expect: " + expect);

    if (nonterm->charid() != rule.m_lhs) {
        const string get = this->get_dchar(nonterm->charid()).name;
        throw ParserError("ReduceCallback: expect a valid token, but get a token with different charid, get: " + get + ", expect: " + expect);
    }
    for (auto c: rhs_tokens)
        nonterm->contain(*c);

    if (this->h_debug_stream != nullptr) {
        *this->h_debug_stream << "    do_reduce: "
                              << this->help_rule2str(ruleid, -1)
                              << endl;
    }

    if (nonterm->charid() == this->m_real_start_symbol.value()) {
        this->p_char_stack.push_back(nonterm);
        return nullopt;
    } else {
        return nonterm;
    }

    return nonterm;
}

optional<dchar_t> DCParser::handle_lookahead(dctoken_t token)
{
    assert(this->p_not_finished.has_value());
    assert(!this->p_state_stack.empty());

    const auto cstate = this->p_state_stack.back();
    auto nf = this->p_not_finished.value();
    auto ptoken = nf.first;

    if (this->h_debug_stream) {
        *this->h_debug_stream << "  handle_lookahead [ "
                              << ptoken->charname()
                              << ", ahead token is " << token->charname()
                              << " ]" << endl;
    }

    const auto& entry = *nf.second;
    assert(entry.type() == PushdownEntry::STATE_TYPE_LOOKAHEAD);
    const auto& state_lookup = entry.lookup();
    if (state_lookup->find(token->charid()) == state_lookup->end())
        throw ParserUnknownToken("handle_lookahead(): unknown lookahead char: " + string(token->charname()));

    const auto& state_entry = state_lookup->at(token->charid());
    assert(state_entry.type() == PushdownEntry::STATE_TYPE_REDUCE ||
           state_entry.type() == PushdownEntry::STATE_TYPE_SHIFT);

    this->p_not_finished = nullopt;
    if (state_entry.type() == PushdownEntry::STATE_TYPE_REDUCE) {
        return this->do_reduce(state_entry.rule(), ptoken);
    } else {
        this->do_shift(state_entry.state(), ptoken);
        return nullopt;
    }
}

void DCParser::feed_internal(dchar_t char_)
{
    assert(!this->p_not_finished.has_value());
    assert(!this->p_state_stack.empty());
    
    if (this->h_debug_stream) {
        *this->h_debug_stream << "  feed_internal: "
                              << char_->charname()
                              << endl;
    }

    const auto cstate = this->p_state_stack.back();
    auto& mapping = this->m_pds_mapping->val;
    assert(mapping.size() > cstate);
    const auto& lookup = mapping[cstate];
    const auto charid = char_->charid();

    if (lookup.find(charid) == lookup.end())
        throw ParserUnknownToken("feed_internal(): unknown char: " + string(char_->charname()));

    const auto& _entry = lookup.at(charid);
    const PushdownEntry* ptrentry = &_entry;

    if (_entry.type() == PushdownEntry::STATE_TYPE_DECISION)
    {
        auto decision = _entry.decision();
        set<pair<ruleid_t,size_t>> eliminated_rules;

        for (auto& ev: decision->evals) {
            assert(ev.first < this->m_rules.size());
            auto& rule = this->m_rules[ev.first];
            assert(ev.second <= rule.m_rhs.size());
            assert(rule.m_rule_option->decision_pos.find(ev.second) != 
                   rule.m_rule_option->decision_pos.end());

            vector<dchar_t> rhs_tokens;
            assert(this->p_char_stack.size() + 1 >= ev.second);
            if (ev.second > 1) {
                rhs_tokens.resize(ev.second - 1);
                std::copy(this->p_char_stack.end() - (ev.second - 1),
                          this->p_char_stack.end(),
                          rhs_tokens.begin());
            }
            if (ev.second > 0)
                rhs_tokens.push_back(char_);

            vector<dchar_t> rhs_tokens_with_optional;
            size_t vn = 0;
            for (size_t vn=0,vo=0; vn<rhs_tokens.size(); ++vo) {
                assert(vo < rule.m_rhs_optional.size());
                const auto opt = rule.m_rhs_optional[vo];

                if (opt) {
                    rhs_tokens_with_optional.push_back(nullptr);
                } else {
                    rhs_tokens_with_optional.push_back(rhs_tokens[vn++]);
                }
            }

            auto decision = rule.m_rule_option->decision;
            assert(decision);

            if (!decision->decide(this->getContext(), rhs_tokens_with_optional, this->p_char_stack))
                eliminated_rules.insert(ev);
        }

        const auto& action = decision->action;
        assert(action.find(eliminated_rules) != action.end());
        ptrentry = action.at(eliminated_rules).get();
    }

    const auto& entry = *ptrentry;
    switch (entry.type()) {
        case PushdownEntry::STATE_TYPE_SHIFT:
            this->do_shift(entry.state(), char_);
            break;
        case PushdownEntry::STATE_TYPE_REDUCE: {
            auto nc = this->do_reduce(entry.rule(), char_);
            if (nc.has_value())
                this->feed_internal(nc.value());
         }  break;
        case PushdownEntry::STATE_TYPE_LOOKAHEAD:
            this->p_not_finished = make_pair(char_,&entry);
            if (this->h_debug_stream) {
                *this->h_debug_stream << "    require lookahead" << endl;
            }
            break;
        case PushdownEntry::STATE_TYPE_REJECT:
        {
            string posinfo;
            auto pt = dynamic_pointer_cast<LexerToken>(char_);
            if (pt && this->h_textinfo)
                posinfo = this->h_textinfo->row_col_str(*pt);

            throw ParserRejectTokenError(this->help_when_reject_at(cstate, char_->charid()) + posinfo);
            break;
        }
        default:
            assert(false && "unexpected action type");
    }
}

void DCParser::feed(dctoken_t token)
{
    assert(this->m_pds_mapping);
    assert(this->m_start_state.has_value());

    if (this->p_state_stack.empty()) {
        this->p_state_stack.push_back(this->m_start_state.value());

        if (this->h_debug_stream) {
            const auto startstate = this->m_start_state.value();
            const auto& ss = this->h_state2set[startstate];
            *this->h_debug_stream
                << "number of states = " << this->h_state2set.size() << endl
                << "start_state = " << startstate << endl
                << "      stateset(" << ss.size() << "): " << endl;
            *this->h_debug_stream
                << this->help_print_state(startstate, 8, ' ') << endl;
        }
    }

    if (this->m_preAction) {
        auto a = this->m_preAction(this->p_char_stack, token);
        if (a.has_value()) {
            token = a.value();
        } else {
            return;
        }
    }

    if (this->h_debug_stream) {
        *this->h_debug_stream << endl;
        *this->h_debug_stream << "feed: " << token->charname() << endl;
    }

    try {
        m_prevSave.push_back(token);
        if (m_need_recover) {
            auto rt = this->recover_from_reject();
            if (rt.has_value()) {
                auto err = m_need_recover;
                m_need_recover = std::nullopt;
                if (!rt.value()) {
                    throw err;
                }
            } else {
                return;
            }
        }

        while (!m_prevSave.empty()) {
            auto tt = m_prevSave.front();
            m_prevSave.erase(m_prevSave.begin());
            while (this->p_not_finished.has_value()) {
                auto v = this->handle_lookahead(tt);

                if (v.has_value())
                    this->feed_internal(v.value());
            }

            this->feed_internal(tt);
        }
    } catch (ParserRejectTokenError& err) {
        assert(!m_need_recover.has_value());
        m_need_recover = err;
        m_prevSave.push_back(token);
        auto rt = this->recover_from_reject();
        if (rt.has_value()) {
            m_need_recover = std::nullopt;
            if (!rt.value()) {
                throw err;
            }
        }
    }

    if (!m_need_recover.has_value() && !m_prevSave.empty()) {
        auto t = m_prevSave.front();
        m_prevSave.erase(m_prevSave.begin());
        this->feed(t);
    }
}

std::optional<bool> DCParser::recover_from_reject()
{
    if (m_recFn) {
        const auto ans = m_recFn(p_char_stack, m_prevSave);
        if (ans.has_value()) {
            assert(m_prevSave.size() <= ans.value().second);
            m_prevSave.erase(m_prevSave.begin(), m_prevSave.begin() + ans.value().second);

            const auto rm = ans.value().first;
            if (rm >= 0) {
                assert(p_state_stack.size() > rm);
                assert(p_char_stack.size() > rm);
                p_state_stack.resize(p_state_stack.size() - rm);
                p_char_stack.resize(p_char_stack.size() - rm);
                return true;
            } else {
                return false;
            }
        }
    } else {
        return false;
    }
    return std::nullopt;
}

dnonterm_t DCParser::end()
{
    if (this->p_state_stack.empty())
        throw ParserError("nothing be parsed");

    assert(!this->p_state_stack.empty());
    assert(!this->p_char_stack.empty() || this->p_not_finished.has_value());

    auto eos = std::make_shared<EOFChar>();
    m_prevSave.push_back(eos);
    if (m_need_recover) {
        auto rt = this->recover_from_reject();
        auto err = m_need_recover;
        m_need_recover = std::nullopt;
        if (!rt.has_value() || !rt.value()) {
            throw err;
        }
    }

    while (!m_prevSave.empty()) {
        auto tt = m_prevSave.front();
        m_prevSave.erase(m_prevSave.begin());
        while (this->p_not_finished.has_value()) {
            auto v = this->handle_lookahead(tt);

            if (v.has_value())
                this->feed_internal(v.value());

            if (tt != eos) this->feed_internal(tt);
        }
    }

    assert (!this->p_char_stack.empty());
    if (this->p_char_stack.size() != 1) {
        std::string charstackdump;
        for (auto& c: p_char_stack) {
            charstackdump += "  ";
            charstackdump += c->charname();
            charstackdump += "\n";
        }
        throw ParserSyntaxError("unexpected end of token stream, char stack:\n" + charstackdump);
    }

    assert(this->p_state_stack.size() >= 1);
    if (this->p_state_stack.size() > 1)
        throw ParserSyntaxError("unexpected end of token stream, state");

    auto char_ = this->p_char_stack.back();
    auto realstart = dynamic_pointer_cast<RealStartSymbol>(char_);
    assert(realstart && "should be real start symbol");

    return realstart->sym;
}

dnonterm_t DCParser::parse(ISimpleLexer& lexer)
{
    assert(this->m_pds_mapping);
    assert(this->m_start_state.has_value());
    assert(this->p_state_stack.empty());

    while (!lexer.end()) {
        auto token = lexer.next();
        this->feed(token);
    }

    return this->end();
}

void DCParser::reset()
{
    this->p_char_stack.clear();
    this->p_state_stack.clear();
    this->p_not_finished = nullopt;
}


RuleDecisionFunction::RuleDecisionFunction(decider_t decider, std::set<size_t> positions, bool on_end):
    m_decider(decider), m_positions(positions), m_on_end(on_end)
{
}

bool RuleDecisionFunction::decide_on_pos(size_t pos) const {
    return this->m_positions.find(pos) != this->m_positions.end();
}
bool RuleDecisionFunction::decide_on_end() const {
    return this->m_on_end;
}
bool RuleDecisionFunction::decide(pcontext_t ctx, const vector<dchar_t>& cx, const vector<dchar_t>& char_stack)
{
    return this->m_decider(ctx, cx, char_stack);
}
