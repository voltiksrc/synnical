#pragma once

#include <filesystem>
#include <string>
#include <vector>

// Returns the process exit code, or -1 on execution failure.
int run_command(
    const std::vector<std::string>& args,
    const std::filesystem::path& working_dir = ".",
    const std::vector<std::string>& environment = {}
);
