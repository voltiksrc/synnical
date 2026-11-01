#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <system_error>
#include <toml++/toml.hpp>

using std::string;

namespace fs = std::filesystem;

bool scan_pkgdir(const fs::path &repo_path) {
    fs::path packages_dir = repo_path / "packages";

    if (!fs::is_directory(packages_dir)) {
        std::cerr << "Packages directory missing.\n";
        return false;
    }

    std::map<string, string> packages;

    fs::path index_path = repo_path / "index.toml";

    auto index = toml::parse_file(index_path.string());
    auto index_url = index["repository"]["indexurl"].value<string>();

    if (!index_url || index_url->empty()) {
        std::cerr << "Missing repository index URL.\n";
        return false;
    }

    for (const auto &entry : fs::directory_iterator(packages_dir)) {
        if (!entry.is_directory())
            continue;

        fs::path recipe = entry.path() / "package.toml";

        if (!fs::is_regular_file(recipe))
            continue;

        auto data = toml::parse_file(recipe.string());
        auto name = data["package"]["name"].value<string>();

        if (!name || name->empty()) {
            std::cerr << "Recipe name is empty.\n";
            return false;
        }

        auto [it, inserted] =
            packages.emplace(*name, recipe.lexically_relative(repo_path).generic_string());

        if (!inserted) {
            std::cerr << "Duplicate package: " << *name << '\n';
            return false;
        }
    }

    if (packages.empty()) {
        std::cerr << "No packages found. Refusing to replace index.\n";
        return false;
    }

    for (const auto &[name, path] : packages) {
        std::cout << "-> " << name << " = " << path << '\n';
    }

    fs::path output_path = repo_path / "index.generated.toml";

    std::cout << ">> Writing index.generated.toml..\n";

    std::ofstream file(output_path);

    if (!file) {
        std::cerr << "Failed to create index.\n";
        return false;
    }

    file << "[repository]\n";
    file << "indexurl = \"" << *index_url << "\"\n\n";
    file << "[packages]\n";

    for (const auto &[name, path] : packages) {
        file << name << " = \"" << path << "\"\n";
    }

    file.flush();

    if (!file.good()) {
        std::cerr << "Failed to write index.\n";
        return false;
    }

    file.close();

    if (file.fail()) {
        std::cerr << "Failed to close index.\n";
        return false;
    }

    try {
        auto generated = toml::parse_file(output_path.string());

        auto url = generated["repository"]["indexurl"].value<string>();
        auto generated_packages = generated["packages"].as_table();

        if (!url || url->empty() || !generated_packages) {
            std::cerr << "Generated index is invalid.\n";
            return false;
        }

    } catch (const toml::parse_error &error) {
        std::cerr << "TOML validation failed: " << error << '\n';
        return false;
    }

    fs::path final_path = repo_path / "index.toml";
    std::error_code ec;

    fs::rename(output_path, final_path, ec);

    if (ec) {
        std::cerr << "Failed to rename index.generated.toml: " << ec.message() << '\n';
        return false;
    }

    return true;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Enter repository path.\n";
        return 77;
    }

    fs::path repo_path = argv[1];

    return scan_pkgdir(repo_path) ? 0 : 1;
}
