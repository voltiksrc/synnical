#include <filesystem>
#include <iostream>
#include <sstream>

#include "../general/output.hpp"
#include "build_output.hpp"

namespace {

bool compiler_command(const std::string &text, std::string &summary) {
    std::istringstream words(text);
    std::string tool;
    if (!(words >> tool))
        return false;

    const auto name = std::filesystem::path(tool).filename().string();
    if (name != "cc" && name != "c++" && name != "gcc" && name != "g++" && name != "clang" &&
        name != "clang++" && name != "clang-cl")
        return false;

    // Make usually echoes the entire command; pick out the source filename.
    std::string arg;
    while (words >> arg) {
        if (arg == "-c" && (words >> arg)) {
            summary = "Compiling " + std::filesystem::path(arg).filename().string();
            return true;
        }
    }
    return false;
}

bool interesting(std::string_view line) {
    // Common Make, CMake, Ninja, Meson, Cargo and compiler messages.
    constexpr std::string_view markers[] = {
        "Building ", "Compiling ",    "Linking ",     "Generating ",  " CC ",
        " CXX ",     " LD ",          " AR ",         " GEN ",        "CC ",
        "CXX ",      "LD ",           "AR ",          "GEN ",         "error:",
        "warning:",  "fatal error:",  "FAILED:",      "ninja: error", "undefined reference",
        "Error ",    "   Compiling ", "   Finished ", "   Checking "};

    for (auto marker : markers) {
        if (line.find(marker) != std::string_view::npos)
            return true;
    }
    return false;
}

} // namespace

void BuildOutput::line(std::string_view text) {
    // Don't forward control characters from compiler output to the terminal.
    std::string clean;
    clean.reserve(text.size());
    for (unsigned char c : text) {
        if (c == '\t')
            clean.push_back(' ');
        else if (c >= 32 && c != 127)
            clean.push_back(static_cast<char>(c));
    }

    std::string summary;
    if (compiler_command(clean, summary))
        clean = std::move(summary);
    else if (!interesting(clean))
        return;

    // Keep ordinary compiler details short. The full line is still in the log.
    constexpr std::size_t max_width = 170;
    if (clean.size() > max_width)
        clean.resize(max_width);

    // Color only the build action, leaving filenames untouched.
    std::string_view action;
    std::string_view color;

    if (clean.find("Compiling ") != std::string::npos) {
        action = "Compiling";
        color = output::color::blue;
    } else if (clean.find("Linking ") != std::string::npos) {
        action = "Linking";
        color = output::color::purple;
    } else if (clean.find("Generating ") != std::string::npos) {
        action = "Generating";
        color = output::color::yellow;
    }

    if (!action.empty() && output::use_color(std::cout)) {
        const auto position = clean.find(action);

        std::cout << "   " << clean.substr(0, position) << color << action << output::color::reset
                  << clean.substr(position + action.size()) << '\n';
    } else {
        std::cout << "   " << clean << '\n';
    }
}

void BuildOutput::feed(std::string_view chunk) {
    for (char c : chunk) {
        if (c == '\n' || c == '\r') {
            if (!discarding_)
                line(pending_);
            pending_.clear();
            discarding_ = false;
        } else if (!discarding_) {
            // Broken tools can print endless lines; don't grow memory forever.
            if (pending_.size() < 8192)
                pending_ += c;
            else
                discarding_ = true;
        }
    }
}

void BuildOutput::finish() {
    if (!discarding_ && !pending_.empty())
        line(pending_);
    pending_.clear();
    discarding_ = false;
}
