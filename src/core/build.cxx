#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "../common/output.hpp"
#include "../system/process.hpp"
#include "build.hpp"

namespace fs = std::filesystem;

using std::string;
using std::vector;

// Give builds access to packages in Synnical's target root first.
// This doesn't sandbox builds: they can still see the host system.
static vector<string> make_build_environment(const Settings &settings) {
    const fs::path usr = settings.root / "usr";
    const string include = (usr / "include").string();
    const string lib = (usr / "lib").string();
    const string lib64 = (usr / "lib64").string();

    auto existing = [](const char *name) -> string {
        const char *value = std::getenv(name);
        return value ? value : "";
    };

    // Compiler flags use spaces; search paths use colons.
    auto flags = [&](const string &name, const string &extra) -> string {
        const string old = existing(name.c_str());
        return name + "=" + extra + (old.empty() ? "" : " " + old);
    };

    auto search_path = [&](const string &name, const string &extra) -> string {
        const string old = existing(name.c_str());
        return name + "=" + extra + (old.empty() ? "" : ":" + old);
    };

    // Only the child process gets these variables, not Synnical itself.
    return {
        flags("CPPFLAGS", "-I" + include),
        flags("CFLAGS", "-I" + include),
        flags("CXXFLAGS", "-I" + include),
        flags("LDFLAGS", "-L" + lib + " -L" + lib64),
        search_path("PKG_CONFIG_PATH",
                    lib + "/pkgconfig:" + lib64 +
                        "/pkgconfig:" + (usr / "share/pkgconfig").string()),
        search_path("CMAKE_PREFIX_PATH", usr.string()),
        search_path("PATH", (usr / "bin").string()),
        flags("CGO_CPPFLAGS", "-I" + include),
        flags("CGO_LDFLAGS", "-L" + lib + " -L" + lib64),
    };
}

// Build the package using the system selected in its recipe.
bool build_source(const Package &info, const fs::path &source_dir, const Settings &settings) {
    if (settings.build_jobs == 0) {
        output::error("Build jobs must be greater than zero.");
        return false;
    }

    const fs::path usr = settings.root / "usr";
    const auto build_env = make_build_environment(settings);
    const auto mode = settings.verbose || settings.show_build_output
                          ? ProcessOutput::Full : ProcessOutput::Filtered;
    const string jobs = std::to_string(settings.build_jobs);

    output::info("Build system: " + info.build_system);

    // Show compiler activity live; keep the complete log for failures.
    auto run = [&](const vector<string> &args, const string &step) -> bool {
        output::info(step);
        if (run_command(args, source_dir, build_env, mode) == 0)
            return true;

        output::error("Failed to " + step + " for " + info.name + ".");
        return false;
    };

    if (info.build_system == "make") {
        // Most autotools packages use configure. CMake's source release
        // instead uses bootstrap, which creates its Makefiles.
        if (fs::is_regular_file(source_dir / "configure")) {
            if (!run({"./configure", "--prefix=/usr"}, "configure sources"))
                return false;
        } else if (fs::is_regular_file(source_dir / "bootstrap")) {
            if (!run({"./bootstrap", "--prefix=/usr"}, "bootstrap sources"))
                return false;
        }

        if (!run({"make", "-j" + jobs}, "compile sources"))
            return false;

    } else if (info.build_system == "meson") {
        const string pkg_dirs = (usr / "lib/pkgconfig").string() + ":" +
                                (usr / "lib64/pkgconfig").string() + ":" +
                                (usr / "share/pkgconfig").string();

        if (!run({"meson", "setup", "build", "--prefix=/usr", "--pkg-config-path=" + pkg_dirs},
                 "configure with Meson"))
            return false;

        if (!run({"meson", "compile", "-C", "build", "-j", jobs}, "compile with Meson"))
            return false;

    } else if (info.build_system == "cmake") {
        if (!run(
                {"cmake",
                 "-S",
                 ".",
                 "-B",
                 "build",
                 "-DCMAKE_INSTALL_PREFIX=/usr",
                 "-DCMAKE_PREFIX_PATH=" + usr.string(),
                 "-DCMAKE_INCLUDE_PATH=" + (usr / "include").string(),
                 "-DCMAKE_LIBRARY_PATH=" + (usr / "lib").string() + ";" + (usr / "lib64").string()},
                "configure with CMake"))
            return false;

        if (!run({"cmake", "--build", "build", "--parallel", jobs}, "compile with CMake"))
            return false;

    } else if (info.build_system == "cargo") {
        if (!run({"cargo", "build", "--release", "-j", jobs}, "compile with Cargo"))
            return false;

    } else if (info.build_system == "waf") {
        // Waf configures and builds through the same ./waf script.
        if (!run({"./waf", "configure", "--prefix=/usr"}, "configure with Waf"))
            return false;

        if (!run({"./waf", "build", "-j", jobs}, "compile with Waf"))
            return false;

    } else if (info.build_system == "go") {
        // Keep Go's binary separate from the extracted source files.
        const fs::path build_dir = fs::absolute(source_dir / ".synnical-go-build");
        fs::create_directories(build_dir);
        const fs::path binary = build_dir / info.name;

        if (!run({"go", "build", "-p", jobs, "-o", binary.string(), "."}, "compile with Go"))
            return false;

    } else if (info.build_system == "openssl") {
        // OpenSSL uses an uppercase Configure script.
        if (!run({"./Configure", "--prefix=/usr", "--libdir=lib"}, "configure OpenSSL"))
            return false;

        if (!run({"make", "-j" + jobs}, "compile OpenSSL"))
            return false;

    } else if (info.build_system == "perl") {
        // Perl also uses uppercase Configure, but its flags are different.
        if (!run({"sh", "Configure", "-des", "-Dprefix=/usr"}, "configure Perl"))
            return false;

        if (!run({"make", "-j" + jobs}, "compile Perl"))
            return false;

    } else if (info.build_system == "rust-bootstrap") {
        // Rust's x.py handles its own build stages.
        if (!run({"./configure", "--set", "install.prefix=/usr"}, "configure Rust"))
            return false;

        if (!run({"./x.py", "build", "-j", jobs}, "bootstrap Rust"))
            return false;

    } else {
        output::error("Unsupported build system: " + info.build_system);
        return false;
    }

    output::success("Built " + info.name + "-" + info.version);
    return true;
}
