#include <regex/regex_utf8.h>
#include <regex/regex.hpp>
#include <assert.h>
#include <stdexcept>
using namespace std;


RegExpUTF8::RegExpUTF8(const string& pattern): SimpleRegExp<int>(UTF8Decoder::strdecode(pattern))
{
}

void RegExpUTF8::feed(char c)
{
    auto cp = this->m_decoder.decode(c);
    if (cp.presented())
        this->feed(cp.getval());
}

void RegExpUTF8::feed(int c)
{
    assert(this->m_decoder.buflen() == 0);
    SimpleRegExp<int>::feed(c);
}

bool RegExpUTF8::test(const std::string& str) {
    return this->test(str.begin(), str.end());
}

RegExpUTF8 RegExpUTF8::compiled(const string& pattern)
{
    auto reg = RegExpUTF8(pattern);
    reg.compile();
    return reg;
}
