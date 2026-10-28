
#include "sha256.hpp"
#include "output.hpp"

#include <openssl/evp.h>

#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

bool verify_sha256(const std::string &file_path,
                   const std::string &expected_hash) {
    // Open the downloaded file in binary mode.
    std::ifstream file(file_path, std::ios::binary);

    if (!file) {
        output::error("Could not open file for SHA-256 verification.");
        return false;
    }

    // Check the expected checksum before hashing the file.
    if (expected_hash.size() != 64 ||
        expected_hash.find_first_not_of("0123456789abcdefABCDEF") !=
            std::string::npos) {
        output::error("Invalid SHA-256 checksum in recipe.");
        return false;
    }

    // Create the OpenSSL hashing context.
    // unique_ptr automatically frees it when the function returns.
    using HashContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

    HashContext ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);

    if (!ctx) {
        output::error("Failed to create SHA-256 context.");
        return false;
    }

    // Initialize the context to use SHA-256.
    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        output::error("Failed to initialize SHA-256.");
        return false;
    }

    // Read the file in 8 KB chunks to avoid loading it all into memory.
    std::array<char, 8192> buffer{};

    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        std::streamsize bytes_read = file.gcount();

        // Pass each chunk to OpenSSL to update the hash.
        if (EVP_DigestUpdate(ctx.get(), buffer.data(),
                             static_cast<std::size_t>(bytes_read)) != 1) {
            output::error("Failed to update SHA-256.");
            return false;
        }
    }

    if (file.bad()) {
        output::error("Failed to read file during verification.");
        return false;
    }

    // Get the final SHA-256 digest from OpenSSL.
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_length = 0;

    if (EVP_DigestFinal_ex(ctx.get(), digest.data(), &digest_length) != 1) {
        output::error("Failed to finalize SHA-256.");
        return false;
    }

    if (digest_length != 32) {
        output::error("Unexpected SHA-256 digest length.");
        return false;
    }

    // Convert the raw digest into a readable hexadecimal string.
    std::ostringstream result;

    for (unsigned int i = 0; i < digest_length; ++i) {
        result << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<unsigned int>(digest[i]);
    }

    std::string actual_hash = result.str();
    std::string normalized_hash = expected_hash;

    // Convert uppercase checksums to lowercase before comparing.
    for (char &c : normalized_hash) {
        c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
    }

    // Stop installation if the downloaded file doesn't match the recipe.
    if (actual_hash != normalized_hash) {
        output::error("SHA-256 verification failed.");
        output::info("Expected: " + normalized_hash);
        output::info("Actual:   " + actual_hash);
        return false;
    }

    output::info("SHA-256 verified successfully.");
    output::stage("HASH DOES = " + actual_hash);
    return true;
}
