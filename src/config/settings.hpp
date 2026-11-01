#pragma once

#include <filesystem>
#include <string>

#include "../general/paths.hpp"

struct Settings {
    // Paths
    std::filesystem::path root = paths::root();
    std::filesystem::path cache = paths::cache();

    // General
    bool verbose = false;
    std::string color = "auto";

    // Building
    unsigned int build_jobs = 4;
    bool show_build_output = false;

    // Downloading
    bool show_progress = true;
    std::string progress_style = "trail";
};

Settings default_settings();

bool validate_settings(const Settings &settings);
bool load_settings(Settings &settings);
