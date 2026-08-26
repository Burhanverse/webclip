#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Detect source files (supports running from extracted tarball or from the repository)
if [ -f "$SCRIPT_DIR/bin/webclip" ]; then
    # Running from extracted tarball root
    SRC_BIN="$SCRIPT_DIR/bin/webclip"
    SRC_DESKTOP="$SCRIPT_DIR/share/applications/webclip.desktop"
    SRC_ICON="$SCRIPT_DIR/share/icons/hicolor/scalable/apps/webclip.svg"
elif [ -f "$SCRIPT_DIR/../build/webclip" ]; then
    # Running from packaging/ inside repository
    SRC_BIN="$SCRIPT_DIR/../build/webclip"
    SRC_DESKTOP="$SCRIPT_DIR/webclip.desktop"
    SRC_ICON="$SCRIPT_DIR/../src/gui/resources/icons/webclip.svg"
elif [ -f "$SCRIPT_DIR/webclip" ]; then
    # Running from portable directory with webclip in same directory
    SRC_BIN="$SCRIPT_DIR/webclip"
    SRC_DESKTOP="$SCRIPT_DIR/webclip.desktop"
    SRC_ICON="$SCRIPT_DIR/webclip.svg"
elif [ -f "build/webclip" ]; then
    # Running from repository root
    SRC_BIN="build/webclip"
    SRC_DESKTOP="packaging/webclip.desktop"
    SRC_ICON="src/gui/resources/icons/webclip.svg"
else
    echo "Error: webclip executable not found."
    echo "If running from repository, please compile first (e.g. cmake --build build --parallel)."
    exit 1
fi

# Determine default prefix
if [ "$(id -u)" -eq 0 ]; then
    DEFAULT_PREFIX="/usr/local"
else
    DEFAULT_PREFIX="$HOME/.local"
fi

PREFIX="${PREFIX:-$DEFAULT_PREFIX}"

while [ $# -gt 0 ]; do
    case "$1" in
        --prefix=*)
            PREFIX="${1#*=}"
            shift
            ;;
        --prefix)
            PREFIX="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: ./install.sh [--prefix /path/to/prefix]"
            echo "Default prefix: $DEFAULT_PREFIX"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

BINDIR="$PREFIX/bin"
DATADIR="$PREFIX/share"
APPDIR="$DATADIR/applications"
ICONDIR="$DATADIR/icons/hicolor/scalable/apps"
PIXMAPDIR="$DATADIR/pixmaps"

echo "Installing WebClip Sync to prefix: $PREFIX..."
mkdir -p "$BINDIR" "$APPDIR" "$ICONDIR" "$PIXMAPDIR"

# Install binary
install -m 755 "$SRC_BIN" "$BINDIR/webclip"

# Install desktop entry
if [ -f "$SRC_DESKTOP" ]; then
    install -m 644 "$SRC_DESKTOP" "$APPDIR/webclip.desktop"
fi

# Install icons
if [ -f "$SRC_ICON" ]; then
    install -m 644 "$SRC_ICON" "$ICONDIR/webclip.svg"
    install -m 644 "$SRC_ICON" "$PIXMAPDIR/webclip.svg"
fi

# Refresh desktop database & icon caches if available
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPDIR" 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -q -t "$DATADIR/icons/hicolor" 2>/dev/null || true
fi

echo "WebClip Sync successfully installed to $BINDIR/webclip"
