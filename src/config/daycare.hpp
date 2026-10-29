#pragma once

#include <string>
#include <filesystem>
#include <vector>

struct Config {
    std::string hostname;
    std::string timezone;

    std::vector<std::string> packages;
};

Config parse_daycare(const std::filesystem::path& path);
