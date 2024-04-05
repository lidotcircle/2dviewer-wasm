#ifndef _LEXER_TOKEN_H_
#define _LEXER_TOKEN_H_

#include <string>
#include <type_traits>
#include <optional>
#include "./text_info.h"

class DChar: public TextRangeEntity  {
public:
    DChar();
    DChar(TextRange info);

    virtual const char* charname() const;
    virtual size_t charid() const;
    virtual ~DChar() = default;
};

struct DCharInfo {
    size_t id;
    const char* name;
};

template<typename T,typename = std::enable_if_t<std::is_base_of<DChar,T>::value>>
DCharInfo CharInfo() {
    auto& typeinfo = typeid(T);
    return { typeinfo.hash_code(), typeinfo.name() };
}

template<typename T, typename = std::enable_if_t<std::is_base_of<DChar,T>::value>>
size_t CharID() { return CharInfo<T>().id; }

using TextRange = TextRangeEntity::TextRange;
class LexerToken : public DChar {
public:
    LexerToken();
    LexerToken(TextRange info);

    virtual ~LexerToken() = default;
};

#endif // _LEXER_TOKEN_H_
