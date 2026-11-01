#pragma once

#include "../config/settings.hpp"
#include <string>
#include <vector>

struct PlannedPackage {
    std::string name;
    std::string version;
};

bool solve_install_plan(
    const std::vector<std::string> &requested,
    std::vector<PlannedPackage> &plan,
    const Settings &settings
);

bool resolve_package(const std::string &name, const Settings &settings);
