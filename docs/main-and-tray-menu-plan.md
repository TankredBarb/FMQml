# Main Menu and Tray Menu Improvement Plan

## 1. Goal

Turn the application main menu and system tray menu from minimal utility menus into two clear, useful entry points that share product language and semantic state without pretending that QML `FmMenu` and native `QMenu` can render identically.

The work has two separate outcomes:

- the main menu becomes the compact navigation and tools hub for the visible application;
- the tray menu becomes a small remote-control and status surface for a visible, minimized, or hidden application.

This document covers research, proposed contents, visual direction, architecture, implementation phases, and acceptance criteria. It does not authorize implementation by itself.

## 2. Current implementation

### 2.1 Main menu

The main toolbar owns a QML `FmMenu` in `qml/components/MainToolbar.qml`.

Current actions:

1. Settings (`Ctrl+,`)
2. Help (`F1`)
3. separator
4. Quit (`Ctrl+Q`)

The menu already uses production `FmMenu`, `FmMenuItem`, and `FmMenuSeparator` controls. Its surface, item hover/press states, icon recoloring, typography, and border therefore follow the active FM theme through the native QPainter-backed framework visuals.

The weakness is primarily information architecture and density, not a missing themed base. It exposes only three destinations even though the application already has a command registry with navigation, view, tools, settings, theme, administration, and help actions.

### 2.2 Tray menu

`SystemTrayController` owns a native `QSystemTrayIcon` and `QMenu`.

Current actions:

1. Show
2. Hide
3. Options
4. separator
5. Exit

Current state behavior:

- Show is enabled only while the window is hidden or minimized;
- Hide is enabled only while the window is visible and not minimized;
- Options shows the window and opens Settings through a QML signal connection;
- Exit follows the explicit force-quit path;
- a click or double-click on the tray icon shows the window;
- background operations may keep a minimized taskbar window alive;
- menu colors and action icons are already regenerated from `ThemeController` on theme changes.

The native menu currently uses `QPalette`, a stylesheet, and pre-rendered themed SVG icons. This works when Qt owns and paints the popup. On Linux desktops that export tray menus through StatusNotifier/AppIndicator, the desktop shell may render the menu itself and ignore some or all widget styling. Windows and different Linux tray hosts must therefore be treated as separate visual environments.

## 3. Product roles

### 3.1 Main menu role

The main menu answers: “What important application-level action or destination do I need while working in FM?”

It may contain:

- stable workspace navigation;
- view configuration;
- tools that operate on the current workspace;
- application customization and management;
- help and application lifecycle actions.

It should not become a second command palette. The command palette remains the exhaustive and searchable action surface; the menu contains a deliberately curated set.

### 3.2 Tray menu role

The tray menu answers: “What do I need while FM is hidden, minimized, or in the background?”

It may contain:

- one clear window visibility action;
- a small number of safe destinations that first restore the window;
- background-operation status;
- Settings;
- Quit.

It should not contain selection-dependent file actions, destructive operations, panel layout details, theme editing, plugin management, or developer tools. Those actions require visible context or are too uncommon for a tray surface.

## 4. Proposed main menu

The initial proposal is intentionally bounded. It uses existing capabilities and does not invent new workflows.

### 4.1 Primary section

1. **Command Palette…** (`Ctrl+K`)
   - icon: search or command icon;
   - opens the existing command palette;
   - makes the exhaustive action surface discoverable without overloading the menu.

2. **Favorites** (`Ctrl+B`)
   - icon: star;
   - navigates the active panel to `favorites://`;
   - enabled when workspace commands are available.

### 4.2 Workspace and view section

Use a **View** submenu if submenu behavior is visually accepted in `FmMenu`. Otherwise keep only the three highest-value toggles at the top level.

Proposed View items:

- **Split Panels** (`F3`), checked when split view is enabled;
- **Preview Pane**, checked when the preview pane is visible;
- **Show Hidden Files** (`Ctrl+H`), checked from the active panel state;
- separator;
- **Theme…**, showing the active theme name as secondary state if supported, opening the existing theme selector.

Do not expose Details/Grid/Brief in the first pass. Those controls already have a direct toolbar location, and including every view command would turn the menu into a duplicate command palette.

### 4.3 Tools section

Use a **Tools** submenu:

- **File Search…** — enabled only when the active path supports local search;
- **Disk Usage…** — enabled only when the active path supports analysis;
- **Compare Folders…** — enabled only when both panel paths are usable.

Each item must reuse the same eligibility rules as `CommandRegistry`, rather than adding simplified menu-only checks.

Do not add checksums, batch rename, archive, or properties here. Those are selection/context actions and belong to the file context menu or command palette.

### 4.4 Application section

- **Plugins…** — opens Plugin Manager;
- **Settings…** (`Ctrl+,`);
- **Help and Shortcuts** (`F1`);
- separator;
- **Quit FM** (`Ctrl+Q`), destructive styling.

Developer-only Debug Information and UI Lab remain hidden shortcuts and must not appear in the ordinary main menu.

### 4.5 Proposed main-menu order

```text
Command Palette…                 Ctrl+K
Favorites                       Ctrl+B
--------------------------------------
View                            >
Tools                           >
--------------------------------------
Plugins…
Settings…                       Ctrl+,
Help and Shortcuts              F1
--------------------------------------
Quit FM                         Ctrl+Q
```

This is nine top-level rows including separators and submenus, compact enough for a frequent menu while exposing the major application areas.

## 5. Proposed tray menu

### 5.1 Replace Show plus Hide with one dynamic action

The first action should always be enabled and describe the immediate result:

- **Open FM** while hidden or minimized;
- **Hide FM** while visible.

Using two mutually exclusive disabled/enabled rows adds noise and makes the most important action less obvious.

Clicking the tray icon should continue to restore the application. A later acceptance decision may make a single click toggle visibility, but that behavior should not change in the visual/menu pass.

### 5.2 Safe quick destinations

Proposed entries:

- **Favorites** — restore the window, then navigate the active panel to Favorites;
- **File Search…** — restore the window, then open File Search for the active eligible path;
- separator.

If hidden-window testing shows that the last active path can become unavailable or stale, File Search must be disabled rather than guessing a path. Favorites remains safe because it is a virtual root.

Do not add Recent Files until the application has a real privacy-reviewed recent-files model. Do not infer it from command usage or filesystem history.

### 5.3 Background operation status

When no operation is active, no status row is shown in the first pass.

When an operation is active or has an error, show one disabled informational action above Settings:

- `File operations: running`
- `File operations: 42%` when a stable aggregate percentage is already available;
- `File operations: attention required` for a current error/conflict state.

The row may use progress, warning, or error icon color. It is informational in Phase 1. Do not make it open the Operations Drawer until the application has an explicit public action that restores the window and focuses that drawer.

Do not add Pause, Resume, or Cancel All unless those operations have a safe queue-wide contract. Menu design must not invent operation semantics.

### 5.4 Application actions

- **Settings…** — restore the window, then open Settings;
- separator;
- **Quit FM** — explicit process exit, danger icon where the host preserves icon colors.

### 5.5 Proposed tray-menu order

```text
Open FM / Hide FM
--------------------------------------
Favorites
File Search…
--------------------------------------
[File operations: running / 42% / attention required]
Settings…
--------------------------------------
Quit FM
```

The operation row is conditional. The menu stays short when FM is idle.

## 6. Shared language and state

The two menus should share:

- action labels: `Settings…`, `Favorites`, `File Search…`, `Quit FM`;
- semantic icons and icon tones;
- enabled-state rules for shared actions;
- terminology for operation state;
- explicit ellipsis for actions that open another surface and require more interaction.

They should not share a universal menu component or a single identical item model. The two menus have different jobs, rendering technologies, and action sets.

Where eligibility logic already exists in `CommandRegistry`, extract or expose only the specific reusable policy needed by both consumers. Do not duplicate path/provider/admin checks in three places.

## 7. Main-menu visual direction

### 7.1 Preserve the accepted framework base

Keep `FmMenu`, `FmMenuItem`, and `FmMenuSeparator`. They already provide:

- active-theme surface and border tokens;
- QPainter-rendered menu and item visuals;
- hover and press states;
- semantic icon recoloring;
- disabled state;
- shortcut column;
- selected/active item state.

Do not replace the menu with a custom dialog or a universal popup abstraction.

### 7.2 Richness without decoration overload

The richer visual pass should use:

- a width around 270–300 px, validated against long localized labels;
- consistent 16 px semantic icons;
- a stable shortcut column;
- restrained separators between functional groups;
- check/active indication for toggle items;
- submenu arrows aligned as a separate trailing column;
- current theme name in the Theme row or submenu;
- semantic danger treatment only for Quit.

Avoid:

- a large hero/header inside the menu;
- gradients or glass effects that differ from `FmMenu`;
- descriptions under every item;
- badges on ordinary actions;
- more than two submenu levels.

### 7.3 Framework gaps to prototype

Before implementation, use UI Lab to prototype these missing/uncertain states:

1. `FmMenuItem` with a check/active indicator that is distinct from its icon;
2. `FmMenuItem` with a submenu arrow and keyboard navigation;
3. a menu containing two submenus, separators, disabled tools, and a destructive final item;
4. narrow and long-label geometry;
5. light and dark themes.

Extend production controls only when the prototype proves a real missing state. Do not add writable hover/pressed test properties.

## 8. Tray-menu visual direction

### 8.1 Supported baseline

Keep native `QMenu` and `QAction`. This preserves platform integration, keyboard navigation, accessibility, and StatusNotifier/AppIndicator compatibility.

The baseline visual pass should refine the existing theme bridge:

- use the theme's actual `menuSurface`, `menuBorder`, `menuItemHover`, `menuItemPressed`, text, secondary text, and danger colors;
- align padding, icon size, separator margins, and minimum row height with `FmMenu` as closely as the native host allows;
- use the same icon sources and semantic tones as the QML menu;
- update palette, stylesheet, and icons on every `ThemeController::themeChanged`;
- ensure the active theme name is not baked into cached icons or stale palettes.

The existing implementation already does part of this work. The first code phase should be a token-by-token audit rather than a rewrite.

### 8.2 Native-host limitation

The plan must accept three rendering outcomes:

1. **Qt-owned widget popup:** palette, stylesheet, spacing, borders, and icons are under application control.
2. **Desktop-host-rendered StatusNotifier menu:** action structure, labels, enabled state, check state, separators, and icons are reliable; exact background, radius, padding, and hover appearance may be controlled by the desktop shell.
3. **Platform-native menu behavior:** some icon colors or stylesheet details may be normalized by the OS.

“Follows the FM theme” therefore means:

- exact FM tokens where Qt paints the menu;
- semantically correct icons and states plus a native coherent fallback where the host paints it.

It must not mean replacing the tray popup with a frameless QML window positioned near the tray icon. Tray-icon geometry is not consistently available, focus/dismiss behavior becomes fragile, and such a popup would regress native integration.

### 8.3 Optional rich native prototype

After the baseline is accepted, a disabled status header implemented with a normal `QAction` may be tested. Avoid `QWidgetAction` in the baseline because custom widgets may not export through StatusNotifier and can degrade keyboard/accessibility behavior.

A custom `QMenu` paint implementation or `QProxyStyle` is also a prototype-only option. It can improve Qt-owned rendering but cannot force a desktop shell to honor it. Adopt it only if it materially improves Windows and direct Qt menus without adding a second complex theme engine.

## 9. Architecture

### 9.1 Main menu

Keep ownership in `MainToolbar.qml`, but move the growing menu body into a focused component such as:

```text
qml/components/app/AppMainMenu.qml
```

Suggested inputs:

- `appRoot`;
- active panel/workspace state required for enabled and checked states;
- existing action callbacks or command lookup;
- current theme name.

The component remains a production `FmMenu`. It must not own application business logic.

Prefer invoking existing command descriptors by stable command id for actions already represented in `CommandRegistry`. Add a small `runCommand(commandId)`/`commandEnabled(commandId)` access path if needed. Do not copy command validation functions into the menu.

### 9.2 Tray menu

Keep native ownership in `SystemTrayController`.

Add explicit semantic signals for actions requiring QML navigation, for example:

```cpp
void favoritesRequested();
void fileSearchRequested();
void optionsRequested();
void exitRequested();
```

`App.qml` handles each request by restoring the window first and then scheduling the existing application action with `Qt.callLater`, as Settings already does.

For eligibility, pass only the minimal stable state into `SystemTrayController`, or expose a small application action bridge. Do not give the native controller direct ownership of panel controllers or QML objects merely to build the menu.

Operation status remains owned by `OperationQueue`; connect to the smallest set of existing signals needed to update the status action.

### 9.3 Action lifetime and updates

- construct tray `QAction` objects once;
- update text, icon, visibility, enabled state, and checked state in `updateMenuState()`;
- update state immediately before the tray menu is shown, in addition to signal-driven updates;
- keep theme/icon regeneration in `applyTheme()`;
- do not rebuild the entire `QMenu` on every state change;
- close any open submenu before destroying or replacing its model.

## 10. Implementation phases

### Phase 0 — UI Lab and native reference capture

1. Add a Main Menu scenario to UI Lab using production `FmMenu` items.
2. Prototype proposed width, group order, submenus, check states, disabled tools, long labels, and Quit styling.
3. Capture current tray menu screenshots on at least KDE Plasma/Linux and Windows if available.
4. Record whether the Linux menu is Qt-painted or host-painted in each tested environment.

Exit criterion: main-menu composition is visually accepted and native tray constraints are observed rather than assumed.

Current prototype status:

- `menus/main-menu-proposal` is implemented on the UI Lab Combo, Menu, and Popup page;
- it uses a 290 px production `FmMenu`, the proposed action grouping, View and Tools submenus, active examples, a disabled tool, shortcuts, semantic icons, and destructive Quit styling;
- source inspection confirms that `FmMenuItem` currently has no dedicated checkmark column: `active` changes emphasis but is not an independent checked indicator;
- nested `Menu` entries are created by Qt's submenu mechanism, while `FmMenuItem` owns a custom content layout; consistent submenu arrow, icon, hover, and keyboard presentation therefore require live visual acceptance and may need a focused framework extension;
- exact tray reference capture remains pending on the target desktop hosts.

### Phase 1 — Main menu information architecture

1. Extract the menu body from `MainToolbar.qml` into `AppMainMenu.qml` only if the final body is large enough to justify the file.
2. Add Command Palette and Favorites.
3. Add View and Tools submenus using existing command policies.
4. Add Plugins, Settings, Help, and Quit.
5. Preserve keyboard navigation, Escape, outside-click closing, submenu closing, and shortcut display.

Exit criterion: all items invoke the same paths as their existing shortcuts/command palette actions, and checked/enabled state updates while the menu is reopened.

Current implementation status:

- the accepted composition is wired into the production toolbar through `AppMainMenu.qml`;
- actions delegate to the existing `App.qml` routes for Command Palette, Favorites, View, Tools, Plugins, Settings, Help, and Quit;
- split, preview, hidden-files, and tool eligibility state is read from the active workspace rather than stored by the menu;
- live visual and interaction acceptance of the production route remains pending.

### Phase 2 — Tray content and state model

1. Replace separate Show and Hide actions with one dynamic visibility action.
2. Add Favorites and File Search requests.
3. Add the conditional operation-status action using existing queue state only.
4. Rename Options to Settings and Exit to Quit FM for shared terminology.
5. Refresh state on `QMenu::aboutToShow`.

Exit criterion: every tray action works with the window visible, minimized, hidden, and during a background operation.

Current implementation status:

- separate Show and Hide actions are replaced by one dynamic Open FM / Hide FM action;
- Favorites, File Search, Settings, and Quit FM use semantic controller signals and existing QML routes;
- a disabled operation row reflects the current queue label, percentage, or attention state;
- action state is refreshed from `QMenu::aboutToShow` as well as window and queue signals;
- live acceptance for visible, minimized, hidden, and active-operation routes remains pending.

### Phase 3 — Tray visual audit

1. Map current stylesheet values to actual menu theme tokens.
2. Align icon sources, tones, row height, padding, separators, and danger state with the accepted main-menu prototype.
3. Verify live theme changes without restarting.
4. Verify fallback appearance when no `ThemeController` is attached.
5. Test high-DPI icon rendering and disabled icons.

Exit criterion: Qt-owned menus visibly follow the FM theme, while host-rendered menus remain coherent and fully functional.

Current implementation status:

- tray actions use the same `icons-classic` family and semantic color roles as the accepted main menu;
- surface, hover, pressed, border, separator, disabled text, and danger icon colors come directly from `ThemeController` tokens;
- native row padding and width are aligned with the compact accepted menu proportions;
- icons are rendered at 16, 20, 24, and 32 px for DPI-aware native selection;
- live Qt-owned and host-rendered visual acceptance remains pending.

### Phase 4 — Optional native richness

Only after Phase 3 acceptance:

1. test a disabled operation/status header built from an ordinary `QAction`;
2. evaluate `QProxyStyle` or custom `QMenu` painting on platforms where Qt owns the popup;
3. reject the prototype if it breaks StatusNotifier export, accessibility, keyboard control, DPI behavior, or native dismissal.

This phase is optional and should not block the content improvements.

## 11. Testing matrix

### 11.1 Main menu

- light and dark themes, including custom themes;
- normal and minimum supported window width;
- active local folder, provider path, virtual root, and Favorites;
- split on/off;
- preview pane on/off;
- hidden files on/off independently per active panel;
- tools enabled and disabled;
- keyboard navigation through top-level items and submenus;
- Escape and click-outside close;
- opening a dialog closes the menu cleanly;
- long labels and translated-text headroom;
- admin mode active and inactive without unrelated menu changes.

### 11.2 Tray menu

- tray disabled/enabled at runtime;
- window visible, minimized, hidden, maximized, and full screen;
- no operation, operation running, operation error/conflict;
- Settings, Favorites, and File Search restoration sequencing;
- Quit while idle and while operations exist, preserving the application's existing quit policy;
- theme changed while tray remains active;
- high DPI and mixed-DPI monitors;
- KDE Plasma StatusNotifier behavior;
- another Linux tray/AppIndicator host where available;
- Windows notification area;
- application shutdown with the menu open.

## 12. Acceptance criteria

The feature is complete when:

1. the main menu exposes the approved curated actions and no selection-dependent clutter;
2. shared actions use identical labels, icons, and eligibility semantics across surfaces;
3. the main menu is fully theme-native through `FmMenu` and accepted in UI Lab;
4. the tray menu uses one unambiguous visibility action;
5. tray quick destinations restore the application before opening their target;
6. operation status never claims progress or pause capability that the queue cannot provide;
7. live theme changes update the native menu where the platform permits;
8. host-rendered tray menus remain structurally correct even when custom styling is ignored;
9. keyboard, accessibility, DPI, focus, and dismissal behavior do not regress;
10. the exact visible/minimized/hidden runtime routes are manually verified, not inferred from a successful build.

## 13. Explicit non-goals

- replacing `QSystemTrayIcon` with a custom QML tray implementation;
- making native tray menus pixel-identical across desktop environments;
- duplicating the full command palette in either menu;
- adding recent files without a dedicated privacy-reviewed model;
- adding queue-wide Pause, Resume, or Cancel All without an existing safe operation contract;
- exposing Debug Information or UI Lab as ordinary user menu items;
- creating a universal popup/menu abstraction shared by QML and widgets;
- changing single-click tray behavior in the first visual/content pass.
