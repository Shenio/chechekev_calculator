#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

struct Plugin {
    std::string name;
    int arity = 0;
    double (*execute)(const double*, int) = nullptr;
    HMODULE module = nullptr;

    Plugin() = default;
    ~Plugin();
    Plugin(const Plugin&) = delete;
    Plugin& operator=(const Plugin&) = delete;
    Plugin(Plugin&& other) noexcept;
};

class PluginManager {
public:
    void loadFromDirectory(const std::string& dir);
    bool has(const std::string& name) const;
    double call(const std::string& name, const std::vector<double>& args) const;
    void list(std::ostream& os) const;
    const std::vector<std::unique_ptr<Plugin>>& plugins() const;

private:
    void loadOne(const std::filesystem::path& path);

    std::vector<std::unique_ptr<Plugin>> plugins_;
    std::unordered_map<std::string, Plugin*> pluginsByName_;
};
