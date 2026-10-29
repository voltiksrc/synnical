#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>

#include <curl/curl.h>
#include <toml++/toml.hpp>

#define requires requires_deps
#include <solv/pool.h>
#include <solv/repo.h>
#include <solv/repo_solv.h>
#undef requires

#include "general/output.hpp"
#include "repository/sync.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char *index_url =
    "https://raw.githubusercontent.com/voltiksrc/synnical-repo/main/index.toml";

constexpr const char *solv_url =
    "https://raw.githubusercontent.com/voltiksrc/synnical-repo/main/packages.solv";

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    auto *file = static_cast<std::ofstream *>(userdata);

    file->write(ptr, static_cast<std::streamsize>(total));

    return *file ? total : 0;
}

bool download_file(const std::string &url, const fs::path &destination) {
    std::ofstream file(destination, std::ios::binary | std::ios::trunc);

    if (!file) {
        output::error("Could not open " + destination.string());
        return false;
    }

    CURL *curl = curl_easy_init();

    if (!curl) {
        output::error("Failed to initialize libcurl.");
        file.close();
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode result = curl_easy_perform(curl);

    bool failed = !file;
    file.close();

    if (file.fail())
        failed = true;

    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        output::error("Download failed: " +
                      std::string(curl_easy_strerror(result)));
        return false;
    }

    if (failed) {
        output::error("Failed to write " + destination.string());
        return false;
    }

    return true;
}

bool validate_index(const fs::path &path) {
    try {
        auto index = toml::parse_file(path.string());

        if (!index["repository"]["indexurl"].is_string() ||
            !index["packages"].is_table()) {
            output::error("Repository index is missing required fields.");
            return false;
        }

        return true;

    } catch (const std::exception &error) {
        output::error("Invalid index: " + std::string(error.what()));
        return false;
    }
}

bool validate_solv(const fs::path &path) {
    std::unique_ptr<Pool, decltype(&pool_free)> pool(
        pool_create(), pool_free
    );

    if (!pool)
        return false;

    Repo *repo = repo_create(pool.get(), "validation");

    if (!repo)
        return false;

    FILE *file = std::fopen(path.c_str(), "rb");

    if (!file) {
        output::error("Could not open downloaded packages.solv.");
        return false;
    }

    int result = repo_add_solv(repo, file, 0);
    std::fclose(file);

    if (result != 0) {
        output::error("Downloaded packages.solv is invalid.");
        return false;
    }

    return true;
}

bool move_file(const fs::path &from, const fs::path &to) {
    std::error_code ec;
    fs::rename(from, to, ec);

    if (ec) {
        output::error("Could not move " + from.string() +
                      ": " + ec.message());
        return false;
    }

    return true;
}

} // namespace

bool sync_repositories() {
    const fs::path repo_dir = ".cache/repos/main";

    const fs::path index_path = repo_dir / "index.toml";
    const fs::path solv_path = repo_dir / "packages.solv";

    const fs::path index_tmp = repo_dir / "index.toml.tmp";
    const fs::path solv_tmp = repo_dir / "packages.solv.tmp";

    const fs::path index_backup = repo_dir / "index.toml.bak";
    const fs::path solv_backup = repo_dir / "packages.solv.bak";

    std::error_code ec;
    fs::create_directories(repo_dir, ec);

    if (ec) {
        output::error("Failed to create repository directory: " +
                      ec.message());
        return false;
    }

    output::stage("Syncing repositories...");

    // Remove leftovers from previous failed downloads.
    fs::remove(index_tmp, ec);
    fs::remove(solv_tmp, ec);

    output::info("Downloading index.toml");

    if (!download_file(index_url, index_tmp)) {
        fs::remove(index_tmp, ec);
        return false;
    }

    output::info("Downloading packages.solv");

    if (!download_file(solv_url, solv_tmp)) {
        fs::remove(index_tmp, ec);
        fs::remove(solv_tmp, ec);
        return false;
    }

    output::stage("Validating repository metadata");

    if (!validate_index(index_tmp) || !validate_solv(solv_tmp)) {
        fs::remove(index_tmp, ec);
        fs::remove(solv_tmp, ec);
        return false;
    }

    // Preserve the old metadata in case replacement fails.
    fs::remove(index_backup, ec);
    fs::remove(solv_backup, ec);

    bool had_index = fs::exists(index_path);
    bool had_solv = fs::exists(solv_path);

    if (had_index && !move_file(index_path, index_backup))
        return false;

    if (had_solv && !move_file(solv_path, solv_backup)) {
        if (had_index)
            move_file(index_backup, index_path);
        return false;
    }

    // Install both validated metadata files.
    bool index_updated = move_file(index_tmp, index_path);
    bool solv_updated = index_updated && move_file(solv_tmp, solv_path);

    if (!index_updated || !solv_updated) {
        output::error("Metadata update failed. Restoring previous files.");

        fs::remove(index_path, ec);
        fs::remove(solv_path, ec);

        if (had_index)
            move_file(index_backup, index_path);

        if (had_solv)
            move_file(solv_backup, solv_path);

        fs::remove(index_tmp, ec);
        fs::remove(solv_tmp, ec);

        return false;
    }

    fs::remove(index_backup, ec);
    fs::remove(solv_backup, ec);

    output::info("Repositories synced!");
    return true;
}
