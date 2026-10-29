#pragma once

#include <string>
#include <vector>

bool install_requested(const std::vector<std::string> &requested);
bool install_package(const std::string& package, const std::string &expected_version);
