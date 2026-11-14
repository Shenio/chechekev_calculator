#pragma once

#include "plugin.h"
#include "tokenizer.h"

#include <vector>

class Parser {
public:
    Parser(const std::vector<Token>& toks, const PluginManager& pm);
    double parse();

private:
    const std::vector<Token>& toks_;
    const PluginManager& pm_;
    std::size_t pos_ = 0;

    const Token& cur() const;
    void advance();

    double parseExpression();
    double parseTerm();
    double parsePower();
    double parseUnary();
    double parsePrimary();
};
