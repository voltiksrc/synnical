#pragma once
#include "recipe.hpp"
#include <filesystem>

bool build_source(const Package &info, const std::filesystem::path &source_dir);
