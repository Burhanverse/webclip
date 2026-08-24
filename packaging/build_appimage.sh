#!/usr/bin/env bash
set -euo pipefail

# Script to build a fully self-contained Linux AppImage for WebClip
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_DIR"

if [ ! -f "build/webclip" ]; then
    echo "Error: build/webclip not found. Please build the project first (e.g. cmake --build build --parallel)."
    exit 1
fi

export APPIMAGE_EXTRACT_AND_RUN=1

# Download linuxdeploy tools if missing
if [ ! -f "linuxdeploy-x86_64.AppImage" ]; then
    echo "Downloading linuxdeploy..."
    wget -q -c https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
    chmod +x linuxdeploy-x86_64.AppImage
fi

if [ ! -f "linuxdeploy-plugin-qt-x86_64.AppImage" ]; then
    echo "Downloading linuxdeploy-plugin-qt..."
    wget -q -c https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
    chmod +x linuxdeploy-plugin-qt-x86_64.AppImage
fi

# Detect system Qt6 QML and Plugins directories
QT6_QML_DIRS=(
    "/usr/lib/x86_64-linux-gnu/qt6/qml"
    "/usr/lib/qt6/qml"
    "/usr/lib64/qt6/qml"
    "/usr/local/qt6/qml"
)

QT6_PLUGINS_DIRS=(
    "/usr/lib/x86_64-linux-gnu/qt6/plugins"
    "/usr/lib/qt6/plugins"
    "/usr/lib64/qt6/plugins"
    "/usr/local/qt6/plugins"
)

FOUND_QML_DIR=""
for dir in "${QT6_QML_DIRS[@]}"; do
    if [ -d "$dir" ]; then
        FOUND_QML_DIR="$dir"
        break
    fi
done

FOUND_PLUGINS_DIR=""
for dir in "${QT6_PLUGINS_DIRS[@]}"; do
    if [ -d "$dir" ]; then
        FOUND_PLUGINS_DIR="$dir"
        break
    fi
done

echo "Using Qt6 QML directory: ${FOUND_QML_DIR:-not found}"
echo "Using Qt6 Plugins directory: ${FOUND_PLUGINS_DIR:-not found}"

export QML_SOURCES_PATHS="src/gui/qml"
if [ -n "$FOUND_QML_DIR" ]; then
    export QML_MODULES_PATHS="$FOUND_QML_DIR"
fi
export EXTRA_QT_PLUGINS="svg;imageformats;wayland-graphics-integration-client;wayland-shell-integration;wayland-decoration-client;platforms"

# Clean previous AppDir
rm -rf AppDir

echo "Running linuxdeploy with Qt plugin..."
./linuxdeploy-x86_64.AppImage \
    --appdir AppDir \
    -e build/webclip \
    -d packaging/webclip.desktop \
    -i src/gui/resources/icons/webclip.svg \
    --plugin qt

# Ensure essential QML modules (WorkerScript, Shapes, Dialogs, etc.) are bundled
if [ -n "$FOUND_QML_DIR" ]; then
    mkdir -p AppDir/usr/qml/QtQml AppDir/usr/qml/QtQuick

    # Copy WorkerScript if missing
    if [ ! -d "AppDir/usr/qml/QtQml/WorkerScript" ] && [ -d "$FOUND_QML_DIR/QtQml/WorkerScript" ]; then
        echo "Bundling missing QtQml/WorkerScript..."
        cp -r "$FOUND_QML_DIR/QtQml/WorkerScript" AppDir/usr/qml/QtQml/
    fi

    # Copy Shapes if missing
    if [ ! -d "AppDir/usr/qml/QtQuick/Shapes" ] && [ -d "$FOUND_QML_DIR/QtQuick/Shapes" ]; then
        echo "Bundling missing QtQuick/Shapes..."
        cp -r "$FOUND_QML_DIR/QtQuick/Shapes" AppDir/usr/qml/QtQuick/
    fi

    # Copy Dialogs if missing
    if [ ! -d "AppDir/usr/qml/QtQuick/Dialogs" ] && [ -d "$FOUND_QML_DIR/QtQuick/Dialogs" ]; then
        echo "Bundling missing QtQuick/Dialogs..."
        cp -r "$FOUND_QML_DIR/QtQuick/Dialogs" AppDir/usr/qml/QtQuick/
    fi
fi

# Ensure Wayland platform plugins and shell integrations are bundled
if [ -n "$FOUND_PLUGINS_DIR" ]; then
    mkdir -p AppDir/usr/plugins/platforms \
             AppDir/usr/plugins/wayland-graphics-integration-client \
             AppDir/usr/plugins/wayland-shell-integration \
             AppDir/usr/plugins/wayland-decoration-client

    # Copy wayland platform plugin
    for f in "$FOUND_PLUGINS_DIR/platforms/"*wayland*; do
        if [ -f "$f" ]; then
            cp -n "$f" AppDir/usr/plugins/platforms/ || true
        fi
    done

    # Copy wayland client integration plugins
    if [ -d "$FOUND_PLUGINS_DIR/wayland-graphics-integration-client" ]; then
        cp -rn "$FOUND_PLUGINS_DIR/wayland-graphics-integration-client/"* AppDir/usr/plugins/wayland-graphics-integration-client/ 2>/dev/null || true
    fi
    if [ -d "$FOUND_PLUGINS_DIR/wayland-shell-integration" ]; then
        cp -rn "$FOUND_PLUGINS_DIR/wayland-shell-integration/"* AppDir/usr/plugins/wayland-shell-integration/ 2>/dev/null || true
    fi
    if [ -d "$FOUND_PLUGINS_DIR/wayland-decoration-client" ]; then
        cp -rn "$FOUND_PLUGINS_DIR/wayland-decoration-client/"* AppDir/usr/plugins/wayland-decoration-client/ 2>/dev/null || true
    fi
fi

echo "Packaging final AppImage..."
./linuxdeploy-x86_64.AppImage \
    --appdir AppDir \
    --output appimage

mv WebClip*.AppImage webclip-linux-x86_64.AppImage || true

echo "Successfully built webclip-linux-x86_64.AppImage!"
