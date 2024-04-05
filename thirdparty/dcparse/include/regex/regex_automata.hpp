#ifndef _DC_PARSER_REGEX_AUTOMATA_HPP_
#define _DC_PARSER_REGEX_AUTOMATA_HPP_

#include <stdexcept>
#include <string>
#include <vector>
#include "./regex_char.hpp"


template<typename Iterator>
struct is_forward_iterator {
    static constexpr bool value = std::is_base_of<
        std::forward_iterator_tag,
        typename std::iterator_traits<Iterator>::iterator_category
    >::value || std::is_same<
        std::forward_iterator_tag,
        typename std::iterator_traits<Iterator>::iterator_category
    >::value;
};
template<typename Iterator, typename valuetype>
struct iterator_value_type_is_same {
    static constexpr bool value = std::is_same<
        typename std::iterator_traits<Iterator>::value_type,
        valuetype
    >::value;
};

template<typename CharT>
class AutomataMatcher {
private:
    using traits = character_traits<CharT>;
    using char_type = CharT;

public:
    virtual void feed(char_type c) = 0;
    virtual bool match() const = 0;
    virtual bool dead() const = 0;
    virtual void reset() = 0;

    template<typename Iterator, typename = typename std::enable_if<
        is_forward_iterator<Iterator>::value &&
        iterator_value_type_is_same<Iterator,char_type>::value,void>::type>
    bool test(Iterator begin, Iterator end) {
        this->reset();
        for (auto it = begin; it != end; ++it)
            feed(*it);
        return match();
    }

    bool test(const std::vector<char_type>& str) {
        return test(str.begin(), str.end());
    }

    bool test(const std::basic_string<char_type>& str) {
        return test(str.begin(), str.end());
    }

    virtual ~AutomataMatcher() = default;
};

template<size_t Begin = 0, size_t End = std::numeric_limits<size_t>::max()>
class StateAllocator {
private:
    static_assert(Begin < End, "Begin must be less than End");
    size_t last_state;

public:
    StateAllocator() : last_state(Begin) {}

    size_t newstate() {
        if (last_state == End)
            throw std::runtime_error("Too many states");

        return last_state++;
    }
};

#endif // _DC_PARSER_REGEX_AUTOMATA_HPP_
