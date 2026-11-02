#include <INIReader.h>
#include <filesystem>
#include <limits>
#include <string>

#include "../common/output.hpp"
#include "settings.hpp"

Settings default_settings() {
    return Settings{};
}

// Resolve ~ before storing paths. std::filesystem doesn't expand it for us.
static std::filesystem::path expand_home(const std::string &text) {
    if (text == "~")
        return paths::home();
    if (text.starts_with("~/"))
        return paths::home() / text.substr(2);
    return text;
}

bool validate_settings(const Settings &settings) {
    if (settings.build_jobs == 0) {
        output::error("Build jobs must be greater than zero.");
        return false;
    }

    // Keep installations inside Synnical's own data directory during development.
    const auto safe_base = paths::data().lexically_normal();
    const auto root = settings.root.lexically_normal();
    const auto relative = root.lexically_relative(safe_base);
    if (!root.is_absolute() || relative.empty() || relative == "." ||
        *relative.begin() == "..") {
        output::error("Installation root must be inside " + safe_base.string());
        return false;
    }
    // Reject symlinked paths before creating database or package files.
    std::filesystem::path walked;
    for (const auto &part : root) {
        walked /= part;
        std::error_code ec;
        if (std::filesystem::is_symlink(std::filesystem::symlink_status(walked, ec))) {
            output::error("Installation root cannot use symlinked directories.");
            return false;
        }
    }

    if (!settings.cache.is_absolute()) {
        output::error("Cache directory must be an absolute path.");
        return false;
    }
    if (settings.progress_style != "trail") {
        output::error("Unsupported progress style: " + settings.progress_style);
        return false;
    }

    if (settings.color != "auto" && settings.color != "always" && settings.color != "never") {
        output::error("Invalid color mode: " + settings.color);
        return false;
    }

    return true;
}

bool load_settings(Settings &settings) {
    const auto config_path = paths::settings_file();

    // No config yet? Just use the default settings.
    if (!std::filesystem::exists(config_path))
        return validate_settings(settings);

    INIReader reader(config_path.string());

    if (reader.ParseError() != 0) {
        output::error("Failed to parse synnical.ini.");
        return false;
    }

    // Paths stay configurable, but validation keeps the target root isolated.
    settings.root = expand_home(reader.Get("paths", "root", settings.root.string()));
    settings.cache = expand_home(reader.Get("paths", "cache", settings.cache.string()));

    // General settings
    settings.color = reader.Get("general", "color", settings.color);
    settings.verbose = reader.GetBoolean("general", "verbose", settings.verbose);

    // Build settings
    auto jobs = reader.GetInteger("build", "jobs", settings.build_jobs);

    if (jobs <= 0) {
        output::error("Invalid build job count.");
        return false;
    }

    if (static_cast<unsigned long long>(jobs) >
        std::numeric_limits<unsigned int>::max()) {
        output::error("Build job count is too large.");
        return false;
    }
    settings.build_jobs = static_cast<unsigned int>(jobs);
    settings.show_build_output =
        reader.GetBoolean("build", "show_output", settings.show_build_output);

    // Download settings
    settings.show_progress = reader.GetBoolean("download", "progress", settings.show_progress);

    settings.progress_style = reader.Get("download", "style", settings.progress_style);

    return validate_settings(settings);
}
