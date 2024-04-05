#ifndef _LEXER_LEXER_ERROR_H_
#define _LEXER_LEXER_ERROR_H_

#include <stdexcept>

class LexerError : public std::runtime_error {
public:
    LexerError(const std::string& what);
};

#endif // _LEXER_LEXER_ERROR_H_
