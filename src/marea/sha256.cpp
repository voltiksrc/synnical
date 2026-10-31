#include <array>
#include <fstream>
#include <sodium.h>
#include <string>

#include "output.hpp"
#include "sha256.hpp"

bool verify_sha256(const std::string &file_path, const std::string &expected_hash) {
    // Open the downloaded file in binary mode.
    std::ifstream file(file_path, std::ios::binary);

    if (!file) {
        output::error("Could not open file for SHA-256 verification.");
        return false;
    }

    // Check the expected checksum before hashing the file.
    if (expected_hash.size() != 64 ||
        expected_hash.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
        output::error("Invalid SHA-256 checksum in recipe.");
        return false;
    }

    crypto_hash_sha256_state state;

    if (crypto_hash_sha256_init(&state) != 0) {
        output::error("Failed to initialize SHA-256.");
        return false;
    }

    // Read the file in 8 KB chunks to avoid loading it all into memory.
    std::array<char, 8192> buffer{};

    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        std::streamsize bytes_read = file.gcount();

        // Pass each chunk to libsodium to update the hash.
        if (crypto_hash_sha256_update(&state,
                                      reinterpret_cast<const unsigned char *>(buffer.data()),
                                      static_cast<unsigned long long>(bytes_read)) != 0) {
            output::error("Failed to update SHA-256.");
            return false;
        }
    }

    if (file.bad()) {
        output::error("Failed to read file during verification.");
        return false;
    }

    // Get the final SHA-256 digest from libsodium.
    std::array<unsigned char, crypto_hash_sha256_BYTES> digest{};

    if (crypto_hash_sha256_final(&state, digest.data()) != 0) {
        output::error("Failed to finalize SHA-256.");
        return false;
    }

    std::array<unsigned char, crypto_hash_sha256_BYTES> expected{};

    if (sodium_hex2bin(expected.data(),
                       expected.size(),
                       expected_hash.c_str(),
                       expected_hash.size(),
                       nullptr,
                       nullptr,
                       nullptr) != 0) {
        output::error("Invalid SHA-256 checksum.");
        return false;
    }

    if (sodium_memcmp(digest.data(), expected.data(), digest.size()) != 0) {
        output::error("SHA-256 verification failed.");
        return false;
    }

    output::info("SHA-256 verified successfully.");

    std::array<char, crypto_hash_sha256_BYTES * 2 + 1> hex{};

    sodium_bin2hex(hex.data(), hex.size(), digest.data(), digest.size());

    output::stage(std::string("HASH DOES = ") + hex.data());
    return true;
}
