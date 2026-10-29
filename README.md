# Synnical

**Synnical** is a source-based, declarative/imperative hybrid system manager written in C++23.

It handles package installation, dependency resolution, source compilation, and basic system configuration through TOML files.

Synnical is being developed primarily for **Crypt Linux**, but the goal is to make it usable on other Linux distributions as well.

> [!NOTE]
> Synnical is still in active development. It's functional, but not yet suitable for managing your main system's packages. Testing in a container or isolated environment is recommended.

## Features

- **Source-based package installation** — Download, verify, extract, compile, and install packages from source.
- **Dependency resolution** — Uses libsolv to resolve dependencies and determine installation order.
- **TOML recipes** — Package metadata, sources, dependencies, and build systems are defined in TOML.
- **SHA-256 verification** — Verifies downloaded sources using libsodium.
- **Build system support** — Make/Autotools, CMake, Meson, Cargo, Waf, Go, and additional specialized backends.
- **Installation staging** — Builds are staged before being deployed into the target filesystem.
- **Package tracking** — Uses SQLite to keep track of installed packages.
- **Declarative configuration** — Daycare provides an optional way to define packages and system settings in a configuration file.

## Daycare

**Daycare** is Synnical's optional declarative configuration system.

Instead of installing packages individually, you can define the packages you want and basic system settings in a TOML file.

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

[users.voltik]
shell = "/usr/bin/fish"

[repositories]
enabled = ["main"]
```

Apply the configuration with:

```bash
synnical apply
```

Daycare currently supports parsing packages and system settings, with timezone application implemented. Other system settings, including hostname and user shells, are still being developed.

> [!IMPORTANT]
> Review your Daycare configuration before applying it. System settings may be modified, and Synnical is not yet ready for unrestricted system-wide use.

**More information:** [Daycare documentation](documentation/daycare.md)

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

Preview dependency resolution:

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
| `rm <package>` | Alias for `remove` |
| `search <query>` | Search available packages |
| `resolve <package>` | Preview dependency resolution |
| `apply` | Apply the Daycare configuration |
| `--sync` | Synchronize repository metadata |
| `--list-installed` | List installed packages |
| `--version` | Display Synnical's version |
| `--help` | Display available commands |

## Building

Synnical uses **CMake** and requires a C++23-compatible compiler.

### Dependencies

The main development dependencies are:

- C++23 compiler
- CMake
- libcurl
- libarchive
- toml++
- SQLite3
- libsolv and libsolvext
- libsodium
- pkg-config

Additional build tools such as Make, Ninja, Meson, Cargo, Go, and Waf may be required depending on which packages you're building.

### Compile from source

Clone the repository:

```bash
git clone https://github.com/voltiksrc/synnical.git
cd synnical
```

Configure and compile:

```bash
cmake -S . -B build
cmake --build build -j4
```

CMake will use your configured default generator, such as Unix Makefiles or Ninja.

### Installation

An experimental installation script is included:

```bash
chmod +x install.sh
./install.sh
```

Alternatively, after compiling:

```bash
sudo cmake --install build
```

> [!WARNING]
> Installing the executable does not currently make Synnical production-ready. Some paths, including the installation target root and configuration paths, are still development-specific.

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

*This example is illustrative. Actual recipes contain verified source URLs and hashes.*

The repository is still growing, with an initial goal of **100 packages**.

## Development status

Synnical is actively being developed, and several features are still incomplete.

**Working or tested:**

- [x] Source downloading and extraction
- [x] SHA-256 verification
- [x] Source compilation and installation staging
- [x] Dependency resolution with libsolv
- [x] SQLite installation tracking
- [x] Installed-package detection
- [x] Daycare TOML parsing
- [x] Declarative timezone configuration
- [x] Basic Ubuntu Docker testing

**Planned or still being improved:**

- [ ] More complete build dependency isolation
- [ ] Hostname configuration through Daycare
- [ ] User and login-shell configuration
- [ ] Additional package recipes
- [ ] Reproducible system configurations
- [ ] Reproducible builds
- [ ] Binary package support (possibly later)

## Contributing

Synnical is an independent project, and contributions are welcome.

If you encounter a bug, want to suggest a feature, or would like to improve something, feel free to open an issue or pull request.

Package submissions and recipe improvements can be made through the [package repository](https://github.com/voltiksrc/synnical-repo).

## License

Synnical is licensed under the **GNU General Public License v3.0**.

See [LICENSE](LICENSE) for details.
