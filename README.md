# synnical

A Source based C++ system manager that uses TOML recipes that handles packages and someother system settings.

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
cd omi
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

Source install:

```bash
sudo synnical -Fetch fastfetch
```

## Status

It's able to download a tarball, verify with sha256, extract,
install dependencies, build, stage, and install.
