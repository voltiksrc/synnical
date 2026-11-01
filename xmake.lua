set_project("synnical")
set_version("1.10")
set_languages("c++23")

add_rules("mode.debug", "mode.release")

-- Use existing system libraries
add_requires("pkgconfig::libcurl")
add_requires("pkgconfig::libarchive")
add_requires("pkgconfig::tomlplusplus")
add_requires("pkgconfig::sqlite3")
add_requires("pkgconfig::libsolv")
add_requires("pkgconfig::libsolvext")
add_requires("pkgconfig::libsodium")
add_requires("pkgconfig::INIReader")

target("synnical")
set_kind("binary")

add_files(
	"src/main.cxx",
	"src/repository/*.cxx",
	"src/core/*.cxx",
	"src/system/*.cxx",
	"src/config/*.cxx",
	"src/cli/*.cxx",
	"src/build/state.cxx"
)

add_includedirs("src", "src/core", "src/repository", "src/general", "src/system", "src/config")

add_packages(
	"pkgconfig::libcurl",
	"pkgconfig::libarchive",
	"pkgconfig::tomlplusplus",
	"pkgconfig::sqlite3",
	"pkgconfig::libsolv",
	"pkgconfig::libsolvext",
	"pkgconfig::libsodium",
	"pkgconfig::INIReader"
)

target("solv-generator")
set_kind("binary")
add_files("tools/generate_solv.cxx")
add_packages("pkgconfig::libsolv", "pkgconfig::tomlplusplus")

target("index-generator")
set_kind("binary")
add_files("tools/generate_index.cxx")
add_packages("pkgconfig::tomlplusplus")
