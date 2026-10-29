#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <toml++/toml.hpp>
#include <vector>

#define requires requires_deps
#include <solv/pool.h>
#include <solv/poolarch.h>
#include <solv/repo.h>
#include <solv/repo_write.h>
#include <solv/repodata.h>
#include <solv/solvable.h>
#undef requires

namespace fs = std::filesystem;

struct Package {
    std::string name;
    std::string version;
    std::string architecture;
    std::vector<std::string> dependencies;
    std::vector<std::string> build_dependencies;
    std::vector<std::string> provides;
};

Package read_package(const fs::path &path) {
    auto recipe = toml::parse_file(path.string());

    Package package;

    package.name = recipe["package"]["name"].value_or("");
    package.version = recipe["package"]["version"].value_or("");
    package.architecture = recipe["package"]["architecture"].value_or("x86_64");

    auto node1 = recipe["deps"]["requires"];

    if (node1 && !node1.is_array()) {
        throw std::runtime_error("requires must be an array in " + path.string());
    }

    if (auto *deps = node1.as_array()) {
        for (const auto &entry : *deps) {
            auto value = entry.value<std::string>();

            if (!value)
                throw std::runtime_error("Invalid dependency in " + path.string());

            // Empty strings should not become dependencies
            if (!value->empty())
                package.dependencies.push_back(*value);
        }
    }

    auto node = recipe["deps"]["build_requires"];

    if (node && !node.is_array()) {
        throw std::runtime_error("build_requires must be an array in " + path.string());
    }

    if (auto *deps = node.as_array()) {
        for (const auto &entry : *deps) {
            auto value = entry.value<std::string>();

            if (!value)
                throw std::runtime_error("Invalid dependency in " + path.string());

            // Empty strings should not become dependencies
            if (value->empty()) {
                throw std::runtime_error("Invalid dependencies in " + package.name +
                                         "! It is empty or malformed.");
            }

            package.build_dependencies.push_back(*value);
        }
    }

    if (package.name.empty() || package.version.empty())
        throw std::runtime_error("Incomplete recipe: " + path.string());

    auto provided = recipe["package"]["provides"];

    if (provided && !provided.is_array())
        throw std::runtime_error("provides must be an array");

    if (auto *array = provided.as_array()) {
        for (const auto &entry : *array) {
            auto value = entry.value<std::string>();

            if (!value || value->empty())
                throw std::runtime_error("Invalid provides entry");

            package.provides.push_back(*value);
        }
    }

    return package;
}

Id make_dependency(Pool *pool, const std::string &text) {
    std::istringstream stream(text);

    std::string name;
    std::string op;
    std::string version;
    std::string extra;

    if (!(stream >> name))
        return 0;

    Id name_id = pool_str2id(pool, name.c_str(), 1);

    if (!(stream >> op))
        return name_id;

    if (!(stream >> version) || (stream >> extra))
        return 0;

    int flags = 0;

    if (op == ">=")
        flags = REL_GT | REL_EQ;
    else if (op == "<=")
        flags = REL_LT | REL_EQ;
    else if (op == ">")
        flags = REL_GT;
    else if (op == "<")
        flags = REL_LT;
    else if (op == "=" || op == "==")
        flags = REL_EQ;
    else
        return 0;

    Id version_id = pool_str2id(pool, version.c_str(), 1);

    return pool_rel2id(pool, name_id, version_id, flags, 1);
}

bool add_package(Pool *pool, Repo *repo, const Package &package) {
    Id name_id = pool_str2id(pool, package.name.c_str(), 1);
    Id version_id = pool_str2id(pool, package.version.c_str(), 1);
    Id arch_id = pool_str2id(pool, package.architecture.c_str(), 1);

    Id versioned = pool_rel2id(pool, name_id, version_id, REL_EQ, 1);

    std::vector<Id> requirements;
    std::vector<Id> build_requirements;

    for (const auto &dependency : package.dependencies) {
        Id dep = make_dependency(pool, dependency);

        if (!dep) {
            std::cerr << "!!! Invalid dependency in " << package.name << ": " << dependency << '\n';
            return false;
        }

        requirements.push_back(dep);
    }

    for (const auto &build_dependency : package.build_dependencies) {
        Id build_dep = make_dependency(pool, build_dependency);

        if (!build_dep) {
            std::cerr << "!!! Invalid build dependency in " << package.name << ": "
                      << build_dependency << '\n';
            return false;
        }

        build_requirements.push_back(build_dep);
    }

    Id id = repo_add_solvable(repo);
    Solvable *solvable = pool_id2solvable(pool, id);

    Id build_key = pool_str2id(pool, "synnical:buildrequires", 1);

    solvable->name = name_id;
    solvable->evr = version_id;
    solvable->arch = arch_id;

    solvable_add_deparray(solvable, SOLVABLE_PROVIDES, name_id, 0);

    solvable_add_deparray(solvable, SOLVABLE_PROVIDES, versioned, 0);

    for (const auto &capability : package.provides) {
        Id name = pool_str2id(pool, capability.c_str(), 1);

        solvable_add_deparray(solvable, SOLVABLE_PROVIDES, name, 0);

        Id versioned_provide = pool_rel2id(pool, name, version_id, REL_EQ, 1);

        solvable_add_deparray(solvable, SOLVABLE_PROVIDES, versioned_provide, 0);
    }

    for (Id dep : requirements) {
        solvable_add_deparray(solvable, SOLVABLE_REQUIRES, dep, 0);
    }

    for (const auto &dependency : package.build_dependencies) {
        repo_add_poolstr_array(repo, id, build_key, dependency.c_str());
    }

    return true;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: synnical-gen-solv <index.toml> <packages.solv>\n";
        return 1;
    }

    try {
        fs::path index_path = argv[1];
        fs::path output_path = argv[2];

        auto index = toml::parse_file(index_path.string());
        auto *packages = index["packages"].as_table();

        if (!packages) {
            std::cerr << "!!! Missing [packages] table\n";
            return 1;
        }

        std::unique_ptr<Pool, decltype(&pool_free)> pool(pool_create(), pool_free);

        if (!pool)
            return 1;

        pool_setarch(pool.get(), "x86_64");

        Repo *repo = repo_create(pool.get(), "synnical-main");

        if (!repo)
            return 1;

        int count = 0;

        for (const auto &[key, value] : *packages) {
            auto *path = value.as_string();

            if (!path)
                throw std::runtime_error("Invalid index entry");

            fs::path recipe_path = index_path.parent_path() / path->get();

            Package package = read_package(recipe_path);

            if (package.name != std::string(key.str()))
                throw std::runtime_error("Recipe name differs from index: " + package.name);

            if (!add_package(pool.get(), repo, package))
                return 1;

            std::cout << "-> " << package.name << " " << package.version << '\n';

            ++count;
        }

        repo_internalize(repo);

        FILE *file = std::fopen(output_path.c_str(), "wb");

        if (!file) {
            std::cerr << "!!! Could not create " << output_path << '\n';
            return 1;
        }

        int result = repo_write(repo, file);
        int close_result = std::fclose(file);

        if (result != 0 || close_result != 0) {
            fs::remove(output_path);
            std::cerr << "!!! Failed to write repository metadata\n";
            return 1;
        }

        std::cout << ">> Generated " << output_path << " (" << count << " packages)\n";

        return 0;

    } catch (const std::exception &error) {
        std::cerr << "!!! " << error.what() << '\n';
        return 1;
    }
}
