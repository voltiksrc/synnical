# synnical

A Source based C++ system manager that handles packages using TOML recipes, and some other 
system settings, such as timezone, hostname, locale, and shell/user.

## Development

It is an independent system manager that anyone can use, if you would
like to help develop synnical, then please contact me at: tsukasatsa@gmail.com

## Features

- Source tarball downloads
- TOML Recipes
- SHA256 Verification
- Archive Extraction
- Source building which supports 6 types
- Basic Dependency resolution
- Install staging
- Merging into root

## Dependencies

- libarchive
- libcurl
- libcrypto
- libsolv
- CMake
- Toml++
- Ninja
- Make/Autotools
- Meson
- Cargo/rust

## Recommended dependencies
- Go
- Waf

## Build

```bash
git clone https://github.com/voltiksrc/synnical.git
cd synnical
```

```bash
cmake -S . -B build -G ninja
cmake --build build
```

## Install

```bash
sudo cmake --install build
```

## Usage

Apply the default configuration.

```bash
sudo synnical apply
```

## Status

It is able to parse the configuration file, extract the hostname, timezone, and packagesection, and print it.

<!--
It's able to download a tarball, verify with sha256, extract,
install dependencies, build, stage, and install.
-->
