#pragma once

#include <filesystem>
#include <string>

struct BuildState {
    std::string source_hash;
    bool extracted = false;
    bool configured = false;
    bool built = false;
};

BuildState load_state(const std::filesystem::path &path);
bool save_state(const std::filesystem::path &path, const BuildState &state);
