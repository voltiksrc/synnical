#!/usr/bin/env bash
set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"

cd "$(dirname "$0")"

echo ">> Building Synnical..."

if ! command -v cmake >/dev/null 2>&1; then
    echo "Error: CMake is required." >&2
    exit 1
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build build --parallel

echo ">> Installing Synnical..."

PRIVILEGE="${PRIVILEGE:-auto}"

# Sudo will be checked first as it is most common on linux distros,
# but doas will still be checked as it provides more support for people 
# installing Synnical.
if [[ $EUID -eq 0 || "$PREFIX" == "$HOME/"* ]]; then
    cmake --install build --prefix "$PREFIX"
else 
    if [[ "$PRIVILEGE" == "auto" ]]; then
        if command -v sudo >/dev/null 2>&1; then
            PRIVILEGE="sudo"
        elif command -v doas >/dev/null 2>&1; then
            PRIVILEGE="doas"
        else
            echo "Error: Neither doas nor sudo is available." >&2
            exit 1
        fi
    fi

    case "$PRIVILEGE" in
        sudo|doas)
            if ! command -v "$PRIVILEGE" >/dev/null 2>&1; then
                echo "Error: $PRIVILEGE is not installed." >&2
                exit 1
            fi
            "$PRIVILEGE" cmake --install build --prefix "$PREFIX"
            ;;
        *)
            echo "Error: Invalid PRIVILEGE value: $PRIVILEGE" >&2
            exit 1
            ;;
    esac
fi

echo ">> Installation complete!"
echo "Run 'synnical --help' to get started."
