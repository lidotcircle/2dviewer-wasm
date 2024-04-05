#ifndef _LEXER_POSITION_INFO_H_
#define _LEXER_POSITION_INFO_H_

#include <string>
#include <vector>
#include <optional>


class TextRangeEntity
{
public:
    using TextRange = std::pair<size_t,size_t>;

private:
    std::optional<TextRange> m_range;

protected:
    void contain_(const TextRangeEntity& other);

public:
    TextRangeEntity();
    TextRangeEntity(TextRange range);
    std::optional<TextRange> range() const;

    std::optional<size_t> beg() const;
    std::optional<size_t> end() const;
    std::optional<size_t> length() const;
    void benowhere();

    inline void contain(const TextRangeEntity& other) { this->contain_(other); }
};


class TextInfo {
public:
    struct PInfo {
        size_t line;
        size_t column;
    };
    struct LineRange {
        std::string line;
        size_t beg, end;
    };

    virtual const std::string& filename() const = 0;
    virtual size_t len() const = 0;
    virtual PInfo query(size_t pos) const = 0;
    virtual std::pair<size_t,size_t> line_range(size_t line) const = 0;
    virtual std::string query_string(size_t from, size_t to) const = 0;
    inline std::string query_string(const TextRangeEntity& range) const {
        if (!range.range().has_value())
            return "";

        return this->query_string(range.beg().value(), range.end().value());
    }
    inline std::string query_line(size_t line) const {
        auto range = line_range(line);
        return query_string(range.first, range.second);
    }

    std::string row_col_str(size_t pos) const;
    std::string row_col_str(size_t pos, size_t end_pos) const;
    std::string row_col_str(const TextRangeEntity& range) const;

    std::vector<LineRange> lines(size_t beg_pos, size_t end_pos) const;

    virtual ~TextInfo() = default;
};

#endif // _LEXER_POSITION_INFO_H_
