cmake_minimum_required(VERSION 3.20)

project(synnical VERSION 1.10 LANGUAGES CXX)

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
pkg_check_modules(INIReader REQUIRED IMPORTED_TARGET INIReader)

add_executable(synnical
    src/main.cxx
    src/repository/recipe.cxx
    src/core/download.cxx
    src/core/sha256.cxx
    src/core/extract.cxx
    src/core/build.cxx
    src/core/install.cxx
    src/core/stage.cxx
    src/system/process.cxx
    src/repository/sync.cxx
    src/core/database.cxx
    src/core/deploy.cxx
    src/core/search.cxx
    src/core/remove.cxx
    src/core/solver.cxx
    src/config/daycare.cxx
    src/config/timezone.cxx
    src/cli/parser.cxx
    src/system/build_output.cxx
    src/config/settings.cxx
    src/build/state.cxx
)

target_include_directories(synnical PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
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
    PkgConfig::INIReader
)

add_executable(solv-generator
    tools/generate_solv.cxx
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
