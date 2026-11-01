
#pragma once

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <unistd.h>

namespace output {

enum class ColorMode { Auto, Always, Never };

inline ColorMode color_mode = ColorMode::Auto;

inline void set_color_mode(ColorMode mode) {
    color_mode = mode;
}

namespace color {
inline constexpr std::string_view yellow = "\033[33m";
inline constexpr std::string_view green = "\033[32m";
inline constexpr std::string_view red = "\033[31m";
inline constexpr std::string_view reset = "\033[0m";
} // namespace color

inline bool use_color(std::ostream &stream) {
    if (color_mode == ColorMode::Never)
        return false;

    if (color_mode == ColorMode::Always)
        return true;

    // Auto mode: respect NO_COLOR and terminal detection.
    if (std::getenv("NO_COLOR") != nullptr)
        return false;

    return isatty(&stream == &std::cerr ? STDERR_FILENO : STDOUT_FILENO);
}

inline void print(std::ostream &stream, std::string_view prefix, std::string_view message,
                  std::string_view color_code = {}) {

    if (use_color(stream) && !color_code.empty()) {
        stream << color_code << prefix << color::reset;
    } else {
        stream << prefix;
    }

    stream << message << '\n';
}

inline void stage(std::string_view message) {
    static bool first_stage = true;

    if (!first_stage)
        std::cout << '\n';

    print(std::cout, ":: ", message, color::yellow);
    first_stage = false;
}

inline void info(std::string_view message) {
    print(std::cout, "   ", message);
}

inline void success(std::string_view message) {
    print(std::cout, "✓ ", message, color::green);
}

inline void warn(std::string_view message) {
    print(std::cerr, "! ", message, color::yellow);
}

inline void error(std::string_view message) {
    print(std::cerr, "✗ ", message, color::red);
}

} // namespace output
