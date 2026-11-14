#include "plugin.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

Plugin::~Plugin() {
    if (module) {
        FreeLibrary(module);
        module = nullptr;
    }
}

Plugin::Plugin(Plugin&& other) noexcept
    : name(std::move(other.name)),
      arity(other.arity),
      execute(other.execute),
      module(other.module) {
    other.module = nullptr;
    other.execute = nullptr;
}

void PluginManager::loadFromDirectory(const std::string& dir) {
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

bool PluginManager::has(const std::string& name) const {
    return pluginsByName_.count(name) != 0;
}

double PluginManager::call(const std::string& name, const std::vector<double>& args) const {
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

void PluginManager::list(std::ostream& os) const {
    if (plugins_.empty()) {
        os << "  (none)\n";
        return;
    }
    for (const auto& p : plugins_) {
        os << "  " << p->name << " (arity=" << p->arity << ")\n";
    }
}

const std::vector<std::unique_ptr<Plugin>>& PluginManager::plugins() const {
    return plugins_;
}

void PluginManager::loadOne(const fs::path& path) {
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
