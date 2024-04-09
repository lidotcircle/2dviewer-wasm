#include "parser.h"
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>

static bool stringcmp_ignorespace(const std::string& s1, const std::string& s2)
{
    size_t i1 = 0, i2 = 0;
    while (i1 < s1.size() && i2 < s2.size())
    {
        if (s1.at(i1) == ' ' || s1.at(i1) == '\t' || s1.at(i1) == '\r' || s1.at(i1) == '\n') {
            i1++;
            continue;
        }
        if (s2.at(i2) == ' ' || s2.at(i2) == '\t' || s2.at(i2) == '\r' || s2.at(i2) == '\n') {
            i2++;
            continue;
        }
        if (s1.at(i1) != s2.at(i2)) return false;
        i1++, i2++;
    }
    return i1 == s1.size() && i2 == s2.size();
}

TEST(parser, basic) {
    M2V::GObjectParser parser;
    std::vector<std::string> expressions = {
        "(hello 1 2 3)",
        "(let a 100)",
        "(let a b)",
        "(a a b)",
        "(def a () (b))",
        "(def a (a b c) (f 1 2 3))",
        "(+ 1 2)",
        "(- 1 2)",
        "(* 1 2)",
        "(/ 1 2)",
        "(% 1 2)",
        "(&& 1 2)",
        "(|| 1 2)",
        "(> 1 2)",
        "(< 1 2)",
        "(>= 1 2)",
        "(<= 1 2)",
        "(== 1 2)",
        "(!= 1 2)",
        "(|| (!= 1 2) (== 1 2))",
        "(def a (a b c) (f 1 2 3 -5))",
        "(def a (a b c) (f 1 2 3)) -(a 100)",
    };
    for (auto& e: expressions) {
        auto obj = parser.parse(e);
        ASSERT_TRUE(bool(obj));
        const auto o = obj->format();
        ASSERT_TRUE(stringcmp_ignorespace(o, e));
        parser.reset();
    }
}
