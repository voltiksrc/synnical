#include <algorithm>
#include <filesystem>
#include <string>
#include <toml++/toml.hpp>
#include <vector>
#include <cctype>

#include "output.hpp"
#include "search.hpp"

namespace fs = std::filesystem;

bool search_packages(const std::string &query) {
    fs::path index_path = ".cache/repos/main/index.toml";

    if (!fs::is_regular_file(index_path)) {
        output::error("Repository index missing, Run synnical --sync first.");
        return false;
    }

    if (query.empty()) {
        output::error("Please enter a package to search for.");
        return false;
    }

    // Read the synced repository index
    toml::table index;

    try {
        index = toml::parse_file(index_path.string());
    } catch (const toml::parse_error &error) {
        output::error("Failed to parse repository index: " + std::string(error.what()));
        return false;
    }

    auto packages = index["packages"].as_table();

    if (!packages) {
        output::error("Repository index contains no package table.");
        return false;
    }

    std::string search = query;

    std::transform(search.begin(), search.end(), search.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::vector<std::string> matches;

    // Compare the query against each package name
    for (const auto &[name, value] : *packages) {
        std::string package = std::string(name.str());
        std::string lowercase = package;

        std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lowercase.find(search) != std::string::npos) {
            matches.push_back(package);
        }
    }

    std::sort(matches.begin(), matches.end());

    output::stage("Searching repositories...");

    if (matches.empty()) {
        output::info("No packages found for: " + query);
        return true;
    }

    for (const auto &package : matches) {
        output::info(package);
    }

    output::info("Found " + std::to_string(matches.size()) + " package(s)");
    return true;
}

