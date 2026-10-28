#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "database.hpp"
#include "output.hpp"

Database::Database(const std::filesystem::path &path) {
    int result = sqlite3_open(path.c_str(), &db_);

    if (result != SQLITE_OK) {
        std::string error = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;

        throw std::runtime_error("Failed to open database: " + error);
    }
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool Database::initialize() {
    // Make sure files cannot reference a package that doesn't exist
    if (sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        output::error("Failed to enable foreign keys: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    // Store installed packages and which files belong to them
    const char *sql = R"(
        CREATE TABLE IF NOT EXISTS packages (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL UNIQUE,
            version TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS files (
            path TEXT PRIMARY KEY,
            package_id INTEGER NOT NULL,
            FOREIGN KEY (package_id) REFERENCES packages(id)
        );
    )";

    if (sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
        output::error("Failed to initialize database: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    return true;
}

InstallStatus Database::is_installed(const std::string &name) {
    const char *sql = "SELECT 1 FROM packages WHERE name = ? LIMIT 1;";
    sqlite3_stmt *stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        output::error("Failed to prepare package query: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return InstallStatus::Error;
    }

    // Bind the package name to the query
    if (sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        output::error("Failed to bind package name: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return InstallStatus::Error;
    }

    int result = sqlite3_step(stmt);

    if (result != SQLITE_ROW && result != SQLITE_DONE) {
        output::error("Failed to query installed packages: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return InstallStatus::Error;
    }

    sqlite3_finalize(stmt);

    if (result == SQLITE_ROW) {
        return InstallStatus::Installed;
    }

    return InstallStatus::NotInstalled;
}

bool Database::record_installation(const std::string &name, const std::string &version,
                                   const std::vector<std::filesystem::path> &files) {
    // Start a transaction so an error won't leave half the package in the database
    if (sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        output::error("Failed to begin transaction: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    const char *package_sql = "INSERT INTO packages (name, version) VALUES (?, ?);";
    sqlite3_stmt *stmt = nullptr;

    if (sqlite3_prepare_v2(db_, package_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        output::error("Failed to prepare package insertion: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    // Insert the package itself before recording its files
    if (sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, version.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {

        output::error("Failed to insert package: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    sqlite3_int64 package_id = sqlite3_last_insert_rowid(db_);
    sqlite3_finalize(stmt);

    const char *file_sql = "INSERT INTO files (path, package_id) VALUES (?, ?);";
    sqlite3_stmt *file_stmt = nullptr;

    if (sqlite3_prepare_v2(db_, file_sql, -1, &file_stmt, nullptr) != SQLITE_OK) {
        output::error("Failed to prepare file insertion: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(file_stmt);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    // Reuse the same statement for every file owned by this package
    for (const auto &file : files) {
        std::string path = file.generic_string();

        if (sqlite3_reset(file_stmt) != SQLITE_OK ||
            sqlite3_clear_bindings(file_stmt) != SQLITE_OK ||
            sqlite3_bind_text(file_stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_int64(file_stmt, 2, package_id) != SQLITE_OK ||
            sqlite3_step(file_stmt) != SQLITE_DONE) {

            output::error("Failed to record file " + path + ": " + std::string(sqlite3_errmsg(db_)));
            sqlite3_finalize(file_stmt);
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
    }

    sqlite3_finalize(file_stmt);

    // Only save the installation once every file has been recorded
    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        output::error("Failed to commit installation: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    return true;
}

bool Database::list_installed() {
    const char *sql = "SELECT name, version FROM packages ORDER BY name;";
    sqlite3_stmt *stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        output::error("Failed to query installed packages: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    output::stage("Installed packages");

    int result;

    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char *name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        const char *version = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));

        output::info(std::string(name ? name : "") + " " + (version ? version : ""));
    }

    if (result != SQLITE_DONE) {
        output::error("Failed to read installed packages: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}


InstallStatus Database::get_installed_files(
    const std::string &name,
    std::vector<std::filesystem::path> &files
) {
    files.clear();

    InstallStatus status = is_installed(name);

    if (status != InstallStatus::Installed) {
        return status;
    }

    const char *sql = R"(
        SELECT files.path
        FROM files
        JOIN packages ON files.package_id = packages.id
        WHERE packages.name = ?;
    )";

    sqlite3_stmt *stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        output::error("Failed to prepare file query: " + std::string(sqlite3_errmsg(db_)));
        return InstallStatus::Error;
    }

    if (sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        output::error("Failed to bind package name: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return InstallStatus::Error;
    }

    // Collect the paths owned by this package
    int result;

    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char *path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));

        if (!path) {
            sqlite3_finalize(stmt);
            files.clear();
            return InstallStatus::Error;
        }

        files.emplace_back(path);
    }

    if (result != SQLITE_DONE) {
        output::error("Failed to read package files: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        files.clear();
        return InstallStatus::Error;
    }

    sqlite3_finalize(stmt);
    return InstallStatus::Installed;
}

bool Database::remove_installation(const std::string &name) {
    if (sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        output::error("Failed to begin removal transaction: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    // Delete the file records before deleting the package
    const char *queries[] = {
        "DELETE FROM files WHERE package_id = (SELECT id FROM packages WHERE name = ?);",
        "DELETE FROM packages WHERE name = ?;"
    };

    for (const char *sql : queries) {
        sqlite3_stmt *stmt = nullptr;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            output::error("Failed to prepare removal: " + std::string(sqlite3_errmsg(db_)));
            sqlite3_finalize(stmt);
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        if (sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_step(stmt) != SQLITE_DONE) {

            output::error("Failed to remove database records: " + std::string(sqlite3_errmsg(db_)));
            sqlite3_finalize(stmt);
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        sqlite3_finalize(stmt);
    }

    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        output::error("Failed to commit removal: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    return true;
}


bool Database::get_installed_packages(
    std::vector<InstalledPackage> &packages
) {
    packages.clear();

    const char *sql = "SELECT name, version FROM packages ORDER BY name;";

    sqlite3_stmt *stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        output::error("Failed to prepare installed package query: " +
                      std::string(sqlite3_errmsg(db_)));
        return false;
    }

    int result;

    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char *name = reinterpret_cast<const char *>(
            sqlite3_column_text(stmt, 0)
        );

        const char *version = reinterpret_cast<const char *>(
            sqlite3_column_text(stmt, 1)
        );

        if (!name || !version) {
            output::error("Invalid installed package record.");
            sqlite3_finalize(stmt);
            packages.clear();
            return false;
        }

        packages.push_back({name, version});
    }

    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        output::error("Failed to read installed packages.");
        packages.clear();
        return false;
    }

    return true;
}
