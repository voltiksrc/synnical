#include "state.hpp"
#include <iostream>

int main() {
    BuildState state;

    state.source_hash = "abc123";
    state.extracted = true;

    save_state("test.state", state);

    BuildState loaded = load_state("test.state");

    std::cout << std::boolalpha;
    std::cout << "Hash: " << loaded.source_hash << '\n';
    std::cout << "Extracted: " << loaded.extracted << '\n';
}
