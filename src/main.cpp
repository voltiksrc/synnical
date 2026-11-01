#include "general/paths.hpp"
#include <exception>
#include <filesystem>
#include <iostream>
#include <sodium.h>
#include <string>
#include <string_view>
#include <vector>

#include "cli/parser.hpp"
#include "config/daycare.hpp"
#include "config/settings.hpp"
#include "general/output.hpp"
#include "marea/database.hpp"
#include "marea/install.hpp"
#include "marea/remove.hpp"
#include "marea/search.hpp"
#include "marea/solver.hpp"
#include "repository/sync.hpp"

using std::string;

void version() {
    constexpr std::string_view version{"1.10"};
    std::cout << "Synnical version " << version << '\n';
}

void show_help() {
    std::cout << "Synnical - Source-based system manager\n\n";
    std::cout << "Usage: synnical <command> [package...]\n\n";

    std::cout << "Commands:\n";
    std::cout << "  -Fetch <package...> Install packages using command line\n";
    std::cout << "  remove <package>    Remove an installed package\n";
    std::cout << "  search <query>      Search repositories\n";
    std::cout << "  resolve <package>   Preview package dependencies\n";
    std::cout << "  apply               Apply the daycare configuration\n";
    std::cout << "  --list-installed    List installed packages\n";
    std::cout << "  --sync              Sync repositories\n";
    std::cout << "  --version           Show Synnical's version\n";
    std::cout << "  --help              Show this menu\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Usage: synnical <command> [package...]\n";
        return 1;
    }

    // Load settings before parsing CLI arguments
    Settings settings = default_settings();

    if (!load_settings(settings))
        return 1;

    // Set the color mode before parsing arguments.
    if (settings.color == "always") {
        output::set_color_mode(output::ColorMode::Always);
    } else if (settings.color == "never") {
        output::set_color_mode(output::ColorMode::Never);
    } else {
        output::set_color_mode(output::ColorMode::Auto);
    }

    auto parsed = parse_arguments(argc, argv);

    if (!parsed) {
        return 1;
    }

    // CLI flags override the INI configuration.
    if (parsed->verbose)
        settings.verbose = true;

    const auto &command = parsed->command;
    const auto &positionals = parsed->positionals;
    bool verbose = parsed->verbose;

    if (command == "-h" || command == "--help") {
        show_help();
        return 0;
    }

    if (command == "--version" || command == "--ver") {
        version();
        return 0;
    }

    if (sodium_init() < 0) {
        output::error("Failed to initialize libsodium.");
        return 1;
    }

    if (command == "--sync") {
        return sync_repositories() ? 0 : 1;
    }

    if (command == "--list-installed") {
        const auto dblocation = paths::database();

        if (!std::filesystem::exists(dblocation)) {
            output::error("Database does not exist.");
            return 1;
        }

        try {
            Database db(dblocation);

            if (!db.initialize()) {
                output::error("Failed to initialize database.");
                return 1;
            }

            return db.list_installed() ? 0 : 1;
        } catch (const std::exception &e) {
            output::error(string("Database error: ") + e.what());
            return 1;
        }
    }

    if (command == "apply") {
        auto config = parse_daycare("examples/daycare.toml");

        if (!config) {
            return 1;
        }

        // Install requested packages first. Never change the target host OS.
        if (!config->packages.empty() && !install_requested(config->packages)) {
            return 1;
        }

        if (!apply_timezone(config->timezone, paths::root().string())) {
            return 1;
        }

        return 0;
    }

    if (command == "-Fetch" || command == "-fetch") {
        return install_requested(positionals) ? 0 : 1;
    }

    if (command == "remove" || command == "rm") {
        for (const auto &package : positionals) {
            if (!remove_package(package))
                return 1;
        }

        return 0;
    }

    if (command == "search") {
        return search_packages(positionals[0]) ? 0 : 1;
    }

    if (command == "resolve") {
        return resolve_package(positionals[0]) ? 0 : 1;
    }

    output::error("Unknown command: " + command);
    return 1;
}
