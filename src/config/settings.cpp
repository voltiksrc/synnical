#include <INIReader.h>

#include "../general/output.hpp"
#include "settings.hpp"

Settings default_settings() {
    return Settings{};
}

bool validate_settings(const Settings &settings) {
    if (settings.build_jobs == 0) {
        output::error("Build jobs must be greater than zero.");
        return false;
    }

    if (settings.root.empty()) {
        output::error("Installation root cannot be empty.");
        return false;
    }

    if (settings.verbose != true && settings.verbose != false) {
        output::error("Invalid verbose option: " + settings.verbose);
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
        return true;

    INIReader reader(config_path.string());

    if (reader.ParseError() != 0) {
        output::error("Failed to parse synnical.ini.");
        return false;
    }

    // General settings
    settings.color = reader.Get("general", "color", settings.color);
    settings.verbose = reader.GetBoolean("general", "verbose", settings.verbose);

    // Build settings
    auto jobs = reader.GetInteger("build", "jobs", settings.build_jobs);

    if (jobs <= 0) {
        output::error("Invalid build job count.");
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
