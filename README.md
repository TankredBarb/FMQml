# FM

FM is a desktop file manager built with C++20, Qt 6 and QML.

The project is focused on a practical two-panel workflow, responsive directory
loading, useful previews, archive browsing, and a UI that stays predictable when
filesystems, removable drives and mounted images change under it.

## Status

FM is under active development. It is usable for day-to-day testing, but it is
not treated as a finished product yet. Some behavior may still change as the
navigation model, operations queue and platform integration are refined.

The current development targets are Windows with MSVC and Linux with Qt 6.
Windows remains the more complete platform integration target. Linux support is
usable for active development and day-to-day testing, with native work already
started for local enumeration, mount filtering, storage type detection and
Google Drive credential persistence.

## Features

- Two-panel file browsing with details, grid and brief views, optional split
  layout, panel mirroring, sort/filter controls and persistent workspace state.
- Places sidebar with Favorites, disks, common folders, provider places and tree
  navigation.
- Double-click or Enter opens items; single click selects them consistently
  across the main views and Places.
- Context menus for files, folders, drives and mounted ISO images.
- Background copy, move, delete, duplicate, archive extraction and archive
  creation operations with progress reporting.
- Undo and redo for supported file operations.
- Archive browsing through `archive://` paths backed by required `bit7z`
  integration, including password prompts and nested archive preparation.
- Archive creation in 7z, zip, gzip, bzip2 and xz formats.
- Managed ISO mounting and eject flow where platform support is available.
- Quick Look popup and docked preview pane for folders, images, text, PDFs,
  audio metadata/playback, video/audio containers, fonts and supported book
  formats.
- Properties dialog with general metadata, access/security information,
  checksums, property export and checksum comparison.
- Disk usage analyzer and recursive file search tools for local folders.
- Path bar, `Ctrl+G` go-to-path command, local path autocomplete, search/filter
  field and command palette.
- Favorites virtual location for pinned paths, frequent folders and tags.
- Provider/plugin architecture with built-in local, archive, FTP, Google Drive,
  mock and Windows portable-device providers.
- Google Drive browsing and upload/download flows with persisted OAuth
  credentials.
- Storage view with capacity, filesystem, drive type and eject actions where
  supported.
- Theme system with built-in schemes, custom theme editor and JSON
  import/export.
- Settings import/export for workspace, panels, theme and preferences.
- Native icons and thumbnail support where available.

## Keyboard Basics

Common shortcuts:

- `F9`: focus the sidebar for keyboard navigation.
- `Tab`: switch panels, unless sidebar tab trapping is active after `F9`.
- `Enter`: open the focused item.
- `Ctrl+L` or `Alt+D`: focus the path editor.
- `Ctrl+G`: open the go-to-path command.
- `Ctrl+F`: focus search.
- `Ctrl+K` or `Ctrl+Shift+P`: open command palette.
- `Ctrl+P`: toggle the preview pane.
- `F3`: toggle split view.
- `F4`: mirror the active panel to the opposite panel.
- `F5`: refresh, or copy selection to the opposite panel when applicable.
- `F6`: move selection to the opposite panel.
- `F7` or `Ctrl+Shift+N`: create folder.
- `F2`: rename.
- `Space`: quick preview.
- `Delete`: delete selected items.

When the sidebar is focused, global navigation shortcuts such as `Ctrl+L`,
`Ctrl+F`, `Alt+Left`, `Alt+Right`, `Alt+Up`, `Ctrl+R`, and view switching remain
available. File-selection shortcuts such as `Delete`, `Space`, `F2`, `Ctrl+C`,
`Ctrl+X`, and `Ctrl+A` stay tied to the file view to avoid surprising actions.

## Requirements

- Qt 6.7 or newer.
- CMake 3.21 or newer.
- C++20 compiler.
- `bit7z` / `unofficial-bit7z` for archive integration, plus the 7-Zip runtime
  library used by bit7z (`7z.dll` on Windows, `7z.so` on Linux).
- Qt Multimedia C++ and QML modules.
- On Windows: MSVC build tools or Visual Studio with the Desktop C++ workload.
- On Linux: pkg-config and `libsecret-1` development files for Google Drive
  credential storage.

Optional dependencies:

- Qt PDF module for built-in PDF previews.
- TagLib for richer audio metadata.

Archive support and Qt Multimedia are required by the current build. The app can
build without optional dependencies, but related optional features may be
disabled or fall back to simpler behavior.

## Build

Use a Release build for normal development and testing. Keep the configured
build directory as `build`; duplicate platform-specific build folders are not
needed for the current workflow.

### Windows

Configure the project:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DQT_ENABLE_QML_DEBUG=OFF -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64"
```

`CMAKE_BUILD_TYPE=Release` applies to single-config generators such as Ninja.
If you are using a multi-config generator like Visual Studio or Qt Creator's
MSVC setup, that flag is ignored and the active config is selected at build
time.

Build:

```powershell
cmake --build build --config Release --target fm
```

For adequate rendering speed, it is strongly recommended to use a Release
configuration and explicitly disable QML debugging and profiling. Debug or
instrumented builds keep extra runtime overhead enabled and will render
noticeably slower.

Run:

```powershell
.\build\Release\fm.exe
```

If you are using a generated Qt Creator build directory, the executable path may
look different, for example:

```powershell
.\build\6_11_1_MS-Release\fm.exe
```

For MSVC command-line builds, run the commands from a Visual Studio Developer
PowerShell or Developer Command Prompt so compiler and SDK paths are configured.

### Linux

Install Qt 6, Qt Multimedia, pkg-config, libsecret development headers, `bit7z`
and the 7-Zip runtime package that provides `7z.so`. Package names vary by
distribution. On Arch-based systems the system `7zip` package plus a
vcpkg-provided `bit7z` setup are known to work.

Configure with the existing release build directory:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DQT_ENABLE_QML_DEBUG=OFF -DCMAKE_TOOLCHAIN_FILE="$HOME/.local/share/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

If `bit7z` is installed outside vcpkg, point CMake at it with
`CMAKE_PREFIX_PATH`, `unofficial-bit7z_DIR`, or `bit7z_DIR` instead of using the
vcpkg toolchain.

Build:

```bash
cmake --build build --parallel
```

Run:

```bash
./build/fm
```

Install for the current Linux build:

```bash
cmake --install build --prefix "$HOME/.local"
```

This currently installs:

- `fm` into `$HOME/.local/bin`
- `fm.desktop` into `$HOME/.local/share/applications`
- the app icon into `$HOME/.local/share/icons/hicolor/256x256/apps/fm.png`

Uninstall the files recorded by the current build directory's install manifest:

```bash
cmake --build build --target uninstall
```

`uninstall` is only as accurate as the matching `build` directory's
`install_manifest.txt`, so use it with the same build tree that performed the
install.

Linux support currently includes:

- Native local panel directory enumeration using `opendir`, `readdir` and
  `fstatat`.
- Native local path autocomplete for `Ctrl+L` and `Ctrl+G`.
- Filtered Places and tree roots for user-facing Linux mounts.
- Linux storage classification through sysfs for SSD, HDD, USB, optical and
  network labels.
- Native Linux directory watching through `inotify` for panel and tree refreshes.
- Baseline Linux native icons through freedesktop icon themes, Qt MIME icon
  names, `.desktop` file icon parsing and XDG special-folder theme icons.
- Google Drive OAuth persistence through Secret Service via `libsecret`.

Known Linux gaps:

- The MTP/portable-device provider is Windows-only for now.
- Some recursive operations, disk usage and folder sizing still use Qt fallback
  enumeration paths.
- Linux eject and ISO mounting are not at Windows parity yet.

## Project Layout

- `src/`: C++ backend, models, controllers, filesystem providers and operations.
- `qml/`: QML application shell, views, dialogs and reusable UI components.
- `qml/components/app/`: application-level shortcuts, command registry and
  overlay coordination.
- `qml/components/filepanel/`: file panel-specific controls and delegates.
- `qml/components/preview/`: preview renderers.
- `qml/style/`: theme definitions.
- `docs/`: design notes, parity plans and QA checklists.
  Start with `docs/near-term-work-plan.md` for the current feature plan.
- `suggest/`: implementation notes and working guidelines used during feature
  development.

## Development Notes

- Main entry point: `src/main.cpp`.
- Application shell: `qml/App.qml`.
- Shortcut wiring: `qml/components/app/AppShortcuts.qml`.
- Command palette data: `qml/components/app/CommandRegistry.qml`.
- File panel UI: `qml/components/FilePanel.qml`.
- Sidebar UI: `qml/components/Sidebar.qml`.
- Storage view UI: `qml/components/StorageView.qml`.
- Quick Look and preview pane UI: `qml/components/QuickLook.qml` and
  `qml/components/PreviewPane.qml`.
- Provider loading: `src/core/FileProviderPluginRegistry.cpp`.

When changing keyboard behavior, keep a clear distinction between global
application shortcuts, active-panel shortcuts, and shortcuts that must only
apply when the file view itself is focused.

## License

See `LICENSE`.
