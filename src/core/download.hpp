#pragma once

#include "../config/settings.hpp"
#include <string>

std::string download_source(const std::string &source_url, const std::string &name, const std::string &version, const Settings &settings);
