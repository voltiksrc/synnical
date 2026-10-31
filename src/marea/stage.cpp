#include <filesystem>

#include "output.hpp"
#include "process.hpp"
#include "recipe.hpp"
#include "stage.hpp"

bool stage_source(const Package &info, const std::filesystem::path &source_dir,
                  const std::filesystem::path &stage_dir) {
    namespace fs = std::filesystem;

    // Make DESTDIR absolute before passing it to the build system.
    fs::path absolute_path = fs::absolute(stage_dir);

    // Create the staging directory.
    fs::create_directories(absolute_path);

    // Pass DESTDIR to the child process.
    std::string destdir = "DESTDIR=" + absolute_path.string();

    if (info.build_system == "make") {
        if (run_command({"make", "install", "PREFIX=" + (absolute_path / "usr").string()},
                        source_dir,
                        {destdir}) != 0) {
            output::error("Failed to stage Make files.");
            return false;
        }

    } else if (info.build_system == "meson") {
        if (run_command({"meson", "install", "-C", "build"}, source_dir, {destdir}) != 0) {
            output::error("Failed to stage Meson files.");
            return false;
        }

    } else if (info.build_system == "cmake") {
        if (run_command({"cmake", "--install", "build"}, source_dir, {destdir}) != 0) {
            output::error("Failed to stage CMake files.");
            return false;
        }
    } else if (info.build_system == "cargo") {
        fs::path binary = source_dir / "target/release" / info.name;
        fs::path destination = absolute_path / "usr/bin" / info.name;

        if (!fs::is_regular_file(binary)) {
            output::error("Cargo binary not found: " + binary.string());
            return false;
        }

        fs::create_directories(destination.parent_path());

        fs::copy_file(binary, destination, fs::copy_options::overwrite_existing);

        fs::permissions(destination,
                        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                        fs::perm_options::add);

        output::stage("Staged Cargo executable: " + info.name);

    } else if (info.build_system == "waf") {
        // Install into our staging directory instead of real root.
        if (run_command({"./waf", "install", "--destdir=" + absolute_path.string()}, source_dir) !=
            0) {
            output::error("Failed to stage Waf files.");
            return false;
        }
    } else if (info.build_system == "go") {
        // Get the executable we already compiled in build.cpp
        fs::path binary = fs::absolute(source_dir / ".synnical-go-build" / info.name);

        // This is where synnical will stage the executable
        fs::path destination = absolute_path / "usr/bin" / info.name;

        if (!fs::is_regular_file(binary)) {
            output::error("Go executable not found: " + binary.string());
            return false;
        }

        // Create the bin directory if it doesnt exist yet
        fs::create_directories(destination.parent_path());

        fs::copy_file(binary, destination, fs::copy_options::overwrite_existing);

        output::info("Go executable staged successfully");
    } else if (info.build_system == "openssl") {
        // OpenSSL custom make install command
        if (run_command({"make", "install_sw", "DESTDIR=" + absolute_path.string()}, source_dir) !=
            0) {
            output::error("Failed to stage OpenSSL files.");
            return false;
        }
    } else if (info.build_system == "rust-bootstrap") {
        if (run_command({"./x.py", "install"}, source_dir, {destdir}) != 0) {
            return false;
        }
    } else {
        output::error("Build type unsupported.");
        return false;
    }

    return true;
}
