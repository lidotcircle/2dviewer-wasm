#include "lexer/lexer_error.h"


LexerError::LexerError(const std::string& what) : std::runtime_error(what) {}

