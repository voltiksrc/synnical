#pragma once

#include <filesystem>
#include <string>
#include <vector>

enum class ProcessOutput { Full, Quiet, Filtered };

// Returns the child's exit status, or -1 on a process error.
int run_command(const std::vector<std::string>& args,
                const std::filesystem::path& working_dir = ".",
                const std::vector<std::string>& environment = {},
                ProcessOutput mode = ProcessOutput::Full);
