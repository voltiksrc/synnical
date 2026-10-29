#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct Config {
    std::string hostname;
    std::string timezone;
    std::string locale; // Optional; not applied yet.
    std::string shell;  // Legacy [system].shell; not applied yet.
    std::vector<std::string> packages;
    std::map<std::string, std::string> user_shells; // [users.<name>].shell, parsed only.
    std::vector<std::string> repositories; // [repositories].enabled, parsed only.
};

std::optional<Config> parse_daycare(const std::filesystem::path& path);

// Applies only timezone for now, inside the specified target root.
bool apply_timezone(const std::string& timezone,
                    const std::filesystem::path& target_root);
