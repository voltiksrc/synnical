#pragma once

#include "../config/settings.hpp"
#include <string>
#include <vector>

bool install_requested(const std::vector<std::string> &requested, const Settings &settings);
bool install_package(const std::string& package, const std::string &expected_version, const Settings &settings);
