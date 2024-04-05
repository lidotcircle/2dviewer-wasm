#include "parser/parser_error.h"


ParserError::ParserError(const std::string& what) : std::runtime_error(what) {}

ParserGrammarError::ParserGrammarError(const std::string& what) : ParserError(what) {}

ParserSyntaxError::ParserSyntaxError(const std::string& what) : ParserError(what) {}

ParserRejectTokenError::ParserRejectTokenError(const std::string& what) : ParserSyntaxError(what) {}

ParserUnknownToken::ParserUnknownToken(const std::string& what) : ParserError(what) {}

