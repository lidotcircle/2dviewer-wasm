#include "regex/regex_internal.hpp"


template<>
std::string char_to_string<char>(char c) {
    return std::string(1, c);
}
