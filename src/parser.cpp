#include "parser.h"

#include <cmath>
#include <numbers>
#include <stdexcept>

Parser::Parser(const std::vector<Token>& toks, const PluginManager& pm)
    : toks_(toks), pm_(pm) {}

double Parser::parse() {
    double v = parseExpression();
    if (cur().type != Token::End) {
        throw std::runtime_error("Unexpected token: " + cur().text);
    }
    return v;
}

const Token& Parser::cur() const {
    return toks_[pos_ < toks_.size() ? pos_ : toks_.size() - 1];
}

void Parser::advance() {
    if (pos_ + 1 < toks_.size()) ++pos_;
}

double Parser::parseExpression() {
    double v = parseTerm();
    while (cur().type == Token::Op && (cur().text == "+" || cur().text == "-")) {
        std::string op = cur().text;
        advance();
        double rhs = parseTerm();
        if (op == "+") v += rhs; else v -= rhs;
    }
    return v;
}

double Parser::parseTerm() {
    double v = parsePower();
    while (cur().type == Token::Op && (cur().text == "*" || cur().text == "/")) {
        std::string op = cur().text;
        advance();
        double rhs = parsePower();
        if (op == "*") {
            v *= rhs;
        } else {
            if (rhs == 0.0) throw std::runtime_error("Division by zero");
            v /= rhs;
        }
    }
    return v;
}

double Parser::parsePower() {
    double base = parseUnary();
    if (cur().type == Token::Op && cur().text == "^") {
        advance();
        double exp = parsePower();
        return std::pow(base, exp);
    }
    return base;
}

double Parser::parseUnary() {
    if (cur().type == Token::Op && (cur().text == "+" || cur().text == "-")) {
        std::string op = cur().text;
        advance();
        double v = parseUnary();
        return (op == "-") ? -v : v;
    }
    return parsePrimary();
}

double Parser::parsePrimary() {
    if (cur().type == Token::Number) {
        double v = cur().value;
        advance();
        return v;
    }
    if (cur().type == Token::LParen) {
        advance();
        double v = parseExpression();
        if (cur().type != Token::RParen) throw std::runtime_error("Expected ')'");
        advance();
        return v;
    }
    if (cur().type == Token::Ident) {
        std::string name = cur().text;
        advance();
        if (cur().type != Token::LParen) {
            if (name == "pi" || name == "PI") return std::numbers::pi_v<double>;
            if (name == "e" || name == "E") return std::numbers::e_v<double>;
            return pm_.call(name, {});
        }
        advance();
        std::vector<double> args;
        if (cur().type != Token::RParen) {
            args.push_back(parseExpression());
            while (cur().type == Token::Comma) {
                advance();
                args.push_back(parseExpression());
            }
        }
        if (cur().type != Token::RParen) throw std::runtime_error("Expected ')'");
        advance();
        return pm_.call(name, args);
    }
    throw std::runtime_error("Expected number, identifier, or '('");
}
