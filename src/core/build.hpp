#pragma once
#include "../config/settings.hpp"
#include "../repository/recipe.hpp"
#include <filesystem>

bool build_source(const Package &info, const std::filesystem::path &source_dir,
                  const Settings &settings);
