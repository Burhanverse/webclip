# Building WebClip

## Prerequisites

- **C++20 compiler**: `clang++` (Linux) / `clang-cl` (Windows), or `g++` / MSVC 2022
- **Linker**: `lld` (Linux) / `lld-link` (Windows)
- **Build System**: **CMake** (>= 3.22) and **Ninja**
- **Qt 6** (Core, Gui, Widgets, Svg, Quick, Qml)
- **libcurl**
- **Clipboard helper** (Linux only: `wl-clipboard` for Wayland, `xclip` for X11)

### Linux Dependencies

**Ubuntu / Debian:**
```bash
sudo apt-get update && sudo apt-get install -y \
  clang lld build-essential cmake ninja-build libcurl4-openssl-dev \
  qt6-base-dev qt6-declarative-dev qt6-tools-dev libqt6svg6-dev \
  qt6-wayland libqt6waylandclient6 wl-clipboard xclip
```

**Arch Linux:**
```bash
sudo pacman -S clang lld base-devel cmake ninja curl qt6-base qt6-declarative qt6-wayland qt6-svg qt6-tools wl-clipboard xclip
```

**Fedora / RHEL:**
```bash
sudo dnf install clang lld gcc-c++ cmake ninja-build libcurl-devel \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland-devel \
  qt6-qtsvg-devel qt6-qttools-devel wl-clipboard xclip
```

---

## Linux Build

### 1. Compile with Clang + Ninja + lld (Default)

Using CMake Presets:
```bash
cmake --preset linux-clang
cmake --build --preset linux-clang
```

Or manually:
```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++

cmake --build build --parallel
```

The output binary is located at `build/webclip`.

### Legacy GCC Fallback

To build using the legacy GCC toolchain:
```bash
cmake --preset linux-gcc
cmake --build --preset linux-gcc
```
Or manually:
```bash
cmake -B build-gcc -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DUSE_LEGACY_TOOLCHAIN=ON \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++

cmake --build build-gcc --parallel
```

### 2. Packaging (Optional)

- **All-in-one build script**:
  ```bash
  ./build_linux.sh
  ```
  *(Pass `--legacy` to compile with GCC)*
  Generates both the AppImage and Tarball in `./dist/`.

- **AppImage only**:
  ```bash
  ./packaging/build_appimage.sh
  ```

- **Tarball only**:
  ```bash
  ./packaging/pack_linux_tarball.sh
  ```

### 3. Local Install / Uninstall

To install the built binary, desktop launcher, and icons to `~/.local` (or `/usr/local` with `sudo`):

```bash
./packaging/install.sh
```

To remove:
```bash
./packaging/uninstall.sh
```

---

## Windows Build (clang-cl + Ninja + lld-link)

### 1. Install Dependencies

Install `libcurl` using [vcpkg](https://github.com/microsoft/vcpkg):
```powershell
vcpkg install curl:x64-windows
```

### 2. Compile

Using CMake Presets:
```powershell
cmake --preset windows-clang-cl -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build --preset windows-clang-cl
```

Or manually:
```powershell
cmake -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=clang-cl `
  -DCMAKE_CXX_COMPILER=clang-cl `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"

cmake --build build --parallel
```

Output binary: `build\webclip.exe`.

### Legacy MSVC Fallback

To build using the native MSVC compiler and linker:
```powershell
cmake --preset windows-msvc -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build --preset windows-msvc
```

### 3. Package Portable Zip

```powershell
New-Item -ItemType Directory -Force -Path "deploy"
if (Test-Path "build\webclip.exe") {
    Copy-Item "build\webclip.exe" -Destination "deploy\"
} elseif (Test-Path "build\Release\webclip.exe") {
    Copy-Item "build\Release\webclip.exe" -Destination "deploy\"
}
Copy-Item "$env:VCPKG_INSTALLATION_ROOT\installed\x64-windows\bin\*.dll" -Destination "deploy\"

windeployqt deploy\webclip.exe --qmldir src\gui\qml --no-translations --no-opengl-sw --compiler-runtime

New-Item -ItemType Directory -Force -Path "portable\webclip"
Copy-Item -Recurse "deploy\*" -Destination "portable\webclip\"
Compress-Archive -Path "portable\webclip" -DestinationPath "webclip-windows-x64-portable.zip" -Force
```

### 4. Build Inno Setup Installer (Optional)

Requires [Inno Setup 6](https://jrsoftware.org/isinfo.php):
```powershell
iscc.exe /DMyAppVersion="1.6.1" packaging\setup.iss
```

Outputs `webclip-setup-x64.exe`.

---

## Cross-Compiling from Linux (MinGW-w64)

The repository provides a toolchain file at `packaging/mingw-w64-x86_64.toolchain.cmake`:

```bash
cmake -B build-mingw -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=packaging/mingw-w64-x86_64.toolchain.cmake \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.0/mingw_64" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-mingw --parallel
```
