#pragma once

#include <string>
#include <string_view>

// Formats selected build lines for the terminal. The complete output is kept
// separately by the process runner so diagnostics aren't lost on failure.
class BuildOutput {
public:
    void feed(std::string_view chunk);
    void finish();

private:
    void line(std::string_view text);

    std::string pending_;
    bool discarding_ = false;
};
