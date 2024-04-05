#include "dcutf8.h"
#include <stdexcept>
using namespace std;


string UTF8Encoder::encode(int val)
{
    string result;
    uint32_t c = val;

    if (c < 0x80) {
        result.push_back(c);
    } else if (c < 0x800) {
        result.push_back(0b11000000 | (c >> 6));
        result.push_back(0b10000000 | (c & 0b00111111));
    } else if (c < 0x10000) {
        result.push_back(0b11100000 | (c >> 12));
        result.push_back(0b10000000 | ((c >> 6) & 0b00111111));
        result.push_back(0b10000000 | (c & 0b00111111));
    } else if (c < 0x110000) {
        result.push_back(0b11110000 | (c >> 18));
        result.push_back(0b10000000 | ((c >> 12) & 0b00111111));
        result.push_back(0b10000000 | ((c >> 6) & 0b00111111));
        result.push_back(0b10000000 | (c & 0b00111111));
    } else {
        return result;
    }

    return result;
}

constexpr int utf8_bound[4][2] = {
    {0x00,    0x7f},
    {0x80,    0x7ff},
    {0x800,   0xffff},
    {0x10000, 0x10ffff},
};

UTF8CodePoint UTF8Decoder::decode(char cc)
{
    this->m_buffer.push_back(cc);
    auto& str = this->m_buffer;

    unsigned char c = str[0];
    int val, len = 0;
    if ((c & 0b10000000) == 0) {
        len = 0;
        val = c;
    } else if ((c & 0b11111000) == 0b11110000) {
        len = 3;
        val = (c & 0b00000111) << 18;
    } else if ((c & 0b11110000) == 0b11100000) {
        len = 2;
        val = (c & 0b00001111) << 12;
    } else if ((c & 0b11100000) == 0b11000000) {
        len = 1;
        val = (c & 0b00011111) << 6;
    } else {
        throw runtime_error("Invalid UTF-8 sequence");
    }

    if (len >= str.size()) 
        return UTF8CodePoint();

    for (int j = 0; j < len; ++j) {
        c = str[1+j];
        if ((c & 0b11000000) != 0b10000000)
            throw runtime_error("Invalid UTF-8 sequence");

        val |= (c & 0b00111111) << (6 * (len - j - 1));
    }

    auto lb = utf8_bound[len][0];
    auto ub = utf8_bound[len][1];

    if (val < lb || val > ub)
        throw runtime_error("Invalid UTF-8 sequence: unexpected codepoint");

    this->m_buffer.clear();
    return UTF8CodePoint(val);
}

vector<int> UTF8Decoder::strdecode(const string& str)
{
    UTF8Decoder decoder;
    vector<int> result;
    for (auto c : str) {
        auto cp = decoder.decode(c);
        if (cp.presented())
            result.push_back(cp.getval());
    }
    return result;
}
