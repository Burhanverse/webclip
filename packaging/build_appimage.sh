#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_DIR"

BUILD_DIR="${BUILD_DIR:-build}"
OUTPUT_DIR="${OUTPUT_DIR:-.}"

if [ ! -f "${BUILD_DIR}/webclip" ]; then
    echo "Error: ${BUILD_DIR}/webclip not found. Please build the project first (e.g. cmake --build ${BUILD_DIR} --parallel)."
    exit 1
fi

export APPIMAGE_EXTRACT_AND_RUN=1
export NO_STRIP=1

if command -v qmake6 >/dev/null 2>&1; then
    export QMAKE="$(command -v qmake6)"
elif [ -f "/usr/lib/qt6/bin/qmake" ]; then
    export QMAKE="/usr/lib/qt6/bin/qmake"
elif [ -f "/usr/lib/x86_64-linux-gnu/qt6/bin/qmake" ]; then
    export QMAKE="/usr/lib/x86_64-linux-gnu/qt6/bin/qmake"
elif command -v qmake >/dev/null 2>&1; then
    export QMAKE="$(command -v qmake)"
fi
echo "Using QMake: ${QMAKE:-auto}"

download_file_with_progress() {
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
        echo "Error: Neither wget nor curl is installed. Cannot download ${desc}."
        exit 1
    fi

    if [ -s "$tmp_dest" ]; then
        mv -f "$tmp_dest" "$dest"
    else
        echo "Error: Download of ${desc} resulted in an empty file."
        rm -f "$tmp_dest"
        exit 1
    fi
}

if [ ! -s "linuxdeploy-x86_64.AppImage" ]; then
    download_file_with_progress \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" \
        "linuxdeploy-x86_64.AppImage" \
        "linuxdeploy (linuxdeploy-x86_64.AppImage)"
fi
chmod +x linuxdeploy-x86_64.AppImage

if [ ! -s "linuxdeploy-plugin-qt-x86_64.AppImage" ]; then
    download_file_with_progress \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" \
        "linuxdeploy-plugin-qt-x86_64.AppImage" \
        "linuxdeploy Qt plugin (linuxdeploy-plugin-qt-x86_64.AppImage)"
fi
chmod +x linuxdeploy-plugin-qt-x86_64.AppImage

if [ ! -s "runtime-x86_64" ]; then
    download_file_with_progress \
        "https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64" \
        "runtime-x86_64" \
        "AppImage runtime (runtime-x86_64)"
fi
chmod +x runtime-x86_64
export LDAI_RUNTIME_FILE="$PWD/runtime-x86_64"

QT6_QML_DIRS=(
    "/usr/lib/qt6/qml"
    "/usr/lib/x86_64-linux-gnu/qt6/qml"
    "/usr/lib64/qt6/qml"
    "/usr/local/qt6/qml"
)

QT6_PLUGINS_DIRS=(
    "/usr/lib/qt6/plugins"
    "/usr/lib/x86_64-linux-gnu/qt6/plugins"
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

if [ -n "$FOUND_PLUGINS_DIR" ]; then
    rm -rf "${BUILD_DIR}/clean_plugins"
    mkdir -p "${BUILD_DIR}/clean_plugins"
    for plugindir in "$FOUND_PLUGINS_DIR"/*; do
        bname="$(basename "$plugindir")"
        if [ "$bname" = "imageformats" ]; then
            mkdir -p "${BUILD_DIR}/clean_plugins/imageformats"
            for f in "$plugindir"/libq*.so; do
                if [ -f "$f" ]; then
                    cp -n "$f" "${BUILD_DIR}/clean_plugins/imageformats/"
                fi
            done
        elif [ -d "$plugindir" ]; then
            ln -s "$plugindir" "${BUILD_DIR}/clean_plugins/$bname"
        fi
    done
    export QT_PLUGIN_PATH="$PWD/${BUILD_DIR}/clean_plugins"

    REAL_QMAKE="${QMAKE:-qmake6}"
    cat << EOF > "${BUILD_DIR}/qmake_wrapper"
#!/usr/bin/env bash
if [ "\${1:-}" = "-query" ] && [ -z "\${2:-}" ]; then
    "$REAL_QMAKE" -query | sed "s|QT_INSTALL_PLUGINS:.*|QT_INSTALL_PLUGINS:$PWD/${BUILD_DIR}/clean_plugins|"
    exit 0
elif [ "\${1:-}" = "-query" ] && [ "\${2:-}" = "QT_INSTALL_PLUGINS" ]; then
    echo "$PWD/${BUILD_DIR}/clean_plugins"
    exit 0
fi
exec "$REAL_QMAKE" "\$@"
EOF
    chmod +x "${BUILD_DIR}/qmake_wrapper"
    export QMAKE="$PWD/${BUILD_DIR}/qmake_wrapper"
fi

export QML_SOURCES_PATHS="src/gui/qml"
if [ -n "$FOUND_QML_DIR" ]; then
    export QML_MODULES_PATHS="$FOUND_QML_DIR"
fi
export EXTRA_QT_PLUGINS="svg;platforms;wayland-graphics-integration-client;wayland-shell-integration;wayland-decoration-client;imageformats"

HELPER_DEPLOY_ARGS=()
for helper in wl-copy wl-paste xclip xsel; do
    helper_path="$(command -v "$helper" 2>/dev/null || true)"
    if [ -n "$helper_path" ]; then
        HELPER_DEPLOY_ARGS+=(-e "$helper_path")
        echo "Bundling clipboard helper: $helper ($helper_path)"
    else
        echo "Warning: clipboard helper '$helper' not found on system, it will not be bundled."
    fi
done

rm -rf AppDir

echo "Running linuxdeploy with Qt plugin..."
./linuxdeploy-x86_64.AppImage \
    --appdir AppDir \
    -e "${BUILD_DIR}/webclip" \
    "${HELPER_DEPLOY_ARGS[@]}" \
    -d packaging/webclip.desktop \
    -i src/gui/resources/icons/webclip.svg \
    --plugin qt

if [ -n "$FOUND_PLUGINS_DIR" ] && [ -d "$FOUND_PLUGINS_DIR/imageformats" ]; then
    mkdir -p AppDir/usr/plugins/imageformats
    for imgplugin in libqjpeg.so libqwebp.so libqsvg.so libqico.so libqgif.so; do
        if [ -f "$FOUND_PLUGINS_DIR/imageformats/$imgplugin" ]; then
            cp -n "$FOUND_PLUGINS_DIR/imageformats/$imgplugin" AppDir/usr/plugins/imageformats/ || true
        fi
    done
fi

if [ -n "$FOUND_QML_DIR" ]; then
    mkdir -p AppDir/usr/qml/QtQml AppDir/usr/qml/QtQuick

    if [ ! -d "AppDir/usr/qml/QtQml/WorkerScript" ] && [ -d "$FOUND_QML_DIR/QtQml/WorkerScript" ]; then
        echo "Bundling missing QtQml/WorkerScript..."
        cp -r "$FOUND_QML_DIR/QtQml/WorkerScript" AppDir/usr/qml/QtQml/
    fi

    if [ ! -d "AppDir/usr/qml/QtQuick/Shapes" ] && [ -d "$FOUND_QML_DIR/QtQuick/Shapes" ]; then
        echo "Bundling missing QtQuick/Shapes..."
        cp -r "$FOUND_QML_DIR/QtQuick/Shapes" AppDir/usr/qml/QtQuick/
    fi
    if [ ! -d "AppDir/usr/qml/QtQuick/Dialogs" ] && [ -d "$FOUND_QML_DIR/QtQuick/Dialogs" ]; then
        echo "Bundling missing QtQuick/Dialogs..."
        cp -r "$FOUND_QML_DIR/QtQuick/Dialogs" AppDir/usr/qml/QtQuick/
    fi
fi

if [ -n "$FOUND_PLUGINS_DIR" ]; then
    mkdir -p AppDir/usr/plugins/platforms \
             AppDir/usr/plugins/wayland-graphics-integration-client \
             AppDir/usr/plugins/wayland-shell-integration \
             AppDir/usr/plugins/wayland-decoration-client

    for f in "$FOUND_PLUGINS_DIR/platforms/"*wayland*; do
        if [ -f "$f" ]; then
            cp -n "$f" AppDir/usr/plugins/platforms/ || true
        fi
    done

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

if [ -f "packaging/io.github.burhanverse.webclip.metainfo.xml" ]; then
    mkdir -p AppDir/usr/share/metainfo
    cp "packaging/io.github.burhanverse.webclip.metainfo.xml" AppDir/usr/share/metainfo/io.github.burhanverse.webclip.metainfo.xml
fi

echo "Packaging final AppImage..."
./linuxdeploy-x86_64.AppImage \
    --appdir AppDir \
    --output appimage

mkdir -p "$OUTPUT_DIR"
APPIMAGE_TARGET="${OUTPUT_DIR}/webclip-linux-x86_64.AppImage"
mv WebClip*.AppImage "$APPIMAGE_TARGET" || true

echo "Successfully built: $APPIMAGE_TARGET"
