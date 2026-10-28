#pragma once
#include <iostream>
#include <string_view>

namespace output {
    inline void stage(std::string_view message) {
        std::cout << ">> " << message << '\n';
    }

    inline void info(std::string_view message) {
        std::cout << "-> " << message << '\n';
    }

    inline void error(std::string_view message) {
        std::cerr << "!!! " << message << '\n';
    }
}
