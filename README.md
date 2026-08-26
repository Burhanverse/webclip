# WebClip

WebClip is a lightweight, standalone C++ and Qt 6 application for bidirectional clipboard synchronization with **Gboard Web Clipboard** (from [Shotgun Patches](https://github.com/Burhanverse/shotgun-patches)).

---

## Features

- **Bidirectional Clipboard Sync**:
  - Real-time phone-to-PC synchronization via Server-Sent Events (SSE).
  - Background OS clipboard monitor for automatic PC-to-phone synchronization.
  - Multi-tier deduplication (FNV-1a hashing, perceptual pixel fingerprinting, and monotonic clip IDs) to prevent echo loops.
- **Text and Image Support**:
  - Text synchronization with automatic URL linkification and clickable links.
  - Image clipboard synchronization supporting PNG, JPEG, and WebP formats.
  - Drag-and-drop image sharing and image lightbox preview with save and copy actions.
- **Material Design 3 Interface**:
  - Dynamic light and dark mode with customizable accent colors.
  - Conversation-style clip history feed with inline actions (Copy, Open Link, Save Image, Delete).
  - System tray integration with background sync.
- **Cross-Platform Support**:
  - **Linux**: Wayland (`wl-clipboard`) and X11 (`xclip` / `xsel`).
  - **Windows**: Native Win32 Unicode and DIB/PNG clipboard APIs with working set memory compaction.
- **Headless CLI Daemon**:
  - The same binary can run without a graphical environment on servers or minimal setups.

---

## Installation

### Linux (One-Line Installer)

Install the latest release with desktop integration with a single command:

```bash
curl -fsSL https://raw.githubusercontent.com/Burhanverse/webclip/main/packaging/install.sh | bash
```

To install the standalone portable AppImage bundle instead:

```bash
curl -fsSL https://raw.githubusercontent.com/Burhanverse/webclip/main/packaging/install.sh | bash -s -- --appimage
```

#### Uninstall

```bash
curl -fsSL https://raw.githubusercontent.com/Burhanverse/webclip/main/packaging/uninstall.sh | bash
```

### Windows

Download and run `webclip-setup-x64.exe` installer or extract `webclip-windows-x64-portable.zip` from [GitHub Releases](https://github.com/Burhanverse/webclip/releases/latest).

---

## Build Instructions

For prerequisites and packaging details, see [docs/build.md](docs/build.md).

### Linux

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Windows (MSVC + vcpkg)

```powershell
vcpkg install curl:x64-windows
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release --parallel
```

---

## Usage

### GUI Mode (Default)

```bash
webclip
```

1. Enter your phone's local IP address or URL and pairing code.
2. Click **Connect**.
3. Minimizing or closing the window keeps WebClip running in the system tray.

### CLI Mode

```bash
webclip --host <phone-ip> --code <pairing-code> [options]
```

#### Options

| Option | Description | Default |
|---|---|---|
| `-h, --help` | Show usage options | — |
| `-v, --version` | Show version information | — |
| `--host <ip/url>` | Phone IP address or URL | — |
| `-p, --port <port>` | Port number | `8080` (HTTP) / `8081` (HTTPS) |
| `-c, --code <code>` | Pairing code | `""` |
| `--https` | Use HTTPS endpoint | `false` |
| `-k, --insecure` | Allow self-signed certificates | `false` |
| `-i, --poll-interval <sec>` | Polling interval in seconds | `1.0` |
| `--client-id <id>` | Custom client identifier | Auto-generated |
| `--headless` | Run in headless daemon mode | `false` |

---

## Platform Notes

- **Linux (Wayland)**: Requires `wl-clipboard` (`wl-copy` / `wl-paste`). The AppImage bundles these helpers when they are present at build time.
- **Linux (X11)**: Requires `xclip` or `xsel`. Rounded transparent window corners require a running compositor.
- **GNOME**: The system tray icon requires the *AppIndicator* extension. Tray balloon notifications may be unavailable without a notification daemon.
- **Windows**: Image clipboard interop uses GDI+ to convert between PNG and DIB formats, so images copy and paste correctly with both modern and legacy applications. Toast notifications from the portable build may require installing via the setup executable.

---

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE) (GPL-3.0).
