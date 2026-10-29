#!/usr/bin/env bash
set -e

echo ">> Installing Synnical..."

if ! command -v cmake >/dev/null; then
    echo "Error: CMake is required."
    exit 1
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

echo ">> Installing binary..."
sudo cmake --install build --prefix /usr/local

echo ">> Installation complete!"
echo "Run 'synnical --help' to get started."
