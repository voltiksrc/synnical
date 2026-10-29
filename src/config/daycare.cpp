#include <toml++/toml.hpp>
#include <string>
#include <filesystem>
#include <optional>
#include <sstream>

#include "daycare.hpp"
#include "../general/output.hpp"

using std::string;

namespace fs = std::filesystem;

bool validate_packages(toml::node_view<toml::node> node) {
    if (!node) {
        output::error("Missing [packages].install area, Please fill it out.");
        return false;
    }

    if (!node.is_array()) {
        output::error("Expected an array at [packages].install!");
        return false;
    }

    if (auto *pkgs = node.as_array()) {
        for (const auto &pkg : *pkgs) {
            auto value = pkg.value<std::string>();

            if (!value || value->empty()) {
                output::error("Invalid package name in [packages].install");
                return false;
            }
        }
    }

    return true;
}

bool validate_string(toml::node_view<toml::node> node, const string& name) {
    if (!node) {
        output::error("Missing [system]." + name);
        return false;
    }  

    if (!node.is_string()) {
        std::ostringstream type;
        type << node.type();

        output::error("[system]." + name + ": expected string, received " + type.str());
        return false;
    }

    auto check = node.value<string>();
    if (check->empty()) {
        output::error("Error: Empty [system]." + name + ", should be filled out.");
        return false;
    }

    return true;
}

std::optional<Config> parse_daycare(const fs::path& path) 
{ 
    // We parse our table so we can get the individual sections inside the config file.
    auto table = toml::parse_file(path.string());

    output::info("Reading Daycare configuration...");

    Config config;

    auto hostnode = table["system"]["hostname"];
    auto timenode = table["system"]["timezone"];
    auto localenode = table["system"]["locale"];
    auto shellnode = table["system"]["shell"];
    auto packagenode = table["packages"]["install"];

    if (!validate_string(hostnode, "hostname")) {
        return std::nullopt;
    }

    if (!validate_string(timenode, "timezone")) {
        return std::nullopt;
    }

    if (!validate_string(localenode, "locale")) {
        return std::nullopt;
    }

    if (!validate_string(shellnode, "shell")) {
        return std::nullopt;
    }

    if (!validate_packages(packagenode)) {
        return std::nullopt;
    }

    // Read the sections off the table/config to get our values which we will
    // eventually apply to the host system.
    config.hostname = table["system"]["hostname"].value_or("");
    config.timezone = table["system"]["timezone"].value_or("");
    config.locale   = table["system"]["locale"].value_or("");
    config.shell    = table["system"]["shell"].value_or("");

    // Retrieve the package list as a TOML array, then iterate through its
    // elements and store each valid package name in our vector.
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
