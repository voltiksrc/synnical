#include <filesystem>
#include <fstream>
#include <string>

#include "state.hpp"

using std::string;

namespace fs = std::filesystem;

BuildState load_state(const fs::path &path) {

    std::ifstream file(path);

    BuildState buildstate;

    if (!file) {
        return BuildState{};
    }

    string line;
    while (std::getline(file, line)) {
        auto pos = line.find('=');

        if (pos != string::npos) {

            string key = line.substr(0, pos);
            string value = line.substr(pos + 1);

            if (key == "source_hash") {
                buildstate.source_hash = value;
            } else if (key == "extracted") {
                buildstate.extracted = (value == "true");
            } else if (key == "configured") {
                buildstate.configured = (value == "true");
            } else if (key == "built") {
                buildstate.built = (value == "true");
            }
        }
    }

    return buildstate;
}

bool save_state(const fs::path &path, const BuildState &state) {
    std::ofstream file(path);

    if (!file) {
        return false;
    }

    file << "source_hash=" << state.source_hash << '\n';
    file << std::boolalpha;
    file << "extracted=" << state.extracted << '\n';
    file << "configured=" << state.configured << '\n';
    file << "built=" << state.built << '\n';

    file.flush();
    return file.good();
}
