#include <optional>
#include <string>
#include <string_view>

#include "../general/output.hpp"
#include "parser.hpp"

using std::nullopt;
using std::string;
using std::string_view;

std::optional<Arguments> parse_arguments(int argc, char *argv[]) {
    Arguments arguments;

    for (int i = 1; i < argc; ++i) {
        string_view arg = argv[i];

        if (arg == "--verbose") {
            arguments.verbose = true;
            continue;
        }

        if (arg == "--resume") {
            arguments.resume = true;
            continue;
        }

        // Reject unrecognized flags.
        if (arg.starts_with('-') && arg != "-Fetch" && arg != "-fetch" && arg != "--help" &&
            arg != "-h" && arg != "--version" && arg != "--ver" && arg != "--sync" &&
            arg != "--list-installed") {

            output::error("Synnical: Unknown flag option: '" + string(arg) + "'");
            return nullopt;
        }

        // Recognize commands before collecting positional arguments.
        if (arguments.command.empty()) {
            if (arg == "-Fetch" || arg == "-fetch" || arg == "search" || arg == "resolve" ||
                arg == "remove" || arg == "rm" || arg == "apply" || arg == "--help" ||
                arg == "-h" || arg == "--version" || arg == "--ver" || arg == "--sync" ||
                arg == "--list-installed") {

                arguments.command = string(arg);
                continue;
            }

            output::error("Unknown command: '" + string(arg) + "'");
            return nullopt;
        }

        // Reject additional command flags.
        if (arg == "-Fetch" || arg == "-fetch" || arg == "--help" || arg == "-h" ||
            arg == "--version" || arg == "--ver" || arg == "--sync" || arg == "--list-installed") {
            output::error("Multiple commands specified.");
            return nullopt;
        }

        arguments.positionals.emplace_back(arg);
    }

    if (arguments.command.empty()) {
        output::error("Missing command.");
        return nullopt;
    }

    if (arguments.command == "apply") {
        arguments.resume = true;
    }

    const string &command = arguments.command;
    const auto count = arguments.positionals.size();

    if (arguments.resume && command != "apply" && command != "-Fetch" && command != "-fetch") {
        output::error("--resume is only supported with -Fetch and apply.");
        return nullopt;
    }

    // Commands requiring one or more packages.
    if (command == "-Fetch" || command == "-fetch" || command == "remove" || command == "rm") {

        if (count == 0) {
            output::error("Missing package argument.");
            return nullopt;
        }
    }

    // Commands requiring exactly one positional argument.
    else if (command == "search" || command == "resolve") {
        if (count == 0) {
            output::error(command == "search" ? "Missing search query."
                                              : "Missing package argument.");
            return nullopt;
        }

        if (count > 1) {
            output::error(command == "search" ? "Too many search arguments."
                                              : "Too many package arguments.");
            return nullopt;
        }
    }

    // Commands accepting no positional arguments.
    else if (count != 0) {
        output::error("Command '" + command + "' does not take arguments.");
        return nullopt;
    }

    return arguments;
}
