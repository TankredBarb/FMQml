# FM

FM is a desktop file manager built with C++20, Qt 6 and QML.

The project is focused on a practical two-panel workflow, responsive directory
loading, useful previews, archive browsing, and a UI that stays predictable when
filesystems, removable drives and mounted images change under it.

## Status

FM is under active development. It is usable for day-to-day testing, but it is
not treated as a finished product yet. Some behavior may still change as the
navigation model, operations queue and platform integration are refined.

The current development target is Windows with MSVC and Qt 6. The codebase uses
Qt APIs where possible, but several features intentionally rely on Windows
integration.

## Features

- Two-panel file browsing with details, grid and brief views.
- Places sidebar with disks, common folders and tree navigation.
- Double-click or Enter opens items; single click selects them consistently
  across the main views and Places.
- Context menus for files, folders, drives and mounted ISO images.
- Background copy, move and delete operations with progress reporting.
- Undo and redo for supported file operations.
- Archive browsing through `archive://` paths, with optional 7-Zip support.
- Managed ISO mounting and eject flow.
- Quick Look-style preview for common file types.
- Properties and checksum dialogs.
- Path bar, search/filter field and command palette.
- Theme system with built-in schemes and JSON import/export.
- Native icons and thumbnail support where available.

## Keyboard Basics

Common shortcuts:

- `F9`: focus the sidebar for keyboard navigation.
- `Tab`: switch panels, unless sidebar tab trapping is active after `F9`.
- `Enter`: open the focused item.
- `Ctrl+L` or `Alt+D`: focus the path editor.
- `Ctrl+F`: focus search.
- `Ctrl+K` or `Ctrl+Shift+P`: open command palette.
- `F3`: toggle split view.
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
- On Windows: MSVC build tools or Visual Studio with the Desktop C++ workload.

Optional dependencies:

- Qt PDF module for built-in PDF previews.
- TagLib for richer audio metadata.
- `unofficial-bit7z` for archive integration.

The app can build without optional dependencies, but related features may be
disabled or fall back to simpler behavior.

## Build

Configure the project:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64"
```

Build:

```powershell
cmake --build build --config Debug --target fm
```

Run:

```powershell
.\build\Debug\fm.exe
```

If you are using a generated Qt Creator build directory, the executable path may
look different, for example:

```powershell
.\build\6_11_1_MS-Debug\fm.exe
```

For MSVC command-line builds, run the commands from a Visual Studio Developer
PowerShell or Developer Command Prompt so compiler and SDK paths are configured.

## Project Layout

- `src/`: C++ backend, models, controllers, filesystem providers and operations.
- `qml/`: QML application shell, views, dialogs and reusable UI components.
- `qml/components/app/`: application-level shortcuts, command registry and
  overlay coordination.
- `qml/components/filepanel/`: file panel-specific controls and delegates.
- `qml/components/preview/`: preview renderers.
- `qml/style/`: theme definitions.
- `docs/`: design notes and implementation plans.

## Development Notes

- Main entry point: `src/main.cpp`.
- Application shell: `qml/App.qml`.
- Shortcut wiring: `qml/components/app/AppShortcuts.qml`.
- Command palette data: `qml/components/app/CommandRegistry.qml`.
- File panel UI: `qml/components/FilePanel.qml`.
- Sidebar UI: `qml/components/Sidebar.qml`.
- Storage view UI: `qml/components/StorageView.qml`.

When changing keyboard behavior, keep a clear distinction between global
application shortcuts, active-panel shortcuts, and shortcuts that must only
apply when the file view itself is focused.

## License

See `LICENSE`.
