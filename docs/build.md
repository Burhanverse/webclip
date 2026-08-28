# Building WebClip

## Prerequisites

- **C++17 compiler** (`g++`, `clang++`, or MSVC 2022)
- **CMake** (>= 3.16) and **Ninja** (or Make)
- **Qt 6** (Core, Gui, Widgets, Svg, Quick, Qml)
- **libcurl**
- **Clipboard helper** (Linux only: `wl-clipboard` for Wayland, `xclip` for X11)

### Linux Dependencies

**Ubuntu / Debian:**
```bash
sudo apt-get update && sudo apt-get install -y \
  build-essential cmake ninja-build libcurl4-openssl-dev \
  qt6-base-dev qt6-declarative-dev qt6-tools-dev libqt6svg6-dev \
  qt6-wayland libqt6waylandclient6 wl-clipboard xclip
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake ninja curl qt6-base qt6-declarative qt6-wayland qt6-svg qt6-tools wl-clipboard xclip
```

**Fedora / RHEL:**
```bash
sudo dnf install gcc-c++ cmake ninja-build libcurl-devel \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland-devel \
  qt6-qtsvg-devel qt6-qttools-devel wl-clipboard xclip
```

---

## Linux Build

### 1. Compile

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The output binary is located at `build/webclip`.

### 2. Packaging (Optional)

- **All-in-one build script**:
  ```bash
  ./build_linux.sh
  ```
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

## Windows Build (MSVC)

### 1. Install Dependencies

Install `libcurl` using [vcpkg](https://github.com/microsoft/vcpkg):
```powershell
vcpkg install curl:x64-windows
```

### 2. Compile

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release --parallel
```

Output binary: `build\Release\webclip.exe`.

### 3. Package Portable Zip

```powershell
New-Item -ItemType Directory -Force -Path "deploy"
Copy-Item "build\Release\webclip.exe" -Destination "deploy\"
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
