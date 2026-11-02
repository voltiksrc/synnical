#pragma once

#include <cstdlib>
#include <filesystem>
#include <stdexcept>

// This file holds the system paths so we dont hardcode the path in every file

namespace fs = std::filesystem;

namespace paths {

inline fs::path home() {
    const char *value = std::getenv("HOME");

    if (!value || !*value)
        throw std::runtime_error("HOME is not set.");

    return fs::path(value);
}

inline fs::path data() {
    const char *xdg = std::getenv("XDG_DATA_HOME");

    if (xdg && *xdg && fs::path(xdg).is_absolute())
        return fs::path(xdg) / "synnical";

    return home() / ".local/share/synnical";
}

inline fs::path config() {
    const char *xdg = std::getenv("XDG_CONFIG_HOME");

    if (xdg && *xdg && fs::path(xdg).is_absolute())
        return fs::path(xdg) / "synnical";

    return home() / ".config/synnical";
}

inline fs::path cache() {
    const char *xdg = std::getenv("XDG_CACHE_HOME");

    if (xdg && *xdg && fs::path(xdg).is_absolute())
        return fs::path(xdg) / "synnical";

    return home() / ".cache/synnical";
}

inline fs::path root() {
    return data() / "test-root";
}

inline fs::path database() {
    return root() / "var/lib/synnical/installed.db";
}

inline fs::path usr() {
    return root() / "usr";
}

inline fs::path settings_file() {
    return config() / "synnical.ini";
}

} // namespace paths
