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
Both platforms share the panel, operation, archive, preview and provider
architecture. Platform-specific integrations are enabled when their native
dependencies are available.

## Features

- Two-panel file browsing with details, grid and brief views, optional split
  layout, panel mirroring, sort/filter controls and persistent workspace state.
- Places sidebar with Favorites, disks, common folders, provider places and tree
  navigation.
- Double-click or Enter opens items; single click selects them consistently
  across the main views and Places.
- Context menus for files, folders, drives and mounted ISO images.
- Background copy, move, delete, duplicate, archive extraction and archive
  creation operations with an operation drawer, progress, speed, ETA and
  cancellation.
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
- Disk usage analyzer, recursive content/file search, read-only folder compare
  and batch rename tools.
- Path bar, `Ctrl+G` go-to-path command, local path autocomplete, search/filter
  field and command palette.
- Favorites virtual location for pinned paths, frequent folders and tags.
- Provider/plugin architecture for local files, archives, FTP, Google Drive,
  Instagram and optional MEGA and Telegram integrations.
- Google Drive and MEGA browsing and transfer flows, Telegram shared-file
  browsing, Instagram media access and persisted provider sessions where the
  required plugin is available.
- Portable media browsing and copy-to-local support through Windows WPD or KDE
  KIO/MTP on Linux; the portable provider is intentionally read-only.
- Open With application discovery, per-file preferences and platform launch
  integration.
- Linux administrator mode for supported local operations through a separately
  installed polkit helper.
- Storage view with capacity, filesystem, drive type and eject actions where
  supported.
- Theme system with built-in schemes, custom theme editor and JSON
  import/export.
- Settings import/export for workspace, panels, theme and preferences.
- Native icons and thumbnail support where available.

## Keyboard Basics

Common shortcuts:

- `F1`: open the in-app workflow and shortcut reference.
- `F9`: focus the sidebar for keyboard navigation.
- `Tab`: switch panels, unless sidebar tab trapping is active after `F9`.
- `Enter`: open the focused item.
- `Ctrl+L` or `Alt+D`: focus the path editor.
- `Ctrl+G`: open the go-to-path command.
- `Ctrl+B`: open Favorites.
- `Ctrl+F`: focus search.
- `Ctrl+K` or `Ctrl+Shift+P`: open command palette.
- `Ctrl+P`: toggle the preview pane.
- `F3`: toggle split view.
- `F4`: mirror the active panel to the opposite panel.
- `F5`: copy selection to the opposite panel.
- `Shift+F5`: move selection to the opposite panel.
- `F7` or `Ctrl+Shift+N`: create folder.
- `F2`: rename.
- `Space`: quick preview.
- `Delete`: delete selected items.
- `Shift+Delete`: request permanent deletion without using the trash.
- `Ctrl+I`: invert the current selection.
- `Ctrl+Shift+F`: recursively search under the active folder.

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
- FFmpeg libraries for generated video thumbnails on Linux.
- KF6 KIO for the read-only Linux MTP/portable-device provider.
- MEGA SDK and TDLib for the optional MEGA and Telegram provider plugins.

Archive support and Qt Multimedia are required by the current build. The app can
build without optional dependencies, but related optional features may be
disabled or fall back to simpler behavior.

## Build

Use a Release build for normal development and testing. Keep the configured
build directory as `build`; duplicate platform-specific build folders are not
needed for the current workflow.

### Windows

Configure the project from a Visual Studio Developer PowerShell. FM expects Qt
from the official MSVC Qt installation and uses the vcpkg toolchain for C/C++
dependencies such as bit7z, TagLib and optional provider SDKs:

```powershell
$env:VCPKG_ROOT = "C:/vcpkg"

cmake -S . -B build `
  -DCMAKE_BUILD_TYPE=Release `
  -DQT_ENABLE_QML_DEBUG=OFF `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
```

`CMAKE_BUILD_TYPE=Release` applies to single-config generators such as Ninja.
If you are using a multi-config generator like Visual Studio or Qt Creator's
MSVC setup, that flag is ignored and the active config is selected at build
time. Keep `--config Release` on the build command in that case.

The vcpkg toolchain must be selected during the first configure of a build
directory. If `build` was previously configured without it, remove that build
directory or use a fresh one before running the command above. Setting
`VCPKG_ROOT` is also useful to FM's Windows deployment rules, which copy the
required vcpkg runtime DLLs from the active triplet into the application output
directory.

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

For MSVC command-line builds, keep using a Visual Studio Developer PowerShell or
Developer Command Prompt so compiler and Windows SDK paths are configured.

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
cmake --build build -j 12
```

Run:

```bash
./build/fm
```

Install or refresh the configured Linux build, including the administrator
helper, polkit policy, desktop entry, icon and provider plugins:

```bash
sudo ./install.sh
```

`install.sh` builds as the invoking user, removes files recorded by the previous
install manifest, and then installs the current build with elevated privileges.
To uninstall without reinstalling:

```bash
sudo make -C build uninstall
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
- Native icons through freedesktop icon themes, Qt MIME icon
  names, `.desktop` file icon parsing and XDG special-folder theme icons.
- Google Drive OAuth persistence through Secret Service via `libsecret`.
- Shared storage-topology policy for responsive local copying and archive
  extraction across same-mount, independent-device and conservative routes.
- Linux administrator sessions through polkit for supported local file and
  permission operations.
- XDG desktop application discovery and portal-aware Open With handling.
- KDE KIO/MTP portable-device browsing, metadata, previews and copy-to-local
  transfers when KF6 KIO is installed.
- FFmpeg-backed video thumbnails when the required libraries are present.

Known Linux limitations:

- The KIO/MTP portable-device provider is read-only; writing, rename and delete
  on the device are intentionally unavailable.
- Optional MEGA, Telegram, video-thumbnail and portable-device features depend
  on their SDK or desktop libraries being present at configure time.
- Some platform-specific shell, removable-media and launch behavior still
  differs from Windows.

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
