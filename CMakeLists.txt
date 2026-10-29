cmake_minimum_required(VERSION 3.20)

project(synical VERSION 0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(ARCHIVE REQUIRED IMPORTED_TARGET libarchive)
pkg_check_modules(TOMLPLUSPLUS REQUIRED IMPORTED_TARGET tomlplusplus)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
pkg_check_modules(LIBSOLV REQUIRED IMPORTED_TARGET libsolv)
pkg_check_modules(LIBSOLVEXT REQUIRED IMPORTED_TARGET libsolvext)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

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
    src/config/timezone.cpp
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
    PkgConfig::SODIUM
)

add_executable(solv-generator
    tools/generate_solv.cpp
)

add_custom_target(generate-solv
    COMMAND $<TARGET_FILE:solv-generator>
        "${CMAKE_SOURCE_DIR}/../synnical-repo/index.toml"
        "${CMAKE_SOURCE_DIR}/../synnical-repo/packages.solv"

    DEPENDS solv-generator
    COMMENT "Generating libsolv repository metadata"
    VERBATIM
)

target_link_libraries(solv-generator PRIVATE
    PkgConfig::LIBSOLV
    PkgConfig::TOMLPLUSPLUS
)

install(TARGETS synnical RUNTIME DESTINATION bin)
