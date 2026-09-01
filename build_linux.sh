#!/usr/bin/env bash
set -euo pipefail

# WebClip Linux Build & Packaging Script
# Builds the binary, portable AppImage, and distributable tarball locally.

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO_DIR"

BUILD_TYPE="Release"
BUILD_DIR="${BUILD_DIR:-build}"
OUTPUT_DIR="${OUTPUT_DIR:-dist}"
PACKAGE_APPIMAGE=true
PACKAGE_TARBALL=true
CLEAN=false
USE_LEGACY=false

print_usage() {
    cat << 'EOF'
Usage: ./build_linux.sh [OPTIONS]

Options:
  --aio, --all-in-one   Build executable and package both AppImage and Tarball [default]
  --appimage-only       Build executable and AppImage package only
  --tarball-only        Build executable and tarball package only
  --bin-only            Compile only the webclip executable (skip packaging)
  --clean               Remove build and dist directories before starting
  --legacy, --gcc       Use legacy GCC toolchain instead of Clang + lld
  --build-type <type>   Set CMake build type (Release, Debug, RelWithDebInfo) [default: Release]
  --build-dir <dir>     Set build directory [default: build]
  --output-dir <dir>    Set output directory for packages [default: dist]
  -h, --help            Show this help message
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --aio|--all-in-one)
            PACKAGE_APPIMAGE=true
            PACKAGE_TARBALL=true
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --legacy|--gcc)
            USE_LEGACY=true
            shift
            ;;
        --build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --bin-only)
            PACKAGE_APPIMAGE=false
            PACKAGE_TARBALL=false
            shift
            ;;
        --appimage-only)
            PACKAGE_APPIMAGE=true
            PACKAGE_TARBALL=false
            shift
            ;;
        --tarball-only)
            PACKAGE_APPIMAGE=false
            PACKAGE_TARBALL=true
            shift
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

echo "=========================================="
echo "  WebClip Linux Builder"
echo "  Toolchain:  $(if [ "$USE_LEGACY" = true ]; then echo "GCC (Legacy)"; else echo "Clang + lld"; fi)"
echo "  Build Type: $BUILD_TYPE"
echo "  Build Dir:  $BUILD_DIR"
echo "  Output Dir: $OUTPUT_DIR"
echo "=========================================="

# Check for required tools
command -v cmake >/dev/null 2>&1 || { echo "Error: cmake is required but not installed."; exit 1; }

GENERATOR="Ninja"
if ! command -v ninja >/dev/null 2>&1; then
    GENERATOR="Unix Makefiles"
fi

if [ "$CLEAN" = true ]; then
    echo "==> Cleaning previous build directories..."
    rm -rf "$BUILD_DIR" "$OUTPUT_DIR" AppDir
fi

mkdir -p "$BUILD_DIR" "$OUTPUT_DIR"

CMAKE_EXTRA_FLAGS=()
if [ "$USE_LEGACY" = true ]; then
    CMAKE_EXTRA_FLAGS+=("-DUSE_LEGACY_TOOLCHAIN=ON" "-DCMAKE_C_COMPILER=gcc" "-DCMAKE_CXX_COMPILER=g++")
else
    if command -v clang >/dev/null 2>&1 && command -v clang++ >/dev/null 2>&1; then
        CMAKE_EXTRA_FLAGS+=("-DUSE_LEGACY_TOOLCHAIN=OFF" "-DCMAKE_C_COMPILER=clang" "-DCMAKE_CXX_COMPILER=clang++")
    fi
fi

echo "==> Configuring CMake ($GENERATOR, $BUILD_TYPE)..."
cmake -B "$BUILD_DIR" -G "$GENERATOR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" "${CMAKE_EXTRA_FLAGS[@]}"

echo "==> Compiling WebClip..."
cmake --build "$BUILD_DIR" --parallel

echo "==> Sanity checking CLI executable..."
"$BUILD_DIR/webclip" --help >/dev/null

echo "==> Binary built successfully: $BUILD_DIR/webclip"

if [ "$PACKAGE_APPIMAGE" = true ]; then
    echo "==> Building portable AppImage..."
    chmod +x packaging/build_appimage.sh
    BUILD_DIR="$BUILD_DIR" OUTPUT_DIR="$OUTPUT_DIR" ./packaging/build_appimage.sh
fi

if [ "$PACKAGE_TARBALL" = true ]; then
    echo "==> Packaging distributable tarball..."
    chmod +x packaging/pack_linux_tarball.sh
    BUILD_DIR="$BUILD_DIR" OUTPUT_DIR="$OUTPUT_DIR" ./packaging/pack_linux_tarball.sh
fi

echo ""
echo "=========================================="
echo "  Build Summary"
echo "=========================================="
echo "Binary:  $BUILD_DIR/webclip"
if [ "$PACKAGE_APPIMAGE" = true ] || [ "$PACKAGE_TARBALL" = true ]; then
    echo "Packages generated in $OUTPUT_DIR/:"
    ls -lh "$OUTPUT_DIR"/webclip-linux-* 2>/dev/null || true
fi
echo "=========================================="
echo "Done!"
