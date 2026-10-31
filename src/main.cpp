#include <iostream>
#include <sodium.h>
#include <string>
#include <string_view>
#include <vector>

#include "config/daycare.hpp"
#include "general/output.hpp"
#include "marea/database.hpp"
#include "marea/install.hpp"
#include "marea/remove.hpp"
#include "marea/search.hpp"
#include "marea/solver.hpp"
#include "repository/sync.hpp"

using std::string;

void smversion() {
    constexpr std::string_view version{"1.10"};
    std::cout << "Synnical version " << version << '\n';
}

void show_help() {
    std::cout << "Synnical - Source-based system manager\n\n";
    std::cout << "Usage: synnical <command> [package...]\n\n";

    std::cout << "Commands:\n";
    std::cout << "  -Fetch <package...> Install packages using command-line\n";
    std::cout << "  remove <package>    Remove an installed package\n";
    std::cout << "  rm <package>        Alias for remove\n";
    std::cout << "  search <query>      Search repositories\n";
    std::cout << "  resolve <package>   Preview package dependencies\n";
    std::cout << "  apply               Apply the daycare configuration\n";
    std::cout << "  --list-installed    List installed packages\n";
    std::cout << "  --sync              Sync repositories\n";
    std::cout << "  --version           Show Synnical's version\n";
    std::cout << "  --help, -h          Show this menu\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Usage: synnical <command> [package...]\n";
        return 1;
    }

    if (sodium_init() < 0) {
        output::error("Failed to initialize libsodium.");
        return 1;
    }

    string command = argv[1];

    if (command == "-h" || command == "--help") {
        show_help();
        return 0;
    }

    if (command == "--version") {
        smversion();
        return 0;
    }

    if (command == "--sync") {
        return sync_repositories() ? 0 : 1;
    }

    if (command == "--list-installed") {
        Database db("/tmp/synnical-test-root/var/lib/synnical/installed.db");

        if (!db.initialize()) {
            output::error("Failed to initialize database.");
            return 1;
        }

        return db.list_installed() ? 0 : 1;
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

        if (!apply_timezone(config->timezone, "/tmp/synnical-test-root")) {
            return 1;
        }

        return 0;
    }
    // Commands below require at least one package argument.
    if (argc < 3) {
        output::error("Missing package argument.");
        return 1;
    }

    string package = argv[2];

    if (command == "-Fetch" || command == "-FETCH" || command == "-fetch") {

        std::vector<std::string> requested;

        for (int i = 2; i < argc; ++i) {
            requested.emplace_back(argv[i]);
        }

        return install_requested(requested) ? 0 : 1;

        return 0;
    }

    if (command == "remove" || command == "rm") {
        for (int i = 2; i < argc; ++i) {
            if (!remove_package(argv[i]))
                return 1;
        }

        return 0;
    }

    if (command == "search") {
        return search_packages(package) ? 0 : 1;
    }

    if (command == "resolve") {
        return resolve_package(package) ? 0 : 1;
    }

    output::error("Unknown command: " + command);
    return 1;
}
