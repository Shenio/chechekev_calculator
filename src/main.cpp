#include "plugin_api.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

struct Plugin {
    std::string name;
    int arity = 0;
    double (*execute)(const double*, int) = nullptr;
    HMODULE module = nullptr;

    Plugin() = default;
    ~Plugin() {
        if (module) {
            FreeLibrary(module);
            module = nullptr;
        }
    }
    Plugin(const Plugin&) = delete;
    Plugin& operator=(const Plugin&) = delete;
    Plugin(Plugin&& other) noexcept
        : name(std::move(other.name)),
          arity(other.arity),
          execute(other.execute),
          module(other.module) {
        other.module = nullptr;
        other.execute = nullptr;
    }
};

class PluginManager {
public:
    void loadFromDirectory(const std::string& dir) {
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
            std::cout << "[info] Plugin directory '" << dir << "' does not exist or is not a directory.\n";
            return;
        }

        std::vector<fs::path> dlls;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            if (entry.is_regular_file() && entry.path().extension() == ".dll") {
                dlls.push_back(entry.path());
            }
        }
        std::sort(dlls.begin(), dlls.end());

        if (dlls.empty()) {
            std::cout << "[info] No plugins found in '" << dir << "'.\n";
            return;
        }

        for (const auto& dllPath : dlls) {
            loadOne(dllPath);
        }

        std::cout << "[info] Loaded " << plugins_.size() << " plugin(s).\n";
    }

    bool has(const std::string& name) const {
        return pluginsByName_.count(name) != 0;
    }

    double call(const std::string& name, const std::vector<double>& args) const {
        auto it = pluginsByName_.find(name);
        if (it == pluginsByName_.end()) {
            throw std::runtime_error("Unknown function: " + name);
        }
        Plugin* p = it->second;
        if ((int)args.size() != p->arity) {
            throw std::runtime_error(name + " expects " + std::to_string(p->arity) +
                                    " argument(s), got " + std::to_string(args.size()));
        }
        try {
            if (!p->execute) throw std::runtime_error("Plugin execute pointer is null");
            return p->execute(args.empty() ? nullptr : args.data(), (int)args.size());
        } catch (const std::exception& e) {
            throw std::runtime_error("Plugin '" + name + "' threw: " + e.what());
        } catch (...) {
            throw std::runtime_error("Plugin '" + name + "' threw an unknown exception");
        }
    }

    void list(std::ostream& os) const {
        if (plugins_.empty()) {
            os << "  (none)\n";
            return;
        }
        for (const auto& p : plugins_) {
            os << "  " << p->name << " (arity=" << p->arity << ")\n";
        }
    }

    const std::vector<std::unique_ptr<Plugin>>& plugins() const { return plugins_; }

private:
    void loadOne(const fs::path& path) {

        HMODULE mod = LoadLibraryA(path.string().c_str());
        if (!mod) {
            std::cout << "[warn] Failed to load '" << path.filename().string()
                      << "' (LoadLibrary error " << GetLastError() << ").\n";
            return;
        }

        auto getName = reinterpret_cast<const char* (*)()>(
            GetProcAddress(mod, "GetFunctionName"));
        auto getArity = reinterpret_cast<int (*)()>(
            GetProcAddress(mod, "GetArity"));
        auto execute = reinterpret_cast<double (*)(const double*, int)>(
            GetProcAddress(mod, "Execute"));

        if (!getName || !execute) {
            std::cout << "[warn] '" << path.filename().string()
                      << "' is not a valid plugin (missing exports).\n";
            FreeLibrary(mod);
            return;
        }

        std::string name;
        int arity = 1;
        try {
            const char* raw = getName();
            if (!raw || raw[0] == '\0') {
                throw std::runtime_error("GetFunctionName returned empty name");
            }
            name = raw;
            if (getArity) {
                arity = getArity();
            }
        } catch (const std::exception& e) {
            std::cout << "[warn] '" << path.filename().string()
                      << "' threw while querying metadata: " << e.what() << ".\n";
            FreeLibrary(mod);
            return;
        } catch (...) {
            std::cout << "[warn] '" << path.filename().string()
                      << "' threw unknown exception while querying metadata.\n";
            FreeLibrary(mod);
            return;
        }

        if (arity < 1) {
            std::cout << "[warn] '" << path.filename().string()
                      << "' reported invalid arity " << arity << ".\n";
            FreeLibrary(mod);
            return;
        }

        if (pluginsByName_.count(name)) {
            std::cout << "[warn] Duplicate function name '" << name
                      << "' from '" << path.filename().string() << "', skipping.\n";
            FreeLibrary(mod);
            return;
        }

        auto plugin = std::make_unique<Plugin>();
        plugin->name = name;
        plugin->arity = arity;
        plugin->execute = execute;
        plugin->module = mod;

        Plugin* rawPtr = plugin.get();
        pluginsByName_[name] = rawPtr;
        plugins_.push_back(std::move(plugin));

        std::cout << "[info] Loaded plugin '" << name << "' from '"
                  << path.filename().string() << "'.\n";
    }

    std::vector<std::unique_ptr<Plugin>> plugins_;
    std::unordered_map<std::string, Plugin*> pluginsByName_;
};

struct Token {
    enum Type { Number, Ident, Op, LParen, RParen, Comma, End };
    Type type = End;
    std::string text;
    double value = 0.0;
};

static std::vector<Token> tokenize(const std::string& s) {
    std::vector<Token> toks;
    size_t i = 0;
    const size_t n = s.size();
    while (i < n) {
        char c = s[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            size_t start = i;
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
            std::string numStr = s.substr(start, i - start);
            if (numStr == "." || numStr.empty()) {
                throw std::runtime_error("Invalid number");
            }
            try {
                double v = std::stod(numStr);
                Token t; t.type = Token::Number; t.text = numStr; t.value = v;
                toks.push_back(t);
            } catch (...) {
                throw std::runtime_error("Invalid number: " + numStr);
            }
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = i;
            while (i < n && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_')) ++i;
            std::string ident = s.substr(start, i - start);
            Token t; t.type = Token::Ident; t.text = ident;
            toks.push_back(t);
            continue;
        }
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^') {
            Token t; t.type = Token::Op; t.text = std::string(1, c);
            toks.push_back(t);
            ++i;
            continue;
        }
        if (c == '(') { Token t; t.type = Token::LParen; t.text = "("; toks.push_back(t); ++i; continue; }
        if (c == ')') { Token t; t.type = Token::RParen; t.text = ")"; toks.push_back(t); ++i; continue; }
        if (c == ',') { Token t; t.type = Token::Comma; t.text = ","; toks.push_back(t); ++i; continue; }
        throw std::runtime_error(std::string("Unexpected character: ") + c);
    }
    Token t; t.type = Token::End; t.text = "";
    toks.push_back(t);
    return toks;
}

class Parser {
public:
    Parser(const std::vector<Token>& toks, const PluginManager& pm)
        : toks_(toks), pm_(pm) {}

    double parse() {
        double v = parseExpression();
        if (cur().type != Token::End) {
            throw std::runtime_error("Unexpected token: " + cur().text);
        }
        return v;
    }

private:
    const std::vector<Token>& toks_;
    const PluginManager& pm_;
    size_t pos_ = 0;

    const Token& cur() const { return toks_[pos_ < toks_.size() ? pos_ : toks_.size() - 1]; }
    void advance() { if (pos_ + 1 < toks_.size()) ++pos_; }

    double parseExpression() {
        double v = parseTerm();
        while (cur().type == Token::Op && (cur().text == "+" || cur().text == "-")) {
            std::string op = cur().text;
            advance();
            double rhs = parseTerm();
            if (op == "+") v += rhs; else v -= rhs;
        }
        return v;
    }

    double parseTerm() {
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

    double parsePower() {
        double base = parseUnary();
        if (cur().type == Token::Op && cur().text == "^") {
            advance();
            double exp = parsePower();
            return std::pow(base, exp);
        }
        return base;
    }

    double parseUnary() {
        if (cur().type == Token::Op && (cur().text == "+" || cur().text == "-")) {
            std::string op = cur().text;
            advance();
            double v = parseUnary();
            return (op == "-") ? -v : v;
        }
        return parsePrimary();
    }

    double parsePrimary() {
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
                if (name == "pi" || name == "PI") return std::acos(-1.0);
                if (name == "e" || name == "E") return std::exp(1.0);
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
};


int main() {
    std::cout << "Calculator (C++)\n";
    std::cout << "Operations: + - * / ^ ( )\n";
    std::cout << "Constants: pi, e\n";
    std::cout << "Type 'help', 'list', or 'exit'.\n\n";

    PluginManager pm;
    std::string pluginDir = "./plugins";
    {
        std::error_code ec;
        if (!(fs::exists(pluginDir, ec) && fs::is_directory(pluginDir, ec))) {
            char buf[MAX_PATH];
            DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
            if (len > 0 && len < MAX_PATH) {
                fs::path exeDir = fs::path(buf).parent_path();
                fs::path candidate = exeDir / "plugins";
                if (fs::exists(candidate, ec) && fs::is_directory(candidate, ec)) {
                    pluginDir = candidate.string();
                }
            }
        }
    }
    pm.loadFromDirectory(pluginDir);

    std::cout << "Available functions:\n";
    pm.list(std::cout);
    std::cout << "\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        size_t a = line.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) continue;
        size_t b = line.find_last_not_of(" \t\r\n");
        std::string cmd = line.substr(a, b - a + 1);

        if (cmd == "exit" || cmd == "quit") break;
        if (cmd == "help") {
            std::cout << "Enter an arithmetic expression, e.g. 16 + 4 * (3 - 1)\n";
            std::cout << "Functions from plugins are called as name(args), e.g. sin(pi/2)\n";
            std::cout << "Commands: help, list, exit\n";
            continue;
        }
        if (cmd == "list") {
            std::cout << "Loaded functions:\n";
            pm.list(std::cout);
            continue;
        }

        try {
            auto toks = tokenize(cmd);
            Parser parser(toks, pm);
            double result = parser.parse();
            if (result == std::floor(result) && std::fabs(result) < 1e15) {
                std::cout << (long long)result << "\n";
            } else {
                std::cout << std::setprecision(15) << result << "\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n";
        } catch (...) {
            std::cout << "Error: unknown exception\n";
        }
    }

    std::cout << "Bye.\n";
    return 0;
}
