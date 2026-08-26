#!/usr/bin/env bash
set -euo pipefail

REPO="Burhanverse/webclip"
APP_ID="io.github.burhanverse.webclip"
APP_NAME="WebClip"

DEFAULT_PREFIX="$HOME/.local"
if [ "$(id -u)" -eq 0 ]; then
    DEFAULT_PREFIX="/usr/local"
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
            echo "Usage: uninstall.sh [options]"
            echo ""
            echo "Options:"
            echo "  --prefix <path>      Prefix from which to uninstall (default: $DEFAULT_PREFIX)"
            echo "  -h, --help           Show this help message"
            echo ""
            echo "One-liner usage:"
            echo "  curl -fsSL https://raw.githubusercontent.com/$REPO/main/packaging/uninstall.sh | bash"
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
METAINFODIR="$DATADIR/metainfo"

echo "=========================================="
echo "  $APP_NAME Uninstaller"
echo "=========================================="
echo "==> Removing $APP_NAME files from: $PREFIX"

rm -f "$BINDIR/webclip"
rm -f "$APPDIR/${APP_ID}.desktop"
rm -f "$APPDIR/webclip.desktop"
rm -f "$ICONDIR/webclip.svg"
rm -f "$PIXMAPDIR/webclip.svg"
rm -f "$METAINFODIR/${APP_ID}.metainfo.xml"
rm -f "$METAINFODIR/webclip.appdata.xml"

# Refresh desktop database & icon caches
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPDIR" 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -q -t "$DATADIR/icons/hicolor" 2>/dev/null || true
fi

echo "=========================================="
echo "  $APP_NAME successfully uninstalled."
echo "=========================================="
