#!/usr/bin/env bash
set -euo pipefail

# Configuration & Defaults
REPO="Burhanverse/webclip"
APP_ID="io.github.burhanverse.webclip"
APP_NAME="WebClip"

DEFAULT_PREFIX="$HOME/.local"
if [ "$(id -u)" -eq 0 ]; then
    DEFAULT_PREFIX="/usr/local"
fi

PREFIX="${PREFIX:-$DEFAULT_PREFIX}"
INSTALL_TYPE="tarball" # "tarball" or "appimage"
TARGET_VERSION="latest"

# Parse CLI arguments
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
        --appimage)
            INSTALL_TYPE="appimage"
            shift
            ;;
        --tarball)
            INSTALL_TYPE="tarball"
            shift
            ;;
        --version=*)
            TARGET_VERSION="${1#*=}"
            shift
            ;;
        --version)
            TARGET_VERSION="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: install.sh [options]"
            echo ""
            echo "Options:"
            echo "  --prefix <path>      Install destination (default: $DEFAULT_PREFIX)"
            echo "  --appimage           Install standalone portable AppImage bundle"
            echo "  --tarball            Install native Linux package (default)"
            echo "  --version <tag>      Install specific version (default: latest)"
            echo "  -h, --help           Show this help message"
            echo ""
            echo "One-liner usage:"
            echo "  curl -fsSL https://raw.githubusercontent.com/$REPO/main/packaging/install.sh | bash"
            echo "  curl -fsSL https://raw.githubusercontent.com/$REPO/main/packaging/install.sh | bash -s -- --appimage"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" 2>/dev/null && pwd || echo "")"

# Helper for download with progress
download_file() {
    local url="$1"
    local dest="$2"
    local desc="${3:-$dest}"

    echo "==> Downloading ${desc}..."
    local tmp_dest="${dest}.tmp"
    rm -f "$tmp_dest"

    if command -v wget >/dev/null 2>&1; then
        wget -q --show-progress "$url" -O "$tmp_dest"
    elif command -v curl >/dev/null 2>&1; then
        curl -fL --progress-bar "$url" -o "$tmp_dest"
    else
        echo "Error: Neither curl nor wget is installed. Please install one to continue."
        exit 1
    fi

    if [ -s "$tmp_dest" ]; then
        mv -f "$tmp_dest" "$dest"
    else
        echo "Error: Download failed or returned empty file for $desc."
        rm -f "$tmp_dest"
        exit 1
    fi
}

find_local_file() {
    for candidate in "$@"; do
        if [ -n "$candidate" ] && [ -f "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

# Locate local source files if running inside an extracted release or repo
LOCAL_BIN="$(find_local_file \
    "$SCRIPT_DIR/bin/webclip" \
    "$SCRIPT_DIR/webclip" \
    "$SCRIPT_DIR/../build/webclip" \
    "build/webclip" || true)"

TMP_DIR=""
cleanup() {
    if [ -n "$TMP_DIR" ] && [ -d "$TMP_DIR" ]; then
        rm -rf "$TMP_DIR"
    fi
}
trap cleanup EXIT

# If not running from existing local files, fetch latest release from GitHub
if [ -z "$LOCAL_BIN" ]; then
    echo "=========================================="
    echo "  $APP_NAME Linux Installer"
    echo "=========================================="
    echo "==> Resolving release info from GitHub ($REPO)..."

    if [ "$TARGET_VERSION" = "latest" ]; then
        RELEASE_API_URL="https://api.github.com/repos/$REPO/releases/latest"
        TAG_NAME=""
        if command -v curl >/dev/null 2>&1; then
            TAG_NAME="$(curl -sSL "$RELEASE_API_URL" | grep -o '"tag_name": *"[^"]*"' | head -n1 | cut -d'"' -f4 || true)"
        elif command -v wget >/dev/null 2>&1; then
            TAG_NAME="$(wget -qO- "$RELEASE_API_URL" | grep -o '"tag_name": *"[^"]*"' | head -n1 | cut -d'"' -f4 || true)"
        fi

        if [ -z "$TAG_NAME" ]; then
            DOWNLOAD_BASE="https://github.com/$REPO/releases/latest/download"
            echo "==> Using latest release download channel..."
        else
            DOWNLOAD_BASE="https://github.com/$REPO/releases/download/$TAG_NAME"
            echo "==> Found release: $TAG_NAME"
        fi
    else
        DOWNLOAD_BASE="https://github.com/$REPO/releases/download/$TARGET_VERSION"
        echo "==> Using specified release: $TARGET_VERSION"
    fi

    TMP_DIR="$(mktemp -d /tmp/webclip-install-XXXXXX)"

    if [ "$INSTALL_TYPE" = "appimage" ]; then
        APPIMAGE_URL="$DOWNLOAD_BASE/webclip-linux-x86_64.AppImage"
        download_file "$APPIMAGE_URL" "$TMP_DIR/webclip" "WebClip AppImage"
        chmod +x "$TMP_DIR/webclip"
        LOCAL_BIN="$TMP_DIR/webclip"

        # Download icon, desktop, and metadata directly for AppImage mode
        RAW_BASE="https://raw.githubusercontent.com/$REPO/main"
        mkdir -p "$TMP_DIR/share/applications" "$TMP_DIR/share/icons" "$TMP_DIR/share/metainfo"
        download_file "$RAW_BASE/packaging/io.github.burhanverse.webclip.desktop" "$TMP_DIR/share/applications/${APP_ID}.desktop" "Desktop entry"
        download_file "$RAW_BASE/src/gui/resources/icons/webclip.svg" "$TMP_DIR/share/icons/webclip.svg" "Application icon"
        download_file "$RAW_BASE/packaging/io.github.burhanverse.webclip.metainfo.xml" "$TMP_DIR/share/metainfo/${APP_ID}.metainfo.xml" "AppStream metadata"

        SRC_DESKTOP="$TMP_DIR/share/applications/${APP_ID}.desktop"
        SRC_ICON="$TMP_DIR/share/icons/webclip.svg"
        SRC_METAINFO="$TMP_DIR/share/metainfo/${APP_ID}.metainfo.xml"
    else
        TARBALL_URL="$DOWNLOAD_BASE/webclip-linux-x86_64.tar.gz"
        download_file "$TARBALL_URL" "$TMP_DIR/package.tar.gz" "WebClip Package"
        echo "==> Extracting package..."
        tar -xzf "$TMP_DIR/package.tar.gz" -C "$TMP_DIR"

        LOCAL_BIN="$(find "$TMP_DIR" -type f -name "webclip" | head -n1 || true)"
        if [ -z "$LOCAL_BIN" ]; then
            echo "Error: webclip executable not found in release archive."
            exit 1
        fi
        SRC_DESKTOP="$(find "$TMP_DIR" -type f -name "${APP_ID}.desktop" | head -n1 || true)"
        SRC_ICON="$(find "$TMP_DIR" -type f -name "webclip.svg" | head -n1 || true)"
        SRC_METAINFO="$(find "$TMP_DIR" -type f \( -name "*.metainfo.xml" -o -name "*.appdata.xml" \) | head -n1 || true)"
    fi
else
    SRC_DESKTOP="$(find_local_file \
        "$SCRIPT_DIR/share/applications/${APP_ID}.desktop" \
        "$SCRIPT_DIR/${APP_ID}.desktop" \
        "packaging/${APP_ID}.desktop" || true)"

    SRC_ICON="$(find_local_file \
        "$SCRIPT_DIR/share/icons/hicolor/scalable/apps/webclip.svg" \
        "$SCRIPT_DIR/webclip.svg" \
        "$SCRIPT_DIR/../src/gui/resources/icons/webclip.svg" \
        "src/gui/resources/icons/webclip.svg" || true)"

    SRC_METAINFO="$(find_local_file \
        "$SCRIPT_DIR/share/metainfo/${APP_ID}.metainfo.xml" \
        "$SCRIPT_DIR/${APP_ID}.metainfo.xml" \
        "packaging/${APP_ID}.metainfo.xml" || true)"
fi

BINDIR="$PREFIX/bin"
DATADIR="$PREFIX/share"
APPDIR="$DATADIR/applications"
ICONDIR="$DATADIR/icons/hicolor/scalable/apps"
PIXMAPDIR="$DATADIR/pixmaps"
METAINFODIR="$DATADIR/metainfo"

echo "==> Installing $APP_NAME to: $PREFIX"
mkdir -p "$BINDIR" "$APPDIR" "$ICONDIR" "$PIXMAPDIR" "$METAINFODIR"

# Install executable
install -m 755 "$LOCAL_BIN" "$BINDIR/webclip"

# Install desktop entry (single, reverse-DNS named entry)
if [ -n "$SRC_DESKTOP" ] && [ -f "$SRC_DESKTOP" ]; then
    install -m 644 "$SRC_DESKTOP" "$APPDIR/${APP_ID}.desktop"
    # Remove any stale legacy entry from older installs to avoid duplicates
    if [ -f "$APPDIR/webclip.desktop" ]; then
        rm -f "$APPDIR/webclip.desktop"
    fi
fi

# Install icons
if [ -n "$SRC_ICON" ] && [ -f "$SRC_ICON" ]; then
    install -m 644 "$SRC_ICON" "$ICONDIR/webclip.svg"
    install -m 644 "$SRC_ICON" "$PIXMAPDIR/webclip.svg"
fi

# Install AppStream metadata
if [ -n "$SRC_METAINFO" ] && [ -f "$SRC_METAINFO" ]; then
    install -m 644 "$SRC_METAINFO" "$METAINFODIR/${APP_ID}.metainfo.xml"
fi

# Update desktop database & icon caches
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPDIR" 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -q -t "$DATADIR/icons/hicolor" 2>/dev/null || true
fi

echo "=========================================="
echo "  $APP_NAME successfully installed!"
echo "  Location: $BINDIR/webclip"
echo "=========================================="

# Check if BINDIR is in user's PATH
case ":$PATH:" in
    *":$BINDIR:"*) ;;
    *)
        echo ""
        echo "Note: '$BINDIR' is not in your current PATH."
        echo "To run 'webclip' directly from anywhere, add it to your shell configuration:"
        echo "  export PATH=\"$BINDIR:\$PATH\""
        echo ""
        ;;
esac
