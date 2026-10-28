#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <toml++/toml.hpp>

#include "recipe.hpp"

using std::string;

namespace fs = std::filesystem;

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;

    auto *file = static_cast<std::ofstream *>(userdata);

    file->write(ptr, total);

    if (!*file) {
        return 0;
    }

    return total;
}

bool download_recipe(const std::string &url, const std::filesystem::path &destination) {
    fs::create_directories(destination.parent_path());

    CURL *curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize libcurl.\n";
        return false;
    }

    fs::path output_path = destination.string() + ".tmp";
    fs::path final_name = destination;

    std::ofstream file(output_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open output file.\n";
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    // Require HTTPS and verify TLS certificates.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");

    CURLcode result = curl_easy_perform(curl);
    std::cout << '\n';

    file.close();
    if (result != CURLE_OK) {
        std::cerr << "Download failed: " << curl_easy_strerror(result) << '\n';
        fs::remove(output_path);
        curl_easy_cleanup(curl);
        return false;
    }
    if (file.fail()) {
        std::cerr << "Failed to write all the data to file.\n";
        fs::remove(output_path);
        curl_easy_cleanup(curl);
        return false;
    }
    std::error_code ec;
    fs::rename(output_path, final_name, ec);
    if (ec) {
        std::cerr << "Failed to rename recipe: " << ec.message() << '\n';
        fs::remove(output_path);
        curl_easy_cleanup(curl);
        return false;
    }
    curl_easy_cleanup(curl);
    return true;
}

Package read_recipe(const std::string &path) {
    // Read and parse the recipe.
    auto recipe = toml::parse_file(path);

    Package package;

    package.name = recipe["package"]["name"].value_or("");
    package.version = recipe["package"]["version"].value_or("");
    package.url = recipe["source"]["url"].value_or("");
    package.sha256 = recipe["source"]["sha256"].value_or("");
    package.build_system = recipe["build"]["system"].value_or("");

    package.architecture = recipe["package"]["architecture"].value_or("x86_64");

    package.dependencies.clear();

    if (auto *deps = recipe["deps"]["requires"].as_array()) {
        for (const auto &dep : *deps) {
            auto value = dep.value<std::string>();

            if (!value || value->empty())
                continue;

            package.dependencies.push_back(*value);
        }
    }

    return package;
}
