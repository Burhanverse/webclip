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

---

### 2. Compile

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
iscc.exe /DMyAppVersion="1.2.0" packaging\setup.iss
```

This outputs `webclip-setup-x64.exe`.
