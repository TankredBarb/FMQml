# Typography And Font Settings Plan

This document turns the near-term typography goal into an implementation plan.
The driver is readability: users report that the UI is hard to read, and the
app must support larger text without requiring source edits.

Status: closed as a near-term workstream. The font family and font scale pass is
implemented, exposed through Settings and command palette, and manually checked
on Windows and Linux. Remaining font/layout issues should be tracked and fixed
as targeted bugs on the affected component rather than reopening the broad
typography migration.

Related guidance:

- `docs/near-term-work-plan.md`: "Typography And Font Settings".
- `suggest/01-agent-workflow.md`: keep changes scoped and verify the path.
- `suggest/03-qml-javascript-boundaries.md`: keep settings/model logic in C++;
  QML should consume exposed settings and theme tokens.
- `suggest/09-testing-and-verification.md`: verify the changed workflow, then
  broaden by regression risk.

## Current Findings

- `qml/style/Theme.qml` hardcodes the app font family and typography tokens:
  `fontFamily`, `fontSizeH1`, `fontSizeH2`, `fontSizeBody`, `fontSizeSmall`,
  `fontSizeMini`, plus a second token set (`fontSizeTitle`,
  `fontSizeSubtitle`, `fontSizeBodyLarge`, `fontSizeLabel`,
  `fontSizeCaption`, `fontSizeMicro`).
- There is no persisted `fontFamily`, `fontScale`, or base font size setting in
  `AppSettingsController`.
- `AppSettingsController` already owns appearance settings, QML exposure,
  import, and export. This is the right place for typography persistence.
- `SettingsDialog.qml` already has a scrollable settings structure and local
  appearance/performance toggles. It can host the first font controls without a
  new top-level settings system.
- The command palette already exposes settings commands. A direct command for
  font settings can route to the settings dialog or to a focused font dialog
  later.
- A repository scan found `534` `font.pixelSize` assignments across `78` QML
  files. Only `24` QML references currently use `Theme.fontSize*` or
  `Theme.fontFamily`.
- Highest local `font.pixelSize` concentration:
  - `ThemeEditorPreviewCard.qml`: 36
  - `BatchRenameDialog.qml`: 35
  - `DebugInformationDialog.qml`: 28
  - `PropertiesDialog.qml`: 27
  - `ThemeEditorDialog.qml`: 23
  - `StorageView.qml`: 23
  - `DiskUsageDialog.qml`: 22
  - `SettingsDialog.qml`: 19
  - `FileSearchDialog.qml`: 15
  - `Splash.qml`: 15
  - `OperationsDrawer.qml`: 14
  - `FavoritesView.qml`: 14
  - `CommandPalette.qml`: 13
- Critical file-manager surfaces also have fixed layout dimensions that can clip
  larger text:
  - `Theme.rowHeight: 38`
  - `FilePanel.qml` `briefRowHeight: 28`, `footerHeight: 32`,
    `panelToolbarHeight: 42`, `selectionActionsHeight: 44`
  - `PathBar.qml` fixed `implicitHeight` values around 28-36
  - `FileTableDelegate.qml` uses `Theme.rowHeight`
  - sidebar rows compute heights locally

## Assumptions

- The first release should prioritize readability over preserving current row
  density exactly.
- The app should keep a compact file-manager feel by default, but users must be
  able to raise font scale substantially.
- The app should not expose per-component font choices. One family and one scale
  is enough for this pass.
- Monospace/code/path contexts can keep a monospace family, but their size
  should still derive from the global scale where practical.
- Preview content has two categories:
  - UI chrome around previews should follow the app typography settings.
  - Document/content previews such as book reader and text preview may keep
    their own content-size controls, but should not make surrounding UI tiny.

## Proposed User-Facing Settings

Add the following persisted appearance settings:

- `fontFamily`: string. Empty means platform/app default.
- `fontScale`: integer percent, default `100`, bounded to `90..150`.

Optional later setting:

- `compactMode`: do not add in this task unless row density becomes impossible
  to preserve with font scale alone. Existing `ultraLightMode` is performance
  oriented and should not become a readability setting.

Recommended defaults:

- Default family: current app default through `Theme.fontFamily`.
- Default scale: `110` if the visual pass is acceptable, otherwise `100` with a
  visible settings control. Because this task is driven by readability
  complaints, test `110` as the candidate default before deciding.

## Architecture

1. Extend `AppSettingsController`.
   - Add `Q_PROPERTY(QString fontFamily ...)`.
   - Add `Q_PROPERTY(int fontScale ...)`.
   - Persist both under the existing `appearance` group.
   - Include both in `appearanceSettings()` and `applyAppearanceSettings()`.
   - Clamp `fontScale` in C++.
   - Normalize `fontFamily` by trimming whitespace; empty string means default.

2. Refactor `Theme.qml` typography.
   - Derive all public typography tokens from `appSettings.fontScale`.
   - Preserve current token names to reduce QML churn.
   - Add a helper such as `scaledFontSize(baseSize)` for rare cases where a
     component needs a local semantic size during migration.
   - Keep existing token families:
     - heading/title: `fontSizeH1`, `fontSizeH2`, `fontSizeTitle`,
       `fontSizeSubtitle`
     - body/label: `fontSizeBody`, `fontSizeBodyLarge`, `fontSizeLabel`
     - small/caption: `fontSizeSmall`, `fontSizeCaption`
     - tiny/micro: `fontSizeMini`, `fontSizeMicro`
   - Consider raising the unscaled body baseline from 13 to 14 if the default
     remains too small after testing.

3. Scale related layout tokens carefully.
   - `Theme.controlHeight`, `Theme.rowHeight`, `Theme.badgeHeight`, and common
     dialog/header/footer heights should derive from typography enough to avoid
     clipping.
   - Do not scale all spacing linearly. A 150% font does not need 150% margins
     everywhere.
   - File panel dense views need explicit min/max row heights:
     - table/details row: enough for `fontSizeBody` plus vertical padding;
     - brief row: enough for `fontSizeLabel` plus padding;
     - grid cell label area: enough for two scaled lines.

4. Add Settings UI.
   - Add a `TYPOGRAPHY` or `ACCESSIBILITY` section near the top of
     `SettingsDialog.qml`, before performance/theme.
   - Controls:
     - font family selector or editable combo box;
     - font scale slider/spinbox from 90% to 150%;
     - reset button.
   - Include a compact live preview using real `Theme.font*` tokens.
   - Avoid long explanatory copy in-app. Labels should be direct.

5. Add command palette entry.
   - Add a command such as `settings.fonts`, title `Font settings`.
   - Route to the settings dialog initially. A focused font dialog can be added
     only if Settings becomes awkward.

6. Migrate QML in priority rings.
   - Ring 1: surfaces users stare at all day:
     - file table/details delegates;
     - grid and brief delegates;
     - inline rename editors;
     - sidebar;
     - path bar and toolbar path editor;
     - command palette;
     - settings dialog;
     - status bars/footers/error banners.
   - Ring 2: high-use dialogs:
     - properties;
     - file search;
     - batch rename;
     - conflict/delete/archive/password dialogs;
     - operations drawer.
   - Ring 3: preview chrome and specialty views:
     - preview header/facts/meta strips;
     - storage/disk usage;
     - debug/help/plugin manager;
     - theme editor.
   - Ring 4: illustrative previews and splash:
     - theme preview mockups may keep deliberately tiny sample text if it is
       clearly not app chrome, but should be documented as intentional.

## Risks

- Text clipping in fixed-height controls.
  - Mitigation: scale row/control heights from theme tokens before auditing
    individual labels.

- Loss of file-manager density.
  - Mitigation: use bounded scale, modest default, and min/max row heights.
    Avoid scaling spacing as aggressively as text.

- Partial migration makes the UI visually inconsistent.
  - Mitigation: complete Ring 1 in the first implementation pass; leave a
    tracked list of remaining hardcoded sizes.

- QML binding churn during live scale changes.
  - Mitigation: keep font math in `Theme.qml` and avoid per-delegate JavaScript
    functions that allocate or do heavy work.

- User-selected font may be missing or render poorly on another machine after
  settings import.
  - Mitigation: empty/default fallback, validate availability in UI, and let Qt
    font fallback handle imported unavailable families.

- Platform font differences.
  - Mitigation: test Windows and Linux with platform defaults; avoid assuming
    Segoe UI exists on Linux.

- Monospace/path fields become too wide or clip.
  - Mitigation: keep monospace family for path/code content, but scale size and
    verify eliding/scrolling.

- Preview content semantics get mixed with app chrome.
  - Mitigation: explicitly separate app typography settings from document
    content controls such as book reader size.

- Export/import changes may break older settings files.
  - Mitigation: default missing keys to current behavior and do not require a
    format bump unless import semantics become incompatible.

## Test Plan

Automated checks:

- Add focused tests for `AppSettingsController` if the test harness can support
  QSettings isolation:
  - default `fontFamily` and `fontScale`;
  - `fontScale` clamping;
  - persistence through construction;
  - export/import round trip;
  - missing keys preserve old defaults.
- If controller tests are not practical yet, document the gap and cover the same
  cases with manual verification.

Build checks:

- Linux: `cmake --build build --config Release` if this build tree exists.
- Cross-platform/Windows build command remains the project-standard release
  build when working on Windows.
- Run `ctest --test-dir build --output-on-failure` where a configured build tree
  exists.

Static QML checks:

- Before and after each migration ring:
  - `rg -n "font\\.pixelSize" qml`
  - `rg -n "font\\.family" qml`
  - `rg -n "Theme\\.fontSize|Theme\\.fontFamily" qml`
- For changed QML, remaining local pixel sizes must be either removed or
  documented as intentional content/mockup sizes.

Manual UI checks at `90%`, `100%`, `125%`, and `150%`:

- File panels:
  - details/table view;
  - brief view;
  - grid view with long filenames;
  - inline rename;
  - selection badges and status/footer text;
  - active/inactive split panels.
- Navigation:
  - path bar breadcrumbs;
  - path editor;
  - search field;
  - command palette search and command rows.
- Settings:
  - font controls update live;
  - reset works;
  - close/reopen shows current values;
  - export/import preserves values;
  - restart preserves values.
- Dialogs:
  - properties;
  - file search;
  - batch rename;
  - delete/conflict dialogs;
  - operations drawer.
- Preview:
  - preview pane chrome;
  - text preview;
  - book preview;
  - media/unsupported/archive previews.
- Themes:
  - at least one dark built-in theme;
  - at least one light built-in theme;
  - text contrast remains readable.
- Window sizes:
  - default 1120x720-ish window;
  - narrow split view;
  - small laptop height;
  - maximized.

Regression checks:

- No overlay or dialog shortcut regression while Settings/command palette is
  open.
- No horizontal text overlap in buttons or rows.
- No unexpected layout jumps while scrolling large folders.
- Large folder scrolling remains responsive enough after token bindings.
- Existing preview-specific content sizing still works.

## Suggested Implementation Order

1. Add settings plumbing and dynamic `Theme.qml` typography tokens.
   - Verify persistence, export/import, and live QML binding updates.

2. Scale global row/control height tokens.
   - Verify no clipping in file table, toolbar/path bar, and Settings.

3. Add Settings UI controls and command palette entry.
   - Verify keyboard interaction and reset.

4. Migrate Ring 1 QML surfaces.
   - Verify daily file-manager workflows at 100%, 125%, and 150%.

5. Migrate Ring 2 dialogs.
   - Verify high-use dialogs do not clip or overflow.

6. Migrate Ring 3 preview chrome and specialty views.
   - Verify previews still distinguish app chrome from document content.

7. Decide final default scale.
   - Test `100` vs `110` after Ring 1. Pick `110` only if density and clipping
     remain acceptable in split view and dense folders.

## Acceptance Criteria

- Users can change app font family and scale from Settings.
- Font settings persist across restart and export/import.
- File panels, sidebar, path bar, command palette, Settings, and common dialogs
  become visibly larger/readable at `125%` and remain usable at `150%`.
- Text does not clip or overlap in critical workflows.
- Remaining hardcoded QML font sizes are either outside the migration scope or
  intentionally tied to preview/mockup content.
- Typography logic is centralized in settings/theme code, not scattered as new
  component-local calculations.
