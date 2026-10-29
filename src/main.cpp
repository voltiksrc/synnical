#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "packages/database.hpp"
#include "packages/install.hpp"
#include "general/output.hpp"
#include "packages/remove.hpp"
#include "packages/search.hpp"
#include "packages/solver.hpp"
#include "repository/sync.hpp"
#include "config/daycare.hpp"

using std::string;

void smversion() {
    constexpr std::string_view version{"1.10"};
    std::cout << "Synnical version " << version << '\n';
}

void show_help() {
    std::cout << "Synnical Package Manager\n\n";
    std::cout << "Usage: synnical <command> [package...]\n\n";

    std::cout << "Commands:\n";
    std::cout << "  -Fetch <package...> Fetch and install packages\n";
    std::cout << "  remove <package>    Remove an installed package\n";
    std::cout << "  rm <package>        Alias for remove\n";
    std::cout << "  search <query>      Search repositories\n";
    std::cout << "  resolve <package>   Preview package dependencies\n";
    std::cout << "  apply               Apply the configuration\n";
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
        Database db("/tmp/synnical-root/var/lib/omi/installed.db");

        if (!db.initialize()) {
            output::error("Failed to initialize database.");
            return 1;
        }

        return db.list_installed() ? 0 : 1;
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

        std::vector<PlannedPackage> plan;

        // Resolve all requested packages together.
        if (!solve_install_plan(requested, plan))
            return 1;

        // Install dependencies before their dependents.
        for (const auto &entry : plan) {
            output::stage("Installing " + entry.name + "-" + entry.version);

            // Verify that the recipe matches the solver's version.
            if (!install_package(entry.name, entry.version)) {
                output::error("Installation stopped at " + entry.name);
                return 1;
            }
        }

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

    if (command == "apply") {
        Config config = parse_daycare("examples/daycare.toml");
    
        std::cout<<"Hostname->"<<config.hostname<<'\n';
        std::cout<<"Timezone->"<<config.timezone<<'\n';

        for (const auto& package : config.packages) {
             std::cout<<package<<'\n';
        } 
    }

    output::error("Unknown command: " + command);
    return 1;
}
