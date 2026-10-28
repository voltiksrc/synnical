#pragma once

#include <string>
#include <filesystem>
#include <vector>

bool download_recipe(
    const std::string& url,
    const std::filesystem::path& destination
);


struct Package {
    std::string name;
    std::string version;
    std::string url;
    std::string sha256;
    std::string build_system;

    std::string architecture;
    std::vector<std::string> dependencies;
};

Package read_recipe(const std::string& path);
