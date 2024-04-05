#pragma once
#include "gobject.h"
#include <memory>
#include <string>


class DCParser;
template<typename T>
class Lexer;

namespace M2V {

class GObjectParser {
public:
    GObjectParser();

    std::vector<std::unique_ptr<M2V::GObject>>
    parse(const std::string& str) const;

private:
    std::unique_ptr<Lexer<int>> m_lexer;
    std::unique_ptr<DCParser>    m_parser;

};

}
