#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <optional>

struct Config {
    std::string hostname;
    std::string timezone;
    std::string locale;
    std::string shell;

    std::vector<std::string> packages;
};

std::optional<Config> parse_daycare(const std::filesystem::path& path);
