#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

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

bool add_installed_package(
    Pool *pool,
    Repo *repo,
    const InstalledPackage &package
) {
    if (package.name.empty() || package.version.empty())
        return false;

    Id name = pool_str2id(pool, package.name.c_str(), 1);
    Id version = pool_str2id(pool, package.version.c_str(), 1);
    Id arch = pool_str2id(pool, "x86_64", 1);

    Id versioned = pool_rel2id(
        pool, name, version, REL_EQ, 1
    );

    Id id = repo_add_solvable(repo);

    Solvable *solvable = pool_id2solvable(pool, id);

    solvable->name = name;
    solvable->evr = version;
    solvable->arch = arch;

    solvable_add_deparray(
        solvable, SOLVABLE_PROVIDES, name, 0
    );

    solvable_add_deparray(
        solvable, SOLVABLE_PROVIDES, versioned, 0
    );

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

bool load_installed_repo(
    Pool *pool,
    Repo *repo,
    std::map<std::string, std::string> &installed_versions
) {
    const fs::path db_path =
        "/tmp/synnical-test-root/var/lib/synnical/installed.db";

    if (!fs::exists(db_path)) {
        repo_internalize(repo);
        return true;
    }

    if (!fs::is_regular_file(db_path)) {
        output::error("Installed database is not a regular file.");
        return false;
    }

    Database db(db_path);

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

        installed_versions[package.name] = package.version;
    }

    repo_internalize(repo);
    return true;
}

} // namespace

bool solve_install_plan(
    const std::vector<std::string> &requested,
    std::vector<PlannedPackage> &plan
) {
    plan.clear();

    const fs::path solv_path =
        ".cache/repos/main/packages.solv";

    if (!fs::is_regular_file(solv_path)) {
        output::error("Repository metadata missing. Run synnical --sync first.");
        return false;
    }

    if (requested.empty()) {
        output::error("No packages requested.");
        return false;
    }

    try {
        output::stage("Resolving dependencies...");

        std::unique_ptr<Pool, decltype(&pool_free)> pool(
            pool_create(), pool_free
        );

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

        if (!load_available_repo(available, solv_path))
            return false;

        std::map<std::string, std::string> installed_versions;

        if (!load_installed_repo(
                pool.get(), installed, installed_versions
            )) {
            return false;
        }

        pool_set_installed(pool.get(), installed);
        pool_createwhatprovides(pool.get());

        Queue jobs;
        queue_init(&jobs);

        for (const auto &name : requested) {
            Id package_name = pool_str2id(
                pool.get(), name.c_str(), 1
            );

            queue_push2(
                &jobs,
                SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                package_name
            );
        }

        std::unique_ptr<Solver, decltype(&solver_free)> solver(
            solver_create(pool.get()), solver_free
        );

        if (!solver) {
            queue_free(&jobs);
            output::error("Failed to create solver.");
            return false;
        }

        int problems = solver_solve(solver.get(), &jobs);
        queue_free(&jobs);

        if (problems != 0) {
            output::error("Could not resolve dependencies.");

            for (Id problem = solver_next_problem(solver.get(), 0);
                 problem != 0;
                 problem = solver_next_problem(solver.get(), problem)) {

                output::error(
                    solver_problem2str(solver.get(), problem)
                );
            }

            return false;
        }

        std::unique_ptr<Transaction, decltype(&transaction_free)> transaction(
            solver_create_transaction(solver.get()),
            transaction_free
        );

        if (!transaction) {
            output::error("Failed to create transaction.");
            return false;
        }

        // Put dependencies before the packages that need them.
        transaction_order(transaction.get(), 0);

        for (int i = 0; i < transaction->steps.count; ++i) {
            Id id = transaction->steps.elements[i];

            Solvable *pkg = pool_id2solvable(pool.get(), id);

            if (!pkg) {
                output::error("Invalid transaction package.");
                return false;
            }

            // Reject removals and unsupported transaction types.
            int type = transaction_type(
                transaction.get(), id, SOLVER_TRANSACTION_SHOW_ACTIVE
            );

            if (type != SOLVER_TRANSACTION_INSTALL) {
                output::error(
                    "Transaction requires an upgrade, removal, "
                    "or other unsupported operation."
                );
                return false;
            }

            // New installations must come from the available repo.
            if (pkg->repo != available) {
                output::error("Unexpected package repository.");
                return false;
            }

            std::string name = pool_id2str(
                pool.get(), pkg->name
            );

            std::string version = pool_id2str(
                pool.get(), pkg->evr
            );

            // Do not overwrite an already-installed package.
            if (installed_versions.contains(name)) {
                output::error(
                    "Package already installed with another version: " +
                    name
                );
                return false;
            }

            plan.push_back({name, version});
        }

        if (plan.empty()) {
            output::info("All requested packages are already satisfied.");
            return true;
        }

        output::stage("Installation plan");

        for (const auto &package : plan) {
            output::info(package.name + " " + package.version);
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
