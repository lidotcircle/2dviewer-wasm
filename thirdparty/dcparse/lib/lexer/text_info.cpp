#include "lexer/text_info.h"
#include "assert.h"
using namespace std;
using PInfo = TextInfo::PInfo;
using LineRange = TextInfo::LineRange;
using TextRange = TextRangeEntity::TextRange;

#define MAX_AB(a, b) ((a) > (b) ? (a) : (b))
#define MIN_AB(a, b) ((a) < (b) ? (a) : (b))


TextRangeEntity::TextRangeEntity() {}

TextRangeEntity::TextRangeEntity(TextRange range): m_range(range) { assert(range.second >= range.first); }

void TextRangeEntity::contain_(const TextRangeEntity& other)
{
    if (!other.m_range.has_value())
        return;

    if (!this->m_range.has_value()) {
        this->m_range = other.m_range.value();
        return;
    }

    const auto p1 = this->m_range.value().first;
    const auto p2 = this->m_range.value().second;
    const auto p3 = other.m_range.value().first;
    const auto p4 = other.m_range.value().second;

    this->m_range = TextRange(
        MIN_AB(p1, p3),
        MAX_AB(p2, p4)
    );
}

optional<TextRange> TextRangeEntity::range() const { return this->m_range; }

optional<size_t> TextRangeEntity::beg() const {
    if (this->m_range.has_value())
        return this->m_range.value().first;
    else
        return nullopt;
}

optional<size_t> TextRangeEntity::end() const {
    if (this->m_range.has_value())
        return this->m_range.value().second;
    else
        return nullopt;
}

optional<size_t> TextRangeEntity::length() const {
    if (!this->m_range.has_value())
        return nullopt;

    return m_range.value().second - m_range.value().first;
}

void TextRangeEntity::benowhere() {
    this->m_range = nullopt;
}

string TextInfo::row_col_str(size_t pos) const
{
    auto px = this->query(pos);
    auto str = "line " + to_string(px.line) + ":" + to_string(px.column);
    if (this->filename().size() > 0)
        str += this->filename() + "[" + str + "]";
    return str;
}

string TextInfo::row_col_str(size_t p1, size_t p2) const
{
    auto px = this->query(p1);
    auto py = this->query(p2);
    auto s = "line " + to_string(px.line) + ":" + to_string(px.column) + "-" +
                       to_string(py.line) + ":" + to_string(py.column);
    if (this->filename().size() > 0)
        s += this->filename() + "[" + s + "]";
    return s;
}

string TextInfo::row_col_str(const TextRangeEntity& range) const
{
    if (!range.range().has_value())
        return "";

    return this->row_col_str(range.range().value().first, range.range().value().second);
}

vector<LineRange> TextInfo::lines(size_t beg_pos, size_t end_pos) const
{
    assert(beg_pos <= end_pos);
    if (beg_pos == end_pos)
        return {};
    end_pos--;

    auto binfo = this->query(beg_pos);
    auto einfo = this->query(end_pos);
    vector<LineRange> ret;

    for (auto bl=binfo.line;bl<=einfo.line;bl++) {
        auto l = this->query_line(bl);
        size_t b = 0, e = l.size();
        if (bl == binfo.line)
            b = binfo.column - 1;
        if (bl == einfo.line)
            e = einfo.column;

        ret.push_back(LineRange{ .line = l, .beg = b, .end = e });
    }

    return ret;
}
