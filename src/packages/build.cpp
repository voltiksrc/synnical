#include "build.hpp"
#include "output.hpp"
#include "process.hpp"

#include <filesystem>

// Builds the package using its specified build system.
bool build_source(const Package &info, const std::filesystem::path &source_dir) {
    namespace fs = std::filesystem;

    output::info("Build system: " + info.build_system);

    if (info.build_system == "make") {
        if (fs::exists(source_dir / "configure")) {
            output::info("Running configure...");

            if (run_command({"./configure", "--prefix=/usr"}, source_dir) != 0) {
                output::error("Make configure failed, how about you fix it yourself?");
                return false;
            }
        }

        output::info("Running make...");

        if (run_command({"make", "-j4"}, source_dir) != 0) {
            output::error("Make compile failed sadly.");
            return false;
        }
    } else if (info.build_system == "meson") {
        output::info("Configuring with Meson...");

        if (run_command({"meson", "setup", "build", "--prefix=/usr"}, source_dir) != 0) {
            output::error("Meson setup failed. How about you switch to cmake?");
            return false;
        }

        output::info("Compiling with Meson...");

        if (run_command({"meson", "compile", "-C", "build"}, source_dir) != 0) {
            output::error("Meson compile failed, CMake is better anyway.");
            return false;
        }

    } else if (info.build_system == "cmake") {
        output::info("Configuring with CMake...");

        if (run_command({"cmake", "-S", ".", "-B", "build", "-DCMAKE_INSTALL_PREFIX=/usr"},
                        source_dir) != 0) {
            output::error("CMake configure failed so fix it.");
            return false;
        }

        output::info("Compiling with CMake...");

        if (run_command({"cmake", "--build", "build", "--parallel"}, source_dir) != 0) {
            output::error("oh dude, cmake build failed? oh well.");
            return false;
        }

    } else if (info.build_system == "cargo") {
        output::info("Building with Cargo...");

        if (run_command({"cargo", "build", "--release"}, source_dir) != 0) {
            output::error("oh dude, cargo build failed? oh well.");
            return false;
        }
    } else if (info.build_system == "waf") {
        output::info("Building with Waf...");

        // Waf uses a configure step before compile
        if (run_command({"./waf", "configure", "--prefix=/usr"}, source_dir) != 0) {
            output::error("oh dude what? waf configuration failed? oh well.");
            return false;
        }

        // Compile the package using its Waf build script
        if (run_command({"./waf", "build"}, source_dir) != 0) {
            output::error("Aww man waf build failed.");
            return false;
        }
    } else if (info.build_system == "go") {
        output::info("Building with Go...");

        // Keep the compiled executable in its own directory
        fs::path build_dir = fs::absolute(source_dir / ".synnical-go-build");

        fs::create_directories(build_dir);

        fs::path binary = build_dir / info.name;

        if (run_command({"go", "build", "-o", binary.string(), "."}, source_dir) != 0) {
            output::error("Go build failed, and you should go fix it.");
            return false;
        }
    } else if (info.build_system == "openssl") {
        output::info("Building with openssl backend...");

        if (run_command({"./Configure", "--prefix=/usr", "--libdir=lib"}, source_dir) != 0) {
            return false;
        }

        if (run_command({"make", "-j8"}, source_dir) != 0) {
            output::error("OpenSSL build failed, and you should go fix it.");
            return false;
        }
    } else if (info.build_system == "rust-bootstrap") {
        output::stage("Bootstrapping Rust toolchain...");

        if (run_command({"./configure", "--set", "install.prefix=/usr"}, source_dir) != 0) {
            return false;
        }

        if (run_command({"./x.py", "build"}, source_dir) != 0) {
            return false;
        }
    } else {
        output::error("Unsupported build system: " + info.build_system);
        return false;
    }

    output::stage("Build completed successfully. WOOHOO");
    return true;
}
