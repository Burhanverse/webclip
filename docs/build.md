# Building WebClip

This document provides instructions for compiling WebClip from source and creating release packages on Linux and Windows.

---

## Linux (Ubuntu, Debian, Fedora, Arch)

### 1. Prerequisites

- **C++17 Compiler**: `g++` (>= 9) or `clang++`
- **Build System**: `cmake` (>= 3.16) and `ninja` or `make`
- **Qt 6**: `Qt6::Core`, `Qt6::Gui`, `Qt6::Quick`, `Qt6::Qml`, `Qt6::QuickControls2`, `Qt6::Svg`, `Qt6::Widgets`
- **Networking**: `libcurl`
- **System Clipboard Tools**: `wl-clipboard` (Wayland) or `xclip` / `xsel` (X11)

#### Install Dependencies

**Debian / Ubuntu:**
```bash
sudo apt-get update && sudo apt-get install -y \
  build-essential cmake ninja-build libcurl4-openssl-dev \
  qt6-base-dev qt6-declarative-dev qt6-tools-dev libqt6svg6-dev \
  qt6-wayland libqt6waylandclient6 \
  qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-templates \
  qml6-module-qtquick-window qml6-module-qtquick-shapes \
  qml6-module-qtquick-dialogs qml6-module-qtqml-workerscript \
  qml6-module-qtqml-models qml6-module-qt-labs-platform \
  qml6-module-qt-labs-settings qt6-image-formats-plugins \
  wl-clipboard xclip
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake ninja curl qt6-base qt6-declarative qt6-wayland qt6-svg qt6-tools qt6-imageformats wl-clipboard xclip
```

**Fedora:**
```bash
sudo dnf install gcc-c++ cmake ninja-build libcurl-devel \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland-devel \
  qt6-qtsvg-devel qt6-qttools-devel qt6-qtimageformats \
  wl-clipboard xclip
```

**Rocky Linux / AlmaLinux / RHEL (9, 10):**
```bash
sudo dnf install -y dnf-plugins-core epel-release
sudo dnf config-manager --set-enabled crb
sudo dnf install gcc-c++ make cmake ninja-build libcurl-devel wget \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel \
  qt6-qttools-devel qt6-qtwayland qt6-qtimageformats \
  wl-clipboard xclip
```
Note: `ninja-build` lives in the CRB repository and `wl-clipboard` / `xclip` in EPEL, so both must be enabled.

---

### 2. Automated Build & Packaging (All-in-One)

You can build the binary and generate both the standalone AppImage and Linux tarball with a single command:

```bash
./build_linux.sh
```

Artifacts will be packaged into the `./dist` directory (`dist/webclip-linux-x86_64.AppImage` and `dist/webclip-linux-x86_64.tar.gz`).

#### Options:
- `--aio`, `--all-in-one`: Build executable and package both AppImage and Tarball (default).
- `--appimage-only`: Build the binary and AppImage package only.
- `--tarball-only`: Build the binary and Tarball package only.
- `--bin-only`: Build only the `build/webclip` binary without packaging.
- `--clean`: Clean previous build outputs before compiling.
- `--build-type <Release|Debug>`: Choose CMake build configuration (default: `Release`).
- `--output-dir <dir>`: Destination directory for packages (default: `dist`).

---

### 3. Manual Compilation

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The output binary is placed at `build/webclip`.

---

### 3. Build Portable AppImage

WebClip can be packaged into a self-contained AppImage with Wayland and QML dependencies bundled:

```bash
./packaging/build_appimage.sh
```

This generates `webclip-linux-x86_64.AppImage`.

---

### 4. Build Distributable Tarball

To package a standalone Linux tarball containing the binary, desktop launcher entry, application icons, and installation/uninstallation scripts:

```bash
./packaging/pack_linux_tarball.sh
```

This generates `webclip-linux-x86_64.tar.gz`.

To install from the extracted tarball to `~/.local` (or `/usr/local` with `sudo`):

```bash
./install.sh
```

To uninstall:

```bash
./uninstall.sh
```

You can also run `./packaging/install.sh` and `./packaging/uninstall.sh` directly from the repository root after building.

---

## Windows (MSVC)

### 1. Prerequisites

- **Visual Studio 2022** with Desktop development with C++
- **CMake** (>= 3.16) and **Ninja**
- **Qt 6.5+** (MSVC 2022 64-bit)
- **vcpkg** for dependency management
- **Inno Setup 6** (optional, for standalone installer creation)

---

### 2. Install libcurl via vcpkg

```powershell
vcpkg install curl:x64-windows
```

---

### 3. Configure & Compile

```powershell
cmake -B build `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"

cmake --build build --config Release --parallel
```

The compiled executable will be at `build\Release\webclip.exe`.

---

### 4. Create Portable Bundle (`windeployqt`)

To create a self-contained folder containing all Qt DLLs, QML modules, and libcurl:

```powershell
# Create staging folder and copy binary
New-Item -ItemType Directory -Force -Path "deploy"
Copy-Item "build\Release\webclip.exe" -Destination "deploy\"

# Copy vcpkg curl DLLs
Copy-Item "$env:VCPKG_INSTALLATION_ROOT\installed\x64-windows\bin\*.dll" -Destination "deploy\"

# Deploy Qt dependencies and QML modules
windeployqt deploy\webclip.exe `
  --qmldir src\gui\qml `
  --no-translations `
  --no-system-d3d-compiler `
  --no-opengl-sw `
  --compiler-runtime

# Package portable ZIP
New-Item -ItemType Directory -Force -Path "portable\webclip"
Copy-Item -Recurse "deploy\*" -Destination "portable\webclip\"
Compress-Archive -Path "portable\webclip" -DestinationPath "webclip-windows-x64-portable.zip" -Force
```

---

### 5. Build Standalone Inno Setup Installer

```powershell
iscc.exe /DMyAppVersion="1.5.0" packaging\setup.iss
```

This outputs `webclip-setup-x64.exe`.

---

## Cross-compiling from Linux (MinGW-w64, experimental)

The CMake configuration is fully toolchain-agnostic, so the Windows binary can also be cross-compiled from a Linux host using MinGW-w64. The repository ships a ready-made toolchain file at `packaging/mingw-w64-x86_64.toolchain.cmake`.

### 1. Prerequisites

- **MinGW-w64 toolchain**: `sudo apt install mingw-w64` (Debian/Ubuntu) or `sudo pacman -S mingw-w64-gcc` (Arch)
- **libcurl for MinGW**: either via vcpkg (`vcpkg install curl:x64-mingw-dynamic`) or MXE (`mxe-x86_64-w64-mingw32.static-curl`)
- **Qt 6 MinGW build**: install the `win64_mingw` desktop kit with the Qt Online Installer (e.g. to `~/Qt/6.8.0/mingw_64`). A Linux-hosted Qt MinGW build is not required — Qt's Windows binaries work directly for cross-compilation.

### 2. Configure & Compile

```bash
cmake -B build-mingw -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=packaging/mingw-w64-x86_64.toolchain.cmake \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.0/mingw_64" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-mingw --parallel
```

With vcpkg, add `-DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic` and point `CMAKE_TOOLCHAIN_FILE` at vcpkg instead (vcpkg's toolchain file can chain-load the MinGW one via `VCPKG_CHAINLOAD_TOOLCHAIN_FILE`).

The output binary is placed at `build-mingw/webclip.exe`.

### 3. Deploying runtime dependencies

Cross-built executables still need `windeployqt` for bundling Qt DLLs/QML modules:

- Run the `windeployqt.exe` that ships inside the same Qt MinGW kit (under wine), or
- copy `build-mingw/webclip.exe` together with libcurl DLLs to a Windows machine and run `windeployqt` there.

For production releases, prefer the native Windows CI job (GitHub Actions runner), which handles deployment, dependency validation, and installer creation automatically.
