
#pragma once

#include <filesystem>
#include <string>

bool extract_source(const std::string &archive_path, const std::string &name,
                    const std::string &version,
                    const std::filesystem::path &destination);
