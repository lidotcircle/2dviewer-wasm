#ifndef _DC_PARSER_DCUTF8_HPP_
#define _DC_PARSER_DCUTF8_HPP_

#include <string>
#include <vector>
#include <assert.h>


class UTF8Encoder {
public:
    UTF8Encoder() = default;
    std::string encode(int);

    template<typename Iterator>
    static std::string strencode(Iterator begin, Iterator end)
    {
        UTF8Encoder encoder;
        std::string result;
        for (Iterator it = begin; it != end; ++it)
            result += encoder.encode(*it);
        return result;
    }
};

union UTF8CodePoint {
private:
    struct {
        bool presented;
    } emptyval;
    struct {
        bool presented;
        int val;
    } codepoint;
public:
    inline UTF8CodePoint(): emptyval({false}) {}
    inline UTF8CodePoint(int cp): codepoint({true, cp}) {}

    inline bool presented() const { return this->codepoint.presented; }
    inline int  getval() const { assert(this->presented()); return this->codepoint.val; }
};

class UTF8Decoder {
private:
    std::vector<char> m_buffer;

public:
    UTF8Decoder() = default;
    UTF8CodePoint decode(char c);
    inline size_t buflen() {return m_buffer.size(); }
    static std::vector<int> strdecode(const std::string&);
};

#endif // _DC_PARSER_DCUTF8_HPP_
