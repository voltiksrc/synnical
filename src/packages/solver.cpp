#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// libsolv's headers use "requires", which is a C++20 keyword.
#define requires requires_deps
#include <solv/pool.h>
#include <solv/poolarch.h>
#include <solv/queue.h>
#include <solv/repo.h>
#include <solv/repo_solv.h>
#include <solv/solvable.h>
#include <solv/solver.h>
#include <solv/transaction.h>
#undef requires

#include "database.hpp"
#include "output.hpp"
#include "solver.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char *metadata_path = ".cache/repos/main/packages.solv";
constexpr const char *database_path = "/tmp/synnical-test-root/var/lib/synnical/installed.db";
constexpr const char *build_key_name = "synnical:buildrequires";

// A libsolv Queue needs queue_free, even when we return early.
struct SolvQueue {
    Queue value;
    SolvQueue() { queue_init(&value); }
    ~SolvQueue() { queue_free(&value); }
    SolvQueue(const SolvQueue &) = delete;
    SolvQueue &operator=(const SolvQueue &) = delete;
};

std::string name_of(Pool *pool, Id id) {
    Solvable *pkg = pool_id2solvable(pool, id);
    return pkg ? pool_id2str(pool, pkg->name) : "<invalid>";
}

bool add_installed_package(Pool *pool, Repo *repo, const InstalledPackage &package) {
    if (package.name.empty() || package.version.empty())
        return false;

    Id name = pool_str2id(pool, package.name.c_str(), 1);
    Id version = pool_str2id(pool, package.version.c_str(), 1);
    Id arch = pool_str2id(pool, "x86_64", 1);
    Id exact = pool_rel2id(pool, name, version, REL_EQ, 1);

    Id id = repo_add_solvable(repo);
    Solvable *pkg = pool_id2solvable(pool, id);
    if (!pkg)
        return false;

    pkg->name = name;
    pkg->evr = version;
    pkg->arch = arch;
    solvable_add_deparray(pkg, SOLVABLE_PROVIDES, name, 0);
    solvable_add_deparray(pkg, SOLVABLE_PROVIDES, exact, 0);
    return true;
}

bool load_available_repo(Repo *repo, const fs::path &path) {
    FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) {
        output::error("Could not open packages.solv.");
        return false;
    }

    int result = repo_add_solv(repo, file, 0);
    std::fclose(file);
    if (result != 0) {
        output::error("Failed to load packages.solv.");
        return false;
    }
    repo_internalize(repo);
    return true;
}

bool load_installed_repo(Pool *pool, Repo *repo,
                         std::map<std::string, std::string> &versions) {
    if (!fs::exists(database_path)) {
        repo_internalize(repo);
        return true;
    }
    if (!fs::is_regular_file(database_path)) {
        output::error("Installed database is not a regular file.");
        return false;
    }

    Database db(database_path);
    if (!db.initialize())
        return false;

    std::vector<InstalledPackage> packages;
    if (!db.get_installed_packages(packages))
        return false;

    for (const auto &package : packages) {
        if (!add_installed_package(pool, repo, package)) {
            output::error("Invalid installed package: " + package.name);
            return false;
        }
        versions[package.name] = package.version;
    }
    repo_internalize(repo);
    return true;
}

std::vector<std::string> get_build_dependencies(Pool *pool, Repo *repo, Id id) {
    std::vector<std::string> result;
    Id key = pool_str2id(pool, build_key_name, 0);
    if (!key)
        return result;

    SolvQueue entries;
    repo_lookup_idarray(repo, id, key, &entries.value);
    for (int i = 0; i < entries.value.count; ++i)
        result.emplace_back(pool_id2str(pool, entries.value.elements[i]));
    return result;
}

// Accept "cargo" and "cmake >= 3.20", using the same format as our recipes.
// Do not create fresh IDs after pool_createwhatprovides(): the metadata
// should already contain every capability/version we're asking for.
Id parse_requirement(Pool *pool, const std::string &text) {
    std::istringstream input(text);
    std::string name, op, version, extra;
    if (!(input >> name))
        return 0;

    Id name_id = pool_str2id(pool, name.c_str(), 0);
    if (!name_id)
        return 0;
    if (!(input >> op))
        return name_id;
    if (!(input >> version) || (input >> extra))
        return 0;

    int flags = 0;
    if (op == ">=") flags = REL_GT | REL_EQ;
    else if (op == "<=") flags = REL_LT | REL_EQ;
    else if (op == ">") flags = REL_GT;
    else if (op == "<") flags = REL_LT;
    else if (op == "=" || op == "==") flags = REL_EQ;
    else return 0;

    Id version_id = pool_str2id(pool, version.c_str(), 0);
    if (!version_id)
        return 0;
    return pool_rel2id(pool, name_id, version_id, flags, 1);
}

// Find the actual selected package for a capability. For example, "cargo"
// can point to the selected "rust-toolchain" package.
Id selected_provider(Pool *pool, Id requirement, const std::set<Id> &selected) {
    Id provider, pos;
    FOR_PROVIDES(provider, pos, requirement) {
        if (selected.contains(provider))
            return provider;
    }
    return 0;
}

bool report_solver_problems(Solver *solver) {
    output::error("Could not resolve dependencies.");
    for (Id problem = solver_next_problem(solver, 0); problem;
         problem = solver_next_problem(solver, problem))
        output::error(solver_problem2str(solver, problem));
    return false;
}

} // namespace

bool solve_install_plan(const std::vector<std::string> &requested,
                        std::vector<PlannedPackage> &plan) {
    plan.clear();
    if (!fs::is_regular_file(metadata_path)) {
        output::error("Repository metadata missing. Run synnical --sync first.");
        return false;
    }
    if (requested.empty()) {
        output::error("No packages requested.");
        return false;
    }

    try {
        output::stage("Resolving dependencies...");
        std::unique_ptr<Pool, decltype(&pool_free)> pool(pool_create(), pool_free);
        if (!pool) {
            output::error("Failed to create solver pool.");
            return false;
        }
        pool_setarch(pool.get(), "x86_64");

        Repo *available = repo_create(pool.get(), "synnical-main");
        Repo *installed = repo_create(pool.get(), "installed");
        if (!available || !installed) {
            output::error("Failed to create repositories.");
            return false;
        }
        if (!load_available_repo(available, metadata_path))
            return false;

        std::map<std::string, std::string> installed_versions;
        if (!load_installed_repo(pool.get(), installed, installed_versions))
            return false;

        pool_set_installed(pool.get(), installed);
        pool_createwhatprovides(pool.get());

        SolvQueue jobs;
        for (const auto &name : requested) {
            Id id = pool_str2id(pool.get(), name.c_str(), 0);
            if (!id) {
                output::error("Unknown package or capability: " + name);
                return false;
            }
            queue_push2(&jobs.value, SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES, id);
        }

        std::set<std::string> added_build_jobs;
        std::unique_ptr<Solver, decltype(&solver_free)> solver(nullptr, solver_free);
        std::unique_ptr<Transaction, decltype(&transaction_free)> transaction(nullptr,
                                                                               transaction_free);

        // Keep solving until the new build tools stop bringing in more tools.
        while (true) {
            transaction.reset(); // This belongs to the previous solver.
            solver.reset(solver_create(pool.get()));
            if (!solver) {
                output::error("Failed to create solver.");
                return false;
            }
            if (solver_solve(solver.get(), &jobs.value) != 0)
                return report_solver_problems(solver.get());

            transaction.reset(solver_create_transaction(solver.get()));
            if (!transaction) {
                output::error("Failed to create transaction.");
                return false;
            }

            bool changed = false;
            for (int i = 0; i < transaction->steps.count; ++i) {
                Id id = transaction->steps.elements[i];
                Solvable *pkg = pool_id2solvable(pool.get(), id);
                if (!pkg || pkg->repo != available)
                    continue;

                for (const auto &text : get_build_dependencies(pool.get(), available, id)) {
                    if (added_build_jobs.contains(text))
                        continue;
                    Id dep = parse_requirement(pool.get(), text);
                    if (!dep) {
                        output::error("Invalid or unknown build requirement: " + text);
                        return false;
                    }
                    queue_push2(&jobs.value, SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES, dep);
                    added_build_jobs.insert(text);
                    changed = true;
                }
            }
            if (!changed)
                break;
        }

        // Build a set of exactly what the solver chose, including installed packages.
        // The installed repo matters here: build tools we already have need no rebuild.
        std::set<Id> selected;
        for (Id id = installed->start; id < installed->end; ++id) {
            Solvable *pkg = pool_id2solvable(pool.get(), id);
            if (pkg && pkg->repo == installed)
                selected.insert(id);
        }

        std::set<Id> pending;
        for (int i = 0; i < transaction->steps.count; ++i) {
            Id id = transaction->steps.elements[i];
            Solvable *pkg = pool_id2solvable(pool.get(), id);
            if (!pkg || pkg->repo != available) {
                output::error("Unexpected package in installation transaction.");
                return false;
            }
            if (transaction_type(transaction.get(), id, SOLVER_TRANSACTION_SHOW_ACTIVE) !=
                SOLVER_TRANSACTION_INSTALL) {
                output::error("Upgrades and removals aren't supported yet.");
                return false;
            }
            if (installed_versions.contains(pool_id2str(pool.get(), pkg->name))) {
                output::error("Package already installed with another version: " + name_of(pool.get(), id));
                return false;
            }
            pending.insert(id);
            selected.insert(id);
        }

        // Both runtime requires and custom build_requires become ordering edges.
        // libsolv chooses packages; this part decides which source to build first.
        std::map<Id, std::set<Id>> prerequisites;
        std::map<std::string, std::vector<std::string>> build_deps_for_output;

        for (Id id : pending) {
            Solvable *pkg = pool_id2solvable(pool.get(), id);
            const std::string name = name_of(pool.get(), id);

            SolvQueue runtime;
            solvable_lookup_deparray(pkg, SOLVABLE_REQUIRES, &runtime.value, -1);
            for (int i = 0; i < runtime.value.count; ++i) {
                Id provider = selected_provider(pool.get(), runtime.value.elements[i], selected);
                if (provider && provider != id && pending.contains(provider))
                    prerequisites[id].insert(provider);
            }

            auto deps = get_build_dependencies(pool.get(), available, id);
            if (!deps.empty())
                build_deps_for_output[name] = deps;

            for (const auto &text : deps) {
                Id requirement = parse_requirement(pool.get(), text);
                Id provider = requirement ? selected_provider(pool.get(), requirement, selected) : 0;
                if (!provider) {
                    output::error("No selected provider for " + name + " build requirement: " + text);
                    return false;
                }
                if (provider == id) {
                    output::error("Package needs itself to build: " + name + " -> " + text);
                    return false;
                }
                if (pending.contains(provider))
                    prerequisites[id].insert(provider);
            }
        }

        std::map<Id, int> state; // 0: unseen, 1: visiting, 2: done
        std::vector<Id> ordered;
        std::function<bool(Id)> visit = [&](Id id) -> bool {
            if (state[id] == 2)
                return true;
            if (state[id] == 1) {
                output::error("Dependency cycle involving: " + name_of(pool.get(), id));
                return false;
            }
            state[id] = 1;
            for (Id dep : prerequisites[id]) {
                if (!visit(dep))
                    return false;
            }
            state[id] = 2;
            ordered.push_back(id);
            return true;
        };

        // Stable alphabetical starting order; dependency edges override it.
        std::vector<Id> roots(pending.begin(), pending.end());
        std::sort(roots.begin(), roots.end(), [&](Id a, Id b) {
            return name_of(pool.get(), a) < name_of(pool.get(), b);
        });
        for (Id id : roots) {
            if (!visit(id))
                return false;
        }

        if (!build_deps_for_output.empty()) {
            output::stage("Build dependencies");
            for (const auto &[name, deps] : build_deps_for_output) {
                // One entry per package, not spammed once per solver pass.
                std::string line = name + ": ";
                for (std::size_t i = 0; i < deps.size(); ++i) {
                    if (i) line += ", ";
                    line += deps[i];
                }
                output::info(line);
            }
        }

        if (ordered.empty()) {
            output::info("All requested packages are already satisfied.");
            return true;
        }

        output::stage("Installation plan");
        for (Id id : ordered) {
            Solvable *pkg = pool_id2solvable(pool.get(), id);
            plan.push_back({pool_id2str(pool.get(), pkg->name),
                            pool_id2str(pool.get(), pkg->evr)});
            output::info(plan.back().name + " " + plan.back().version);
        }
        return true;
    } catch (const std::exception &error) {
        output::error(error.what());
        plan.clear();
        return false;
    }
}

bool resolve_package(const std::string &name) {
    std::vector<PlannedPackage> plan;
    return solve_install_plan({name}, plan);
}
