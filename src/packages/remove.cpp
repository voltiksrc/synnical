
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "database.hpp"
#include "output.hpp"
#include "remove.hpp"

namespace fs = std::filesystem;

bool remove_package(const std::string &name) {
    const fs::path root = "/tmp/omi-test-root";
    const fs::path database_path = root / "var/lib/omi/installed.db";

    if (!fs::is_regular_file(database_path)) {
        output::error("Installed package database not found");
        return false;
    }

    Database db(database_path);

    if (!db.initialize()) {
        return false;
    }

    std::vector<fs::path> files;
    InstallStatus status = db.get_installed_files(name, files);

    if (status == InstallStatus::Error) {
        return false;
    }

    if (status == InstallStatus::NotInstalled) {
        output::error("Package is not installed: " + name);
        return false;
    }

    output::stage("Removing " + name + "...");

    std::vector<fs::path> targets;

    // Validate every recorded path before deleting anything
    for (const auto &file : files) {
        fs::path relative = file.relative_path();

        if (!file.is_absolute() || relative.empty() ||
            relative.lexically_normal() != relative) {
            output::error("Invalid package path: " + file.string());
            return false;
        }

        for (const auto &part : relative) {
            if (part == "." || part == "..") {
                output::error("Unsafe package path: " + file.string());
                return false;
            }
        }

        // Never let a package remove Omi's own database
        if (relative == fs::path("var/lib/omi") ||
            relative.string().starts_with("var/lib/omi/")) {
            output::error("Refusing to remove Omi database files");
            return false;
        }

        fs::path destination = root / relative;

        // Don't follow symlinks in any parent directory
        fs::path parent = root;

        for (const auto &part : relative.parent_path()) {
            parent /= part;

            if (fs::is_symlink(fs::symlink_status(parent))) {
                output::error("Symlinked destination directory: " + parent.string());
                return false;
            }

            if (!fs::is_directory(parent)) {
                output::error("Missing destination directory: " + parent.string());
                return false;
            }
        }

        std::error_code ec;
        fs::file_status state = fs::symlink_status(destination, ec);

        if (ec || (!fs::is_regular_file(state) && !fs::is_symlink(state))) {
            output::error("Installed file is missing or invalid: " + destination.string());
            return false;
        }

        targets.push_back(destination);
    }

    // Remove only the files recorded in SQLite
    for (const auto &target : targets) {
        std::error_code ec;

        if (!fs::remove(target, ec) || ec) {
            output::error("Failed to remove file: " + target.string());
            return false;
        }

        output::info("Removed " + target.string());
    }

    if (!db.remove_installation(name)) {
        output::error("Failed to update installed package database");
        return false;
    }

    output::info("Package removed successfully");
    return true;
}

