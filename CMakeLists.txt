cmake_minimum_required(VERSION 3.20)

project(synical VERSION 0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(ARCHIVE REQUIRED IMPORTED_TARGET libarchive)
pkg_check_modules(TOMLPLUSPLUS REQUIRED IMPORTED_TARGET tomlplusplus)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
pkg_check_modules(LIBSOLV REQUIRED IMPORTED_TARGET libsolv)
pkg_check_modules(LIBSOLVEXT REQUIRED IMPORTED_TARGET libsolvext)

add_executable(synnical
    src/main.cpp
    src/repository/recipe.cpp
    src/packages/download.cpp
    src/packages/sha256.cpp
    src/packages/extract.cpp
    src/packages/build.cpp
    src/packages/install.cpp
    src/packages/stage.cpp
    src/system/process.cpp
    src/repository/sync.cpp
    src/packages/database.cpp
    src/packages/deploy.cpp
    src/packages/search.cpp
    src/packages/remove.cpp
    src/packages/solver.cpp
    src/config/daycare.cpp
)

target_include_directories(synnical PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/packages
    ${CMAKE_CURRENT_SOURCE_DIR}/src/repository
    ${CMAKE_CURRENT_SOURCE_DIR}/src/general
    ${CMAKE_CURRENT_SOURCE_DIR}/src/system
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config
)

target_link_libraries(synnical PRIVATE
    PkgConfig::CURL
    PkgConfig::ARCHIVE
    PkgConfig::TOMLPLUSPLUS
    PkgConfig::SQLITE3
    PkgConfig::LIBSOLV
    PkgConfig::LIBSOLVEXT
    OpenSSL::Crypto
)

add_executable(solv-generator
    tools/generate_solv.cpp
)

target_link_libraries(solv-generator PRIVATE
    PkgConfig::LIBSOLV
    PkgConfig::TOMLPLUSPLUS
)

install(TARGETS synnical RUNTIME DESTINATION bin)
