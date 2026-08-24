# WebClip (`webclip`)

A lightweight, standalone C++ client for two-way automatic clipboard synchronization with **Gboard Web Clipboard** (from Gboard Patches).

## Features

- **Bidirectional Real-Time Sync**: Instant phone-to-computer sync via Server-Sent Events (SSE) and fast local-to-phone synchronization.
- **Cross-Platform**:
  - **Linux**: Supports both **Wayland** (`wl-clipboard` / `wl-copy` / `wl-paste`) and **X11** (`xclip` / `xsel`).
  - **Microsoft Windows**: Native Win32 Unicode clipboard API (`OpenClipboard`, `GetClipboardData`, `SetClipboardData`).
- **Echo & Loop Prevention**: Intelligent suppression of self-echoes and duplicate clipboard events.
- **Auto Reconnect**: Resilient network streaming with automatic reconnection on Wi-Fi drops.
- **HTTPS & Auth Support**: Support for pairing codes and HTTPS endpoints with optional self-signed certificate handling (`--insecure`).
- **Zero Heavy Dependencies**: Clean C++17 codebase with only `libcurl` for networking.

---

## Build Instructions

### Linux (Arch, Fedora, Debian/Ubuntu, etc.)

**Prerequisites:**
- C++17 compiler (`g++` or `clang++`)
- `cmake` (>= 3.16) and `make` or `ninja`
- `libcurl` (development package: `libcurl4-openssl-dev` on Debian/Ubuntu, `libcurl-devel` on Fedora, `curl` on Arch)
- `wl-clipboard` (for Wayland) or `xclip` / `xsel` (for X11)

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

The resulting binary `webclip` will be located in the `build/` directory.

### Windows (MSVC or MinGW)

**Prerequisites:**
- Visual Studio (with C++ Desktop workload) or MinGW-w64
- CMake
- `libcurl` (via vcpkg or pre-built binaries)

**Building with CMake & vcpkg / MSVC:**
```powershell
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[path-to-vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

---

## Usage

```bash
./webclip --host <PHONE_IP> --code <PAIRING_CODE> [options]
```

### Options

| Flag | Description | Default |
|---|---|---|
| `-h, --host <ip/host>` | Phone's LAN IP address (**required**) | — |
| `-p, --port <port>` | Web Clipboard HTTP port | `8080` |
| `-c, --code <code>` | 4-digit pairing code shown in Gboard | `""` |
| `--https` | Use HTTPS endpoint (typically port 8081) | `false` |
| `-k, --insecure` | Allow self-signed certificates when using HTTPS | `false` |
| `-i, --poll-interval <sec>` | Local clipboard watcher interval in seconds | `1.0` |
| `--client-id <id>` | Custom client identifier | Auto-generated |
| `--help` | Show usage options | — |

### Examples

**Standard HTTP Sync:**
```bash
./webclip --host 192.168.1.50 --code 5425
```

**Secure HTTPS Sync:**
```bash
./webclip --host 192.168.1.50 --port 8081 --https --insecure --code 5425
```

---

## License

MIT License.
