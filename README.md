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

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE) (GPL-3.0).
