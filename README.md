# WebClip (`webclip`)

A modern, standalone C++ & Qt 6 application for two-way automatic clipboard synchronization with **Gboard Web Clipboard** (from Gboard Patches).

Designed and styled following **Material Design 3 (MD3)** with dynamic light/dark theming, token-based styling, smooth state feedback layers, and full cross-platform support.

---

## Features

- **Material Design 3 (MD3) GUI**:
  - Full MD3 color token system with primary/secondary/tertiary container tiers and surface container levels.
  - Interactive state layers (hover, pressed, focus ripple/tint).
  - Dynamic Light, Dark, and System theme switching.
  - Real-time live clipboard history feed with origin badges (`📱 Phone`, `💻 Local`, `✏️ Manual`).
  - Text Composer page for instant push to phone.
  - Comprehensive Settings page with persistent configuration (`QSettings`).
- **Bidirectional Real-Time Sync**:
  - Real-time phone-to-computer sync via Server-Sent Events (SSE).
  - Background local clipboard watcher for automatic synchronization.
  - Intelligent echo & loop suppression.
  - Resilient auto-reconnect on network/Wi-Fi drops.
- **Cross-Platform**:
  - **Linux**: Supports both **Wayland** (`wl-clipboard` / `wl-copy` / `wl-paste`) and **X11** (`xclip` / `xsel`).
  - **Microsoft Windows**: Native Win32 Unicode clipboard API (`OpenClipboard`, `GetClipboardData`, `SetClipboardData`).
- **Dual Form Factor**:
  - `webclip`: Rich Qt 6 Quick / QML GUI application.
  - `webclip-cli`: Lightweight standalone headless CLI daemon for servers or minimal setups.

---

## Build Instructions

### Linux (Arch, Fedora, Ubuntu/Debian)

**Prerequisites:**
- C++17 compiler (`g++` or `clang++`)
- `cmake` (>= 3.16) and `make` or `ninja`
- `qt6-base-dev`, `qt6-declarative-dev`, `qt6-tools-dev` (or Arch `qt6-base`, `qt6-declarative`)
- `libcurl` (`libcurl4-openssl-dev` on Debian/Ubuntu, `libcurl-devel` on Fedora, `curl` on Arch)
- `wl-clipboard` (Wayland) or `xclip` / `xsel` (X11)

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

The resulting binaries:
- `build/webclip` (or `webclip.exe` on Windows): Unified standalone application (GUI with system tray & headless CLI daemon)

---

## Usage

### Desktop GUI (Default)

Launch `webclip` directly or install to `~/.local/bin`:

```bash
install -Dm755 build/webclip ~/.local/bin/webclip
webclip
```

- **Close to Tray**: Closing the window keeps WebClip running in the system tray and syncing clips in the background.
- **Connection**: Enter your phone's LAN IP / URL (e.g. `192.168.1.100` or `https://192.168.1.100:8081`) and pairing code, then click **Connect**.

### Headless CLI Daemon

The same `webclip` binary runs headless in the terminal when connection flags or `--headless` are supplied:

```bash
webclip --host 192.168.1.100 --code 1234 [options]
```

#### CLI Options

| Flag | Description | Default |
|---|---|---|
| `-h, --help` | Show usage options | — |
| `-v, --version` | Show version info | — |
| `--host <ip/url>` | Phone's LAN IP or URL (**required for CLI**) | — |
| `-p, --port <port>` | Web Clipboard HTTP/HTTPS port | `8080` (HTTP) / `8081` (HTTPS) |
| `-c, --code <code>` | 4-digit pairing code shown in Gboard | `""` |
| `--https` | Use HTTPS endpoint (typically port 8081) | `false` |
| `-k, --insecure` | Allow self-signed certificates when using HTTPS | `false` |
| `-i, --poll-interval <sec>` | Local clipboard watcher interval in seconds | `1.0` |
| `--client-id <id>` | Custom client identifier | Auto-generated |
| `--headless` | Explicitly run as headless daemon | `false` |

---

## Continuous Integration

A GitHub Actions workflow is provided in `.github/workflows/build.yml` providing automated compilation and release packaging across Linux (Ubuntu) and Windows (MSVC).

---

## License

MIT License.
