#pragma once

#include <optional>
#include <string>
#include <vector>

struct Arguments {
    std::string command;
    std::vector<std::string> positionals;
    bool verbose = false;
    bool resume = false;
};

std::optional<Arguments> parse_arguments(int argc, char *argv[]);
