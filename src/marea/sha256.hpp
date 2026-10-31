#pragma once

#include <string>

bool verify_sha256(const std::string& file_path,
                   const std::string& expected_hash);
