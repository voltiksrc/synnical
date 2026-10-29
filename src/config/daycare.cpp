#include <toml++/toml.hpp>
#include <string>
#include <filesystem>

#include "daycare.hpp"

using std::string;

namespace fs = std::filesystem;

Config parse_daycare(const fs::path& path) {
     auto table = toml::parse_file(path);

    Config config;

    // Read the sections off the config such as hostname and timezone
    config.hostname = table["system"]["hostname"].value_or("");
    config.timezone = table["system"]["timezone"].value_or("");

    config.packages.clear();

    // Our packages variable is a vector so we cannot read off it as normal,
    // we must read it as an array and push_back the pointer value we get
    if (auto *pkgs = table["packages"]["install"].as_array()) {
        for (const auto &pkg : *pkgs) {
            auto value = pkg.value<string>();

            if (!value || value->empty()) 
                continue;

            config.packages.push_back(*value);
        }
    }

    return config;
}
