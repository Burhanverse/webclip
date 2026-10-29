# Theming & Material You Support in WebClip

WebClip features a Material Design 3 (MD3) theming system that supports:
- Automatic system light/dark mode following desktop settings
- Manual brightness modes (Light, Dark, and AMOLED Pitch Black)
- Preset accent colors (Purple, Blue, Teal, Green, Orange, Red, Pink)
- Custom hex seed accent color with an interactive color picker
- **Material You Dynamic Theming (Matugen)** with instant hot reloading

---

## Setting Up Matugen (Dynamic Desktop Theming)

[Matugen](https://github.com/InioX/matugen) extracts color palettes from your wallpaper or arbitrary images and generates Material 3 color schemes.

### Installing Matugen

#### Linux
- **Arch Linux**:
  ```bash
  pacman -S matugen
  # or via AUR
  yay -S matugen-bin
  ```
- **Cargo**:
  ```bash
  cargo install matugen
  ```
- **Nix**:
  ```bash
  nix-env -iA nixpkgs.matugen
  ```

#### Windows
- **Cargo** (recommended):
  ```powershell
  winget install Rustlang.Rustup
  cargo install matugen
  ```
  Ensure `%USERPROFILE%\.cargo\bin` is present in your system `PATH`.
- **Pre-compiled Binary**:
  Download the latest Windows executable release from [InioX/matugen Releases](https://github.com/InioX/matugen/releases) and place it in a directory in your `PATH`.

---

### Default Theme Path

By default, WebClip monitors the following location for incoming theme updates:
- **Linux**: `~/.config/webclip/theme.json`
- **Windows**: `%LOCALAPPDATA%\webclip\theme.json` (e.g. `C:\Users\<YourUsername>\AppData\Local\webclip\theme.json`)

You can change this path in WebClip at any time by opening **Settings > Appearance > Theme JSON Path** and clicking **Browse**.

---

## Configuration & Usage

### Method 1: Using Matugen Templates (Recommended)

#### Linux

1. Copy the WebClip template into your Matugen templates directory:
   ```bash
   mkdir -p ~/.config/matugen/templates
   cp packaging/matugen/webclip-colors.json.template ~/.config/matugen/templates/webclip.json
   ```

2. Add the template to `~/.config/matugen/config.toml` (the `[config]` header is required by Matugen):
   ```toml
   [config]

   [templates.webclip]
   input_path = "~/.config/matugen/templates/webclip.json"
   output_path = "~/.config/webclip/theme.json"
   ```

3. Generate colors from your wallpaper:
   ```bash
   matugen image /path/to/your/wallpaper.png
   ```

#### Windows

1. Copy the WebClip template into your Matugen templates directory:
   - **PowerShell**:
     ```powershell
     New-Item -ItemType Directory -Force -Path "$env:APPDATA\InioX\matugen\templates"
     New-Item -ItemType Directory -Force -Path "$env:LOCALAPPDATA\webclip"
     Copy-Item "packaging\matugen\webclip-colors.json.template" -Destination "$env:APPDATA\InioX\matugen\templates\webclip.json"
     ```
   - **Command Prompt (CMD)**:
     ```cmd
     mkdir "%APPDATA%\InioX\matugen\templates"
     mkdir "%LOCALAPPDATA%\webclip"
     copy "packaging\matugen\webclip-colors.json.template" "%APPDATA%\InioX\matugen\templates\webclip.json"
     ```

2. Configure your Matugen `config.toml` at `%APPDATA%\InioX\matugen\config\config.toml`:
   > [!TIP]
   > In TOML configuration files on Windows, paths should use forward slashes (`/`) or escaped backslashes (`\\`).

   ```toml
   [config]

   [templates.webclip]
   input_path = "C:/Users/<YourUsername>/AppData/Roaming/InioX/matugen/templates/webclip.json"
   output_path = "C:/Users/<YourUsername>/AppData/Local/webclip/theme.json"
   ```

3. Generate colors from your wallpaper:
   ```powershell
   matugen image "C:\path\to\your\wallpaper.png"
   ```

WebClip will automatically detect the updated `theme.json` file (via `inotify` on Linux and `ReadDirectoryChangesW` on Windows) and hot-reload the UI without requiring an application restart.

---

### Method 2: Direct CLI Dump

If you prefer not to use templates, you can output raw JSON directly from Matugen:

#### Linux
```bash
mkdir -p ~/.config/webclip
matugen image /path/to/your/wallpaper.png --json hex > ~/.config/webclip/theme.json
```

#### Windows
- **PowerShell**:
  ```powershell
  New-Item -ItemType Directory -Force -Path "$env:LOCALAPPDATA\webclip"
  matugen image "C:\path\to\your\wallpaper.png" --json hex | Out-File -FilePath "$env:LOCALAPPDATA\webclip\theme.json" -Encoding utf8
  ```
- **Command Prompt (CMD)**:
  ```cmd
  if not exist "%LOCALAPPDATA%\webclip" mkdir "%LOCALAPPDATA%\webclip"
  matugen image "C:\path\to\your\wallpaper.png" --json hex > "%LOCALAPPDATA%\webclip\theme.json"
  ```

WebClip natively parses both raw Matugen JSON exports (`colors.<role>.<mode>.color`) and partitioned template structures (`{"light": {...}, "dark": {...}}`).

---

## Switching Modes in WebClip

In WebClip **Settings > Appearance**:
1. Click the **Material You** pill under **Accent Color**.
2. The Matugen configuration card will appear, displaying:
   - The active path to your `theme.json` file
   - File status (**Theme Loaded**, **Waiting for theme.json**, or **JSON Parse Error**)
   - A **Browse** button to select an alternate file
   - A **Reload** button to trigger a manual reload immediately
