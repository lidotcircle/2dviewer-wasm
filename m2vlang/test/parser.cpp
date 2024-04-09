#include "parser.h"
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>

TEST(parser, basic) {
    M2V::GObjectParser parser;
    parser.parse("(hello 1 2 3)");
}
