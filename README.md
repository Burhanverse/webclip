<p align="center">
  <img src="src/gui/resources/icons/webclip.svg" alt="WebClip" width="128">
</p>

<h1 align="center">WebClip</h1>

<p align="center">
  A lightweight desktop clipboard sync client for <b>Gboard Web Clipboard</b> (from <a href="https://github.com/Burhanverse/shotgun-patches">Shotgun Patches</a>).
</p>

## Features

- **Bidirectional Sync**: Real-time phone-to-PC (SSE) and automatic PC-to-phone synchronization.
- **Text & Images**: Sync text (with clickable URLs) and images (PNG, JPEG, WebP).
- **Desktop Integration**: System tray support, background sync, and native clipboard integration (Wayland, X11, Windows).
- **Material Design 3 UI**: Clean native interface with light/dark themes and clip history.
- **CLI Mode**: Direct terminal/headless sync support.

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

### CLI
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
| `-i, --poll-interval <sec>` | Polling interval in seconds | `1.0` |
| `--client-id <id>` | Custom client identifier | auto |

## Building

See [docs/build.md](docs/build.md) for full setup.

```bash
# Linux (Clang + Ninja + lld)
cmake --preset linux-clang
cmake --build --preset linux-clang

# Or run the script
./build_linux.sh --bin-only
```

## Platform Notes

- **Linux (Wayland)**: Requires `wl-clipboard`.
- **Linux (X11)**: Requires `xclip` or `xsel`.
- **GNOME**: Ensure AppIndicator extension is installed for system tray support.

## License

[GPL-3.0](LICENSE)
