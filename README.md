# Synnical

Synnical is a source-based, declarative/imperative hybrid system manager written in C++23.

It handles package installation, dependency resolution, and source compilation.

> [!NOTE]
> Synnical is still in active development. It's functional, but not very good or reliable.

## Features

- **Source-based package installation** — Download, verify, extract, compile, and install packages from source tarballs
- **Dependency resolution** — Uses libsolv to resolve dependencies and determine the proper order of installation
- **TOML recipes** — Package metadata, sources, dependencies, and build systems are all defined in the recipes
- **SHA-256 verification** — Verifies downloaded sources using libsodium
- **Build system support** — Make/Autotools, CMake, Meson, Cargo, Waf, Go, and some additional support for packages such as OpenSSL and Rust
- **Installation staging** — Builds are staged before being deployed into the target filesystem for safety
- **Package tracking** — Uses SQLite to keep track of installed packages
- **Declarative configuration** — Daycare provides an optional way to define packages and system settings in a configuration file

## Daycare

Daycare is Synnical's optional declarative configuration system.

Instead of installing packages individually, you can define the packages you want and some basic system settings in a TOML file.

For example:

```toml
[system]
hostname = "crypt"
timezone = "America/Los_Angeles"

[packages]
install = [
    "git",
    "curl",
    "fish"
]

# Eventually:

# Example user
[users.alice]
shell = "/usr/bin/fish"

[repositories]
enabled = ["main"]
```

Apply the configuration with:

```bash
synnical apply
```

Daycare currently supports parsing packages and system settings, with timezone option working.

**More information:** [Daycare documentation](doc/daycare.md)

## Package management

Synnical supports both imperative and declarative package management.

### Imperative

Install packages directly:

```bash
synnical -Fetch zlib
```

Install multiple packages:

```bash
synnical -Fetch zlib libpng
```

Preview installation plan/dependencies:

```bash
synnical resolve fish
```

### Declarative

Define the desired packages in Daycare:

```toml
[packages]
install = ["git", "curl", "fish"]
```

Then apply the configuration:

```bash
synnical apply
```

### Available commands

| Command | Description |
|---|---|
| `-Fetch <packages...>` | Resolve and install packages |
| `remove <package>` | Remove an installed package |
| `search <query>` | Search available packages |
| `resolve <package>` | Preview installation plan |
| `apply` | Apply the Daycare configuration |
| `--sync` | Sync repository metadata |
| `--list-installed` | List installed packages |
| `--version` | Display Synnical's version |
| `--help` | Display available commands |

## Building

Synnical uses **XMake** and requires a C++23 compatible compiler.

### Dependencies

The main development dependencies are:

- C++23 compiler
- XMake
- libcurl
- libarchive
- toml++
- SQLite3
- libsolv and libsolvext
- libsodium
- pkg-config
- INIReader

Highly recommended build tools: Ninja, GNU Make, Cargo, CMake, Meson.

Additional build tools such as Waf and Go may be needed depending on what packages you attempt to install.

### Compile from source

Clone the repository:

```bash
git clone https://github.com/voltiksrc/synnical.git
cd synnical
```

Configure release mode:

```bash
xmake f -m release
```

Compile Synnical

```bash
xmake
```

### Installation

An experimental installation script is included:

```bash
chmod +x install.sh
./install.sh
```

> [!WARNING]
> Synnical is not yet ready for personal use. The installation root and other paths are still development specific.

## Package repository

Synnical uses a separate repository to store package recipes and metadata.

**[Synnical official package repository](https://github.com/voltiksrc/synnical-repo)**

Package recipes use TOML. A simplified example:

```toml
[package]
name = "zlib"
version = "1.3.2"
architecture = "x86_64"

[source]
url = "https://example.org/zlib.tar.gz"
sha256 = "..."

[build]
system = "make"

[deps]
requires = []
build_requires = []
```

The repository is still growing! with a goal of **100 packages**.

## Metadata

I use two helper tools to generate repository metadata:

- [index-generator](tools/generate_index.cxx) — Scans package recipes and automatically generates `index.toml`.
- [solv-generator](tools/generate_solv.cxx) — Generates `packages.solv`, which Synnical uses for dependency resolution.

I also made a Fish function called `synnupdate` that runs both tools, commits and pushes repository changes, and syncs Synnical's local repository cache.

This saves me from manually editing the index and regenerating the metadata every time I add a package.

## Development status

Synnical is actively being developed, and several features are still incomplete unfortunately. But thats okay! More features have yet to come but expect more soon.

**Working/tested**

- [x] Source downloading and extraction
- [x] SHA-256 verification
- [x] Source compilation and installation staging
- [x] Dependency resolution with libsolv
- [x] SQLite installation tracking
- [x] Installed-package detection
- [x] Declarative timezone configuration

## Contributing

Synnical is an independent project, and contributions are welcome.

If you encounter a bug, want to suggest a feature, or would like to improve something, feel free to open an issue or pull request!

Package submissions and recipe improvements can be made through the [package repository](https://github.com/voltiksrc/synnical-repo).

## License

Synnical is licensed under the **GNU General Public License v3.0**.

See [LICENSE](LICENSE) for details.
