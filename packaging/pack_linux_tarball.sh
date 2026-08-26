#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_DIR"

BUILD_DIR="${BUILD_DIR:-build}"
OUTPUT_DIR="${OUTPUT_DIR:-.}"
ARCH="$(uname -m)"
VERSION="1.3.0"
if [ -f "src/version.hpp" ]; then
    DETECTED_VERSION="$(grep -E 'VERSION_STRING\s*=' src/version.hpp | sed -E 's/.*"([^"]+)".*/\1/' || true)"
    if [ -n "$DETECTED_VERSION" ]; then
        VERSION="$DETECTED_VERSION"
    fi
fi

echo "Packaging WebClip v${VERSION} for Linux (${ARCH})..."

if [ ! -f "${BUILD_DIR}/webclip" ]; then
    echo "Error: ${BUILD_DIR}/webclip not found. Please build the project first (e.g. cmake --build ${BUILD_DIR} --parallel)."
    exit 1
fi

DIST_NAME="webclip-linux-${ARCH}"
STAGE_DIR="build/tarball_staging"
TARGET_DIR="${STAGE_DIR}/${DIST_NAME}"

rm -rf "${STAGE_DIR}"
mkdir -p "${TARGET_DIR}/bin"
mkdir -p "${TARGET_DIR}/share/applications"
mkdir -p "${TARGET_DIR}/share/icons/hicolor/scalable/apps"
mkdir -p "${TARGET_DIR}/share/pixmaps"

cp "${BUILD_DIR}/webclip" "${TARGET_DIR}/bin/webclip"
if command -v strip >/dev/null 2>&1; then
    strip --strip-unneeded "${TARGET_DIR}/bin/webclip" || true
fi
chmod 755 "${TARGET_DIR}/bin/webclip"

mkdir -p "${TARGET_DIR}/share/metainfo"
cp "packaging/io.github.burhanverse.webclip.metainfo.xml" "${TARGET_DIR}/share/metainfo/io.github.burhanverse.webclip.metainfo.xml" 2>/dev/null || true
cp "packaging/io.github.burhanverse.webclip.desktop" "${TARGET_DIR}/share/applications/io.github.burhanverse.webclip.desktop" 2>/dev/null || true
cp "packaging/webclip.desktop" "${TARGET_DIR}/share/applications/webclip.desktop"
cp "packaging/webclip.desktop" "${TARGET_DIR}/webclip.desktop"
cp "src/gui/resources/icons/webclip.svg" "${TARGET_DIR}/share/icons/hicolor/scalable/apps/webclip.svg"
cp "src/gui/resources/icons/webclip.svg" "${TARGET_DIR}/share/pixmaps/webclip.svg"
cp "src/gui/resources/icons/webclip.svg" "${TARGET_DIR}/webclip.svg"

if [ -f "README.md" ]; then
    cp "README.md" "${TARGET_DIR}/"
fi
if [ -f "LICENSE" ]; then
    cp "LICENSE" "${TARGET_DIR}/"
fi

cp "packaging/install.sh" "${TARGET_DIR}/install.sh"
chmod 755 "${TARGET_DIR}/install.sh"
cp "packaging/uninstall.sh" "${TARGET_DIR}/uninstall.sh"
chmod 755 "${TARGET_DIR}/uninstall.sh"

mkdir -p "${OUTPUT_DIR}"
TARBALL_PATH="${OUTPUT_DIR}/${DIST_NAME}.tar.gz"
tar -czf "${TARBALL_PATH}" -C "${STAGE_DIR}" "${DIST_NAME}"

echo "Successfully generated: ${TARBALL_PATH}"
ls -lh "${TARBALL_PATH}"
