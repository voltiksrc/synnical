#pragma once

#include <filesystem>
#include "../config/settings.hpp"
#include "recipe.hpp"

bool stage_source(
    const Package &info,
    const std::filesystem::path &source_dir,
    const std::filesystem::path &stage_dir,
    const Settings &settings
);
