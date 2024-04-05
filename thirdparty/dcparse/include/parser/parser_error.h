#ifndef _DC_PARSER_ERROR_H_
#define _DC_PARSER_ERROR_H_

#include <stdexcept>

class ParserError : public std::runtime_error {
public:
    ParserError(const std::string& what);
};

class ParserGrammarError : public ParserError {
public:
    ParserGrammarError(const std::string& what);
};

class ParserSyntaxError : public ParserError {
public:
    ParserSyntaxError(const std::string& what);
};

class ParserRejectTokenError : public ParserSyntaxError {
public:
    ParserRejectTokenError(const std::string& what);
};

class ParserUnknownToken : public ParserError {
public:
    ParserUnknownToken(const std::string& what);
};

#endif // _DC_PARSER_ERROR_H_
