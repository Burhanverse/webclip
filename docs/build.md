# Building

## Dependencies

- **Arch Linux:**
  ```bash
  sudo pacman -S clang lld cmake ninja curl qt6-base qt6-declarative qt6-wayland qt6-svg qt6-tools wl-clipboard xclip
  ```
- **Ubuntu / Debian:**
  ```bash
  sudo apt update && sudo apt install -y clang lld build-essential cmake ninja-build libcurl4-openssl-dev qt6-base-dev qt6-declarative-dev qt6-tools-dev libqt6svg6-dev qt6-wayland libqt6waylandclient6 wl-clipboard xclip
  ```
- **Fedora:**
  ```bash
  sudo dnf install clang lld gcc-c++ cmake ninja-build libcurl-devel qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland-devel qt6-qtsvg-devel qt6-qttools-devel wl-clipboard xclip
  ```
- **Windows:**
  - LLVM (Clang + lld), CMake, Ninja, Qt 6.8+ (MSVC 2022 x64)
  - `vcpkg install curl:x64-windows`

---

## Linux

### Build (Clang + Ninja + lld)
```bash
cmake --preset linux-clang
cmake --build --preset linux-clang
```
*(Or use `./build_linux.sh --bin-only`)*

### Legacy Build (GCC)
```bash
cmake --preset linux-gcc
cmake --build --preset linux-gcc
```
*(Or `./build_linux.sh --legacy --bin-only`)*

### Packaging
```bash
./build_linux.sh                  # AppImage + tarball -> dist/
./packaging/build_appimage.sh     # AppImage only
./packaging/pack_linux_tarball.sh # Tarball only
./packaging/install.sh            # Install to ~/.local
./packaging/uninstall.sh          # Uninstall
```

---

## Windows

### Build (clang-cl + Ninja + lld-link)
```powershell
cmake --preset windows-clang-cl -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build --preset windows-clang-cl
```

### Legacy Build (MSVC)
```powershell
cmake --preset windows-msvc -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build --preset windows-msvc
```

### Package
```powershell
mkdir deploy
Copy-Item build\webclip.exe deploy\
Copy-Item "$env:VCPKG_INSTALLATION_ROOT\installed\x64-windows\bin\*.dll" deploy\
windeployqt deploy\webclip.exe --qmldir src\gui\qml --no-translations --no-opengl-sw --compiler-runtime
Compress-Archive deploy\* webclip-windows-x64-portable.zip

# Optional Inno Setup installer
iscc.exe /DMyAppVersion="1.7.0" packaging\setup.iss
```

---

## Cross-Compile (Linux -> MinGW)
```bash
cmake -B build-mingw -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=packaging/mingw-w64-x86_64.toolchain.cmake \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.0/mingw_64" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw --parallel
```
