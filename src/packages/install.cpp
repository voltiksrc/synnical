#include <filesystem>
#include <string>
#include <toml++/toml.hpp>
#include <vector>

#include "build.hpp"
#include "database.hpp"
#include "deploy.hpp"
#include "download.hpp"
#include "extract.hpp"
#include "install.hpp"
#include "output.hpp"
#include "recipe.hpp"
#include "sha256.hpp"
#include "stage.hpp"

using std::string;

namespace fs = std::filesystem;

bool is_source_root(const fs::path &path) {
    return fs::exists(path / "meson.build") || fs::exists(path / "CMakeLists.txt") ||
           fs::exists(path / "configure") || fs::exists(path / "Makefile") ||
           fs::exists(path / "Cargo.toml") || fs::exists(path / "wscript") ||
           fs::exists(path / "go.mod") || fs::exists(path / "Configure");
}

bool install_package(const string &package, const string &expected_version) {

    if (!fs::is_regular_file(".cache/repos/main/index.toml")) {
        output::error("Repository index missing, Run omi --sync first.");
        return false;
    }

    auto index = toml::parse_file(".cache/repos/main/index.toml");

    auto recipe = index["packages"][package].value<std::string>();

    if (!recipe) {
        output::error("Package not found in repository: " + package);
        return false;
    }

    auto index_url = index["repository"]["indexurl"].value<std::string>();

    if (!index_url) {
        output::error("Repository index URL missing.");
        return false;
    }

    std::string base_url = index_url->substr(0, index_url->find_last_of('/') + 1);

    std::string recipe_url = base_url + *recipe;

    fs::path recipe_path = fs::path(".cache/repos/main") / *recipe;

    if (!download_recipe(recipe_url, recipe_path)) {
        output::error("Failed to download recipe.");
        return false;
    }

    Package info = read_recipe(recipe_path.string());

    if (info.name != package || info.version != expected_version) {
        output::error("Recipe does not match the selected package: " + package + "-" +
                      expected_version);
        return false;
    }

    fs::path target_root = "/tmp/omi-test-root";
    fs::path database_path = target_root / "var/lib/omi/installed.db";

    fs::create_directories(database_path.parent_path());

    Database db(database_path);

    if (!db.initialize()) {
        output::error("Failed to initialize database.");
        return false;
    }

    InstallStatus status = db.is_installed(info.name);

    if (status == InstallStatus::Error) {
        output::error("Could not check package installation status");
        return false;
    }

    if (status == InstallStatus::Installed) {
        output::error("Package already installed: " + info.name);
        return false;
    }

    // Checks if any of the stages are empty and if it is we exit before doing the
    // DEBI steps.
    if (info.name.empty() || info.version.empty() || info.url.empty()) {
        output::error("Invalid or incomplete package recipe.");
        return false;
    }

    // Download the package and save the archive path so we can use it later.
    output::stage("Downloading " + info.name + "...");
    string archive = download_source(info.url, info.name, info.version);

    if (archive.empty()) {
        return false;
    }

    // SHA256 call
    output::stage("Verifying SHA-256");

    if (!verify_sha256(archive, info.sha256)) {
        return false;
    }

    // Extract the downloaded archive after verifying its SHA-256.
    output::stage("Extracting sources");

    if (!extract_source(archive, info.name, info.version, ".cache/build")) {
        output::error("Extraction failed.");
        return false;
    }

    // Find the source directory.
    fs::path extract_dir = fs::path(".cache/build") / (info.name + "-" + info.version);

    fs::path source_dir;

    if (!fs::is_directory(extract_dir)) {
        output::error("Extracted source directory is missing.");
        return false;
    }

    // Check the extraction directory first.
    if (is_source_root(extract_dir)) {
        source_dir = extract_dir;
    }

    // Then check its immediate subdirectories.
    if (source_dir.empty()) {
        for (const auto &entry : fs::directory_iterator(extract_dir)) {
            if (entry.is_directory() && is_source_root(entry.path())) {
                source_dir = entry.path();
                break;
            }
        }
    }

    // Finally, search deeper directories.
    if (source_dir.empty()) {
        for (const auto &entry : fs::recursive_directory_iterator(extract_dir)) {
            if (entry.is_directory() && is_source_root(entry.path())) {
                source_dir = entry.path();
                break;
            }
        }
    }

    if (source_dir.empty()) {
        output::error("Could not locate package source directory.");
        return false;
    }

    // build the package after extracting.
    output::stage("Building " + info.name + "-" + info.version);
    if (!build_source(info, source_dir)) {
        return false;
    }

    // Stage the compiled package.
    fs::path stage_dir = fs::path(".cache/stage") / (info.name + "-" + info.version);

    output::stage("Staging " + info.name + "-" + info.version);
    if (!stage_source(info, source_dir, stage_dir)) {
        output::error("Installation staging failed.");
        return false;
    }
    output::info("Files staged successfully.");

    output::stage("Deploying " + info.name + "-" + info.version);

    Deployment deployment;

    if (!install_staged_files(stage_dir, target_root, deployment)) {
        return false;
    }

    if (!db.record_installation(info.name, info.version, deployment.files)) {
        output::error("Failed to record installation.");

        output::stage("Rolling back installation...");

        if (!rollback_deployment(target_root, deployment)) {
            output::error("Rollback incomplete");
        }

        return false;
    }

    output::info("Package installed successfully.");
    return true;
}
