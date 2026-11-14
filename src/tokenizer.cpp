#include "tokenizer.h"

#include <cctype>
#include <stdexcept>
#include <string>

namespace {

Token makeToken(Token::Type type, const std::string& text, double value = 0.0) {
    Token t;
    t.type = type;
    t.text = text;
    t.value = value;
    return t;
}

Token readNumber(const std::string& s, std::size_t& i) {
    const std::size_t n = s.size();
    const std::size_t start = i;
    bool hasDot = false;
    bool hasExp = false;

    while (i < n) {
        char ch = s[i];
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            ++i;
        } else if (ch == '.' && !hasDot && !hasExp) {
            hasDot = true;
            ++i;
        } else if ((ch == 'e' || ch == 'E') && !hasExp) {
            hasExp = true;
            ++i;
            if (i < n && (s[i] == '+' || s[i] == '-')) ++i;
        } else {
            break;
        }
    }

    const std::string numStr = s.substr(start, i - start);
    if (numStr.empty() || numStr == ".") {
        throw std::runtime_error("Invalid number");
    }

    try {
        double v = std::stod(numStr);
        return makeToken(Token::Number, numStr, v);
    } catch (...) {
        throw std::runtime_error("Invalid number: " + numStr);
    }
}

Token readIdentifier(const std::string& s, std::size_t& i) {
    const std::size_t n = s.size();
    const std::size_t start = i;
    while (i < n && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_')) {
        ++i;
    }
    const std::string ident = s.substr(start, i - start);
    return makeToken(Token::Ident, ident);
}

Token readOperatorOrPunct(char c, std::size_t& i) {
    ++i;
    switch (c) {
        case '+': case '-': case '*': case '/': case '^':
            return makeToken(Token::Op, std::string(1, c));
        case '(':
            return makeToken(Token::LParen, "(");
        case ')':
            return makeToken(Token::RParen, ")");
        case ',':
            return makeToken(Token::Comma, ",");
        default:
            throw std::runtime_error(std::string("Unexpected character: ") + c);
    }
}

} // namespace

std::vector<Token> tokenize(const std::string& s) {
    std::vector<Token> toks;
    const std::size_t n = s.size();
    std::size_t i = 0;

    while (i < n) {
        char c = s[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            toks.push_back(readNumber(s, i));
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            toks.push_back(readIdentifier(s, i));
            continue;
        }
        toks.push_back(readOperatorOrPunct(c, i));
    }

    toks.push_back(makeToken(Token::End, ""));
    return toks;
}
