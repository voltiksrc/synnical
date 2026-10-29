#include "daycare.hpp"
#include "../general/output.hpp"

#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

bool apply_timezone(const std::string& timezone, const fs::path& target_root) {
    if (timezone.empty()) return true; // Not specified: do not change it.

    // Reject absolute paths, parent traversal, and odd path components.
    const fs::path name(timezone);
    if (name.is_absolute()) {
        output::error("Invalid timezone: absolute paths are not allowed.");
        return false;
    }
    for (const auto& component : name) {
        if (component == "." || component == ".." || component.empty()) {
            output::error("Invalid timezone: " + timezone);
            return false;
        }
    }

    std::error_code ec;
    const fs::path root = fs::canonical(target_root, ec);
    if (ec) {
        output::error("Cannot access target root: " + ec.message());
        return false;
    }
    const fs::path zone_root = root / "usr/share/zoneinfo";
    const fs::path zone = fs::canonical(zone_root / name, ec);
    if (ec || !fs::is_regular_file(zone)) {
        output::error("Timezone not found inside target root: " + timezone);
        return false;
    }

    // Ensure symlinks in zoneinfo cannot escape the target root.
    const fs::path resolved_zone_root = fs::canonical(zone_root, ec);
    if (ec) {
        output::error("Cannot access timezone database in target root.");
        return false;
    }
    const fs::path zone_root_relative = resolved_zone_root.lexically_relative(root);
    if (zone_root_relative.empty() || zone_root_relative.is_absolute() ||
        *zone_root_relative.begin() == "..") {
        output::error("Timezone database resolves outside the target root.");
        return false;
    }
    const fs::path relative_zone = zone.lexically_relative(resolved_zone_root);
    if (relative_zone.empty() || relative_zone.is_absolute() ||
        *relative_zone.begin() == "..") {
        output::error("Timezone resolves outside the target timezone database.");
        return false;
    }

    const fs::path etc = root / "etc";
    if (fs::is_symlink(etc, ec)) {
        output::error("Refusing to write through a symlinked /etc directory.");
        return false;
    }
    fs::create_directories(etc, ec);
    if (ec) {
        output::error("Cannot create target /etc: " + ec.message());
        return false;
    }

    const fs::path localtime = etc / "localtime";
    const fs::path link_target = fs::path("../usr/share/zoneinfo") / relative_zone;

    if (fs::is_symlink(localtime, ec) && fs::read_symlink(localtime, ec) == link_target) {
        output::info("Timezone already set to " + timezone);
        return true;
    }
    if (fs::is_directory(localtime, ec)) {
        output::error("Refusing to replace a directory at /etc/localtime.");
        return false;
    }

    // Create a new symlink and rename into place, rather than deleting localtime first.
    const fs::path temp = etc / "localtime.synnical-new";
    if (fs::exists(temp, ec) || fs::is_symlink(temp, ec)) {
        output::error("Temporary timezone path already exists: " + temp.string());
        return false;
    }
    fs::create_symlink(link_target, temp, ec);
    if (ec) {
        output::error("Cannot create timezone symlink: " + ec.message());
        return false;
    }
    fs::rename(temp, localtime, ec);
    if (ec) {
        std::error_code cleanup;
        fs::remove(temp, cleanup);
        output::error("Cannot update /etc/localtime: " + ec.message());
        return false;
    }

    output::info("Timezone set to " + timezone);
    return true;
}
