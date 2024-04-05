#ifndef _DC_PARSER_REGEX_CHAR_HPP_
#define _DC_PARSER_REGEX_CHAR_HPP_

#include <limits>
#include <string>
#include <vector>


template<typename CharT>
struct character_traits {
    using char_type = CharT;

    constexpr static char_type LPAREN      = '(';
    constexpr static char_type EXCLAMATION = '!';
    constexpr static char_type RPAREN      = ')';
    constexpr static char_type LBRACKET    = '[';
    constexpr static char_type CARET       = '^';
    constexpr static char_type DASH        = '-';
    constexpr static char_type RBRACKET    = ']';
    constexpr static char_type OR          = '|';
    constexpr static char_type STAR        = '*';
    constexpr static char_type BACKSLASH   = '\\';

    constexpr static char_type LBRACE      = '{';
    constexpr static char_type COMMA       = ',';
    constexpr static char_type RBRACE      = '}';
    constexpr static char_type QUESTION    = '?';
    constexpr static char_type PLUS        = '+';
    constexpr static char_type DOT         = '.';

    constexpr static char_type ZERO        = '0';
    constexpr static char_type ONE         = '1';
    constexpr static char_type TWO         = '2';
    constexpr static char_type THREE       = '3';
    constexpr static char_type FOUR        = '4';
    constexpr static char_type FIVE        = '5';
    constexpr static char_type SIX         = '6';
    constexpr static char_type SEVEN       = '7';
    constexpr static char_type EIGHT       = '8';
    constexpr static char_type NINE        = '9';

    constexpr static char_type LOWER_A     = 'a';
    constexpr static char_type LOWER_B     = 'b';
    constexpr static char_type LOWER_C     = 'c';
    constexpr static char_type LOWER_D     = 'd';
    constexpr static char_type LOWER_E     = 'e';
    constexpr static char_type LOWER_F     = 'f';
    constexpr static char_type LOWER_G     = 'g';
    constexpr static char_type LOWER_H     = 'h';
    constexpr static char_type LOWER_I     = 'i';
    constexpr static char_type LOWER_J     = 'j';
    constexpr static char_type LOWER_K     = 'k';
    constexpr static char_type LOWER_L     = 'l';
    constexpr static char_type LOWER_M     = 'm';
    constexpr static char_type LOWER_N     = 'n';
    constexpr static char_type LOWER_O     = 'o';
    constexpr static char_type LOWER_P     = 'p';
    constexpr static char_type LOWER_Q     = 'q';
    constexpr static char_type LOWER_R     = 'r';
    constexpr static char_type LOWER_S     = 's';
    constexpr static char_type LOWER_T     = 't';
    constexpr static char_type LOWER_U     = 'u';
    constexpr static char_type LOWER_V     = 'v';
    constexpr static char_type LOWER_W     = 'w';
    constexpr static char_type LOWER_X     = 'x';
    constexpr static char_type LOWER_Y     = 'y';
    constexpr static char_type LOWER_Z     = 'z';

    constexpr static char_type UPPER_A     = 'A';
    constexpr static char_type UPPER_B     = 'B';
    constexpr static char_type UPPER_C     = 'C';
    constexpr static char_type UPPER_D     = 'D';
    constexpr static char_type UPPER_E     = 'E';
    constexpr static char_type UPPER_F     = 'F';
    constexpr static char_type UPPER_G     = 'G';
    constexpr static char_type UPPER_H     = 'H';
    constexpr static char_type UPPER_I     = 'I';
    constexpr static char_type UPPER_J     = 'J';
    constexpr static char_type UPPER_K     = 'K';
    constexpr static char_type UPPER_L     = 'L';
    constexpr static char_type UPPER_M     = 'M';
    constexpr static char_type UPPER_N     = 'N';
    constexpr static char_type UPPER_O     = 'O';
    constexpr static char_type UPPER_P     = 'P';
    constexpr static char_type UPPER_Q     = 'Q';
    constexpr static char_type UPPER_R     = 'R';
    constexpr static char_type UPPER_S     = 'S';
    constexpr static char_type UPPER_T     = 'T';
    constexpr static char_type UPPER_U     = 'U';
    constexpr static char_type UPPER_V     = 'V';
    constexpr static char_type UPPER_W     = 'W';
    constexpr static char_type UPPER_X     = 'X';
    constexpr static char_type UPPER_Y     = 'Y';
    constexpr static char_type UPPER_Z     = 'Z';

    constexpr static char_type QUOTE       = '\'';
    constexpr static char_type DQUOTE      = '"';

    constexpr static char_type SPACE       = ' ';
    constexpr static char_type TAB         = '\t';
    constexpr static char_type NEWLINE     = '\n';
    constexpr static char_type RETURN      = '\r';

    constexpr static char_type FORMFEED    = '\f';
    constexpr static char_type VTAB        = '\v';

    constexpr static char_type EMPTY_CHAR = std::numeric_limits<char_type>::min();
    constexpr static char_type MIN = std::numeric_limits<char_type>::min() + 1;
    constexpr static char_type MAX = std::numeric_limits<char_type>::max();
};

template<typename CharT>
std::string char_to_string(CharT c) {
    return std::to_string(c);;
}
template<>
std::string char_to_string<char>(char c);

template<typename CharT>
class RegexPatternChar {
private:
    using traits = character_traits<CharT>;
    bool m_escaping;
    CharT m_char;

    RegexPatternChar(bool escaping, CharT c) : m_escaping(escaping), m_char(c) {}

public:
    RegexPatternChar() : m_escaping(false), m_char(traits::EMPTY_CHAR) {}

    static RegexPatternChar<CharT> unescape(CharT c) {
        return RegexPatternChar<CharT>(false, c);
    }
    static RegexPatternChar<CharT> escape(CharT c) {
        return RegexPatternChar<CharT>(true, c);
    }

    bool is_escaped() const {
        return m_escaping;
    }

    CharT get() const {
        return m_char;
    }

    std::string to_string() const {
        if (m_escaping) {
            return char_to_string(traits::BACKSLASH) + char_to_string(m_char);
        } else {
            return char_to_string(m_char);
        }
    }
};

template<typename CharT>
std::string pattern_to_string(std::vector<RegexPatternChar<CharT>> pattern) {
    std::string result;
    for (auto c : pattern) {
        result += c.to_string();
    }
    return result;
}

#endif // _DC_PARSER_REGEX_CHAR_HPP_
