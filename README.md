# WebClip

A lightweight desktop clipboard sync client for **Gboard Web Clipboard** (from [Shotgun Patches](https://github.com/Burhanverse/shotgun-patches)).

## Features

- **Bidirectional Sync**: Real-time phone-to-PC (via SSE) and automatic PC-to-phone synchronization.
- **Text & Images**: Sync text (with clickable URLs) and images (PNG, JPEG, WebP).
- **Desktop Integration**: System tray support, background sync, and native clipboard integration (Wayland, X11, Windows).
- **Material Design 3 UI**: Clean native interface with light/dark themes and clip history.
- **CLI Daemon Mode**: Headless support for servers or terminal-only setups.

## Installation

### Linux

Using the install script:
```bash
curl -fsSL https://raw.githubusercontent.com/Burhanverse/webclip/main/packaging/install.sh | bash
```

Or grab the standalone **AppImage** or **tar.gz** bundle from [Releases](https://github.com/Burhanverse/webclip/releases/latest).

To uninstall:
```bash
curl -fsSL https://raw.githubusercontent.com/Burhanverse/webclip/main/packaging/uninstall.sh | bash
```

### Windows

Download the installer (`webclip-setup-x64.exe`) or portable zip from [Releases](https://github.com/Burhanverse/webclip/releases/latest).

## Usage

### GUI
Launch `webclip`, enter your phone's IP address and pairing code, and click **Connect**. Closing or minimizing the window keeps it running in the system tray.

### CLI / Headless
```bash
webclip --host <phone-ip> --code <pairing-code> [options]
```

| Option | Description | Default |
|---|---|---|
| `-h, --help` | Show usage options | — |
| `-v, --version` | Show version | — |
| `--host <ip/url>` | Phone IP address or URL | — |
| `-p, --port <port>` | Port number | `8080` (HTTP) / `8081` (HTTPS) |
| `-c, --code <code>` | Pairing code | `""` |
| `--https` | Use HTTPS | `false` |
| `-k, --insecure` | Allow self-signed certificates | `false` |
| `-i, --poll-interval <sec>` | Polling interval | `1.0` |
| `--headless` | Run in background without GUI | `false` |

## Building

See [docs/build.md](docs/build.md) for prerequisites and packaging details.

```bash
# Linux
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Windows (MSVC + vcpkg)
vcpkg install curl:x64-windows
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release --parallel
```

## Platform Notes

- **Linux (Wayland)**: Requires `wl-clipboard`.
- **Linux (X11)**: Requires `xclip` or `xsel`.
- **GNOME**: Ensure the AppIndicator extension is installed for system tray support.

## License

[GPL-3.0](LICENSE)
