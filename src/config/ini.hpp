#pragma once

#include <filesystem>
#include <map>
#include <string>

using IniSection = std::map<std::string, std::string>;
using IniData = std::map<std::string, IniSection>;

IniData parse_ini(const std::filesystem::path &file);
