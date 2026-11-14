#include "parser.h"
#include "plugin.h"
#include "tokenizer.h"

#include <windows.h>

#include <cmath>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main() {
    try {
        std::cout << "Calculator (C++)\n";
        std::cout << "Operations: + - * / ^ ( )\n";
        std::cout << "Constants: pi, e\n";
        std::cout << "Type 'help', 'list', or 'exit'.\n\n";

        PluginManager pm;
        std::string pluginDir;
        {
            std::error_code ec;
            char buf[MAX_PATH];
            DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
            if (len > 0 && len < MAX_PATH) {
                fs::path exeDir = fs::path(buf).parent_path();
                fs::path candidate = exeDir / "plugins";
                if (fs::exists(candidate, ec) && fs::is_directory(candidate, ec)) {
                    pluginDir = candidate.string();
                }
            }
            if (pluginDir.empty() && fs::exists("./plugins", ec) && fs::is_directory("./plugins", ec)) {
                pluginDir = "./plugins";
            }
        }
        if (!pluginDir.empty()) {
            pm.loadFromDirectory(pluginDir);
        }


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
                    std::cout << static_cast<long long>(result) << "\n";
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
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: unknown exception\n";
        return 1;
    }
}
