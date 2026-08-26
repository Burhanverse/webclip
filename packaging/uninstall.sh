#!/usr/bin/env bash
set -euo pipefail

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
            echo "Usage: ./uninstall.sh [--prefix /path/to/prefix]"
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

echo "Uninstalling WebClip Sync from prefix: $PREFIX..."
rm -f "$BINDIR/webclip"
rm -f "$APPDIR/webclip.desktop"
rm -f "$ICONDIR/webclip.svg"
rm -f "$PIXMAPDIR/webclip.svg"

# Refresh desktop database & icon caches if available
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPDIR" 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -q -t "$DATADIR/icons/hicolor" 2>/dev/null || true
fi

echo "WebClip Sync successfully uninstalled from $PREFIX."
