#include <toml++/toml.hpp>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "daycare.hpp"
#include "../general/output.hpp"

using std::string;
namespace fs = std::filesystem;

namespace {
bool read_string(toml::node_view<toml::node> node, const string& name,
                 string& result, bool required) {
    if (!node) {
        if (required) output::error("Missing " + name);
        return !required;
    }
    if (!node.is_string()) {
        output::error(name + ": expected a string");
        return false;
    }
    result = node.value_or("");
    if (result.empty()) {
        output::error(name + " must not be empty");
        return false;
    }
    return true;
}

bool read_array(toml::node_view<toml::node> node, const string& name,
                std::vector<string>& result, bool required) {
    if (!node) {
        if (required) output::error("Missing " + name);
        return !required;
    }
    auto* values = node.as_array();
    if (!values) {
        output::error(name + " must be an array");
        return false;
    }
    for (const auto& item : *values) {
        const auto value = item.value<string>();
        if (!value || value->empty()) {
            output::error(name + " must contain non-empty strings");
            return false;
        }
        result.push_back(*value);
    }
    return true;
}
} // namespace

std::optional<Config> parse_daycare(const fs::path& path) {
    output::info("Reading Daycare configuration...");
    toml::table table;
    try {
        table = toml::parse_file(path.string());
    } catch (const toml::parse_error& e) {
        output::error("Invalid Daycare TOML: " + string(e.what()));
        return std::nullopt;
    } catch (const std::exception& e) {
        output::error("Cannot read Daycare configuration: " + string(e.what()));
        return std::nullopt;
    }

    Config config;
    if (!read_string(table["system"]["hostname"], "[system].hostname", config.hostname, false) ||
        !read_string(table["system"]["timezone"], "[system].timezone", config.timezone, false) ||
        !read_string(table["system"]["locale"], "[system].locale", config.locale, false) ||
        !read_string(table["system"]["shell"], "[system].shell", config.shell, false) ||
        !read_array(table["packages"]["install"], "[packages].install", config.packages, false) ||
        !read_array(table["repositories"]["enabled"], "[repositories].enabled", config.repositories, false)) {
        return std::nullopt;
    }

    if (auto* users = table["users"].as_table()) {
        for (auto&& [username, value] : *users) {
            auto* settings = value.as_table();
            
            if (!settings) {
                output::error("[users." + string(username.str()) + "] must be a table");
                return std::nullopt;
            }
            string shell;
            if (!read_string((*settings)["shell"], "[users." + string(username.str()) + "].shell", shell, false))
                return std::nullopt;
            if (!shell.empty()) config.user_shells.emplace(string(username.str()), shell);
        }
    } else if (table["users"]) {
        output::error("[users] must be a table");
        return std::nullopt;
    }

    return config;
}
