#pragma once

#include <string>
#include <vector>

struct Token {
    enum Type { Number, Ident, Op, LParen, RParen, Comma, End };
    Type type = End;
    std::string text;
    double value = 0.0;
};

std::vector<Token> tokenize(const std::string& s);
