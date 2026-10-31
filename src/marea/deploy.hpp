#pragma once

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

struct Deployment {
    std::vector<fs::path> files;
    std::vector<fs::path> directories;
};

bool install_staged_files(
    const fs::path &stage_dir,
    const fs::path &target_root,
    Deployment &deployment
);

bool rollback_deployment(
    const fs::path &target_root,
    const Deployment &deployment
);
