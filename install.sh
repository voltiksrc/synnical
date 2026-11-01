
#!/usr/bin/env bash
set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"
PRIVILEGE="${PRIVILEGE:-auto}"

cd "$(dirname "$0")"

echo ">> Building Synnical..."

if ! command -v xmake >/dev/null 2>&1; then
    echo "Error: Xmake is required." >&2
    exit 1
fi

xmake f -m release
xmake

# Locate the release executable.
ARCH="$(uname -m)"
BINARY="build/linux/$ARCH/release/synnical"

if [[ ! -f "$BINARY" ]]; then
    echo "Error: Synnical executable not found: $BINARY" >&2
    exit 1
fi

echo ">> Installing Synnical..."

# Install directly when running as root or when the
# destination is inside the user's home directory.
if [[ $EUID -eq 0 || "$PREFIX" == "$HOME" || "$PREFIX" == "$HOME/"* ]]; then
    install -Dm755 "$BINARY" "$PREFIX/bin/synnical"
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

            "$PRIVILEGE" install -Dm755 \
                "$BINARY" "$PREFIX/bin/synnical"
            ;;
        *)
            echo "Error: Invalid PRIVILEGE value: $PRIVILEGE" >&2
            exit 1
            ;;
    esac
fi

# Install default user configuration
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
CONFIG_DIR="$CONFIG_HOME/synnical"
CONFIG_FILE="$CONFIG_DIR/settings.ini"

if [[ ! -f "$CONFIG_FILE" ]]; then
    echo ">> Installing default settings.ini..."

    mkdir -p "$CONFIG_DIR"
    install -m644 "config/settings.ini" "$CONFIG_FILE"
else
    echo ">> Existing settings.ini found, skipping."
fi

echo ">> Installation complete!"
echo "Run 'synnical --help' to get started."

