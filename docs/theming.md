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

### Default Theme Path
By default, WebClip watches for a JSON theme at:
- **Linux**: `~/.config/webclip/theme.json`
- **Windows**: `%APPDATA%/webclip/theme.json`

You can change this path in WebClip by opening **Settings > Appearance > Theme JSON Path** and clicking **Browse**.

---

### Method 1: Using Matugen Templates (Recommended)

1. Copy the WebClip template into your Matugen templates directory:
   ```bash
   mkdir -p ~/.config/matugen/templates
   cp packaging/matugen/webclip-colors.json.template ~/.config/matugen/templates/webclip.json
   ```

2. Add the template to your `~/.config/matugen/config.toml` (note that the `[config]` header is required by Matugen):
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

WebClip will automatically detect the updated `theme.json` file via inotify and hot-reload the UI without requiring an application restart.

---

### Method 2: Direct CLI Dump

If you prefer not to use templates, you can dump raw JSON directly from Matugen:
```bash
mkdir -p ~/.config/webclip
matugen image /path/to/your/wallpaper.png --json hex > ~/.config/webclip/theme.json
```
WebClip's parser natively understands Matugen's raw JSON structure (`colors.<role>.<mode>.color`) as well as partitioned template structures.

---

## Switching Modes in WebClip

In WebClip **Settings > Appearance**:
1. Click the **Material You** pill under **Accent Color**.
2. The Matugen configuration row will appear, displaying:
   - The active path to your `theme.json` file
   - File status (**Theme Loaded**, **Waiting for theme.json**, or **JSON Parse Error**)
   - A **Browse** button to select an alternate file
   - A **Reload** button to trigger a manual reload immediately

