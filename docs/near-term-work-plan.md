# Near-Term Work Plan

This document records the next product and platform work areas. It is meant to
turn broad goals into implementable tasks with clear verification points.

Project guidance used for this plan:

- `suggest/03-qml-javascript-boundaries.md`: keep platform and file-manager
  logic in C++ controllers/core helpers, not QML JavaScript.
- `suggest/04-themes-and-colors.md`: all visual color work must flow through
  theme tokens; no component-local palettes or raw colors.
- `suggest/09-testing-and-verification.md`: every feature needs targeted checks
  before handoff.
- `suggest/15-linux-port-roadmap.md`: Linux work should use native platform
  helpers behind small abstractions, not broad Qt fallbacks as the final shape.

## 1. Gradient Visual Refresh

Goal: make the interface feel lighter and more polished, using Windows 11 Files
as the reference point for restrained gradient use, without turning the app into
a decorative one-hue surface. Gradients must be optional and controlled by a new
`useGradientColors` setting.

Current state:

- Theme colors are centralized in `ThemeController` and `qml/style/Theme.qml`.
- Some QML areas already use gradients directly, including splash and theme
  previews.
- `AppSettingsController` already owns appearance toggles such as
  `ultraLightMode`, but there is no global gradient feature gate.
- `suggest/04-themes-and-colors.md` forbids raw component colors and requires
  semantic theme tokens.

Design constraints:

- Add `useGradientColors` as an application setting, defaulting to enabled only
  if the final visual pass remains calm in both dark and light themes. If there
  is doubt, default it to disabled and expose it clearly in Settings.
- Every gradient must be behind `useGradientColors`. When disabled, surfaces
  must use the existing flat theme tokens.
- Gradients should be subtle surface treatment, not large saturated decoration.
- Do not introduce ad hoc `GradientStop` colors in feature QML. Add semantic
  gradient tokens or helper functions to the theme layer.
- Avoid gradients in dense repeated rows where they hurt scanability.

Implementation plan:

1. Add settings plumbing.
   - Add `useGradientColors` to `AppSettingsController`, persistence,
     export/import, and Settings UI.
   - Expose it to QML as `appSettings.useGradientColors`.
   - Verify: restart app and confirm the setting persists.

2. Add theme-level gradient tokens/helpers.
   - Add semantic tokens for app background, panel surface, control surface,
     selected/current item, and accent wash if needed.
   - Expose helper properties or small reusable components so QML does not
     duplicate gradient definitions.
   - Verify: dark and light built-in themes have coherent fallback colors.

3. Apply gradients only to primary surfaces first.
   - Candidate areas: app background band, sidebar/header surfaces, path bar,
     active panel emphasis, command palette/dialog shells, Settings header.
   - Keep file rows, context menus, and dense lists mostly flat unless there is
     a measured readability improvement.
   - Verify: no text overlap, no low-contrast text, no accidental purple/blue
     wash across the whole app.

4. Audit existing gradients.
   - Gate existing splash/theme-preview gradients where they represent real app
     chrome.
   - Theme preview may still show gradient samples, but it must reflect the
     setting state.
   - Verify: `rg -n "gradient:|GradientStop|#[0-9A-Fa-f]" qml` and inspect
     any changed QML for raw colors.

Acceptance checks:

- Toggling `useGradientColors` updates all app chrome that uses gradients.
- Disabling the setting leaves the app visually coherent and fully flat.
- Dark and light themes remain readable.
- No new component-local raw colors are introduced.
- Dense file-management workflows remain quiet and scannable.

## 2. Application Launching Subsystem

Goal: replace the current direct `QDesktopServices::openUrl()` launch path with
a platform-aware launch subsystem. Non-executable files can continue to route
through shell/default-app behavior. Executables need native handling, proper
error reporting, and platform-specific security behavior.

Detailed implementation plan: `docs/application-launch-subsystem-plan.md`.

Current state:

- `FilePanelController::openSelected()` calls `QDesktopServices::openUrl()` for
  regular files after handling folders, archives, ISO images, and provider URI
  paths.
- `docs/knownIssues.md` records the Windows SmartScreen/MOTW issue: downloaded
  executables can fail silently because the launch path does not pass a valid
  parent `HWND`.
- Linux currently has no deliberate executable-launch policy and no Wine-aware
  path for Windows executables.

Design constraints:

- Add a C++ launch service under `src/core` or `src/platform`; QML and
  controllers should call one project-level API such as `launchPath(path,
  parentWindow)`.
- Keep provider and archive paths out of native launch APIs unless they have
  been materialized to local files through the existing provider flow.
- Preserve shell/default-app behavior for non-executable documents.
- Executable launch must report failure in the file panel status UI or a dialog;
  silent failure is not acceptable.
- Do not bypass OS security prompts.

Implementation plan:

1. Define launch classification.
   - Classify local files as directory, archive, ISO, provider URI, document,
     native executable, script/desktop launcher, Windows `.exe`, or unknown.
   - Windows executable signals: `.exe`, `.msi`, `.bat`, `.cmd`, `.ps1`, and
     files with executable shell association where relevant.
   - Linux executable signals: mode executable bit, ELF magic, shebang scripts,
     `.desktop` files, AppImage, and Windows `.exe`.
   - Verify: unit-test classification on small fixtures where practical.

2. Add Windows native launch path.
   - Use `ShellExecuteExW` for executables with a valid parent `HWND` from the
     focused/top-level Qt window.
   - Allow SmartScreen, UAC, and MOTW dialogs to appear.
   - Use `SEE_MASK_NOCLOSEPROCESS` only if the app needs process handles later;
     otherwise keep launch fire-and-forget.
   - Convert `ShellExecuteExW`/`GetLastError` failures into user-visible
     messages.
   - Keep non-executables on shell/default-open behavior.
   - Verify: downloaded `.exe` with Mark-of-the-Web shows the Windows security
     dialog instead of failing silently.

3. Add Linux native executable path.
   - For executable local files, use `QProcess::startDetached()` or a small
     POSIX launch helper with working directory set to the file parent.
   - For `.desktop`, parse enough to execute safely or delegate to desktop
     services only when trusted/executable according to desktop conventions.
   - Normal Open/double-click must not launch Windows applications on Linux.
   - For Windows applications, expose explicit context-menu actions:
     `Open with Wine` and `Open with Steam Proton`.
   - If the selected runner is unavailable, show a clear message box telling
     the user to install or configure that runner.
   - For scripts without executable bit, do not guess an interpreter silently.
   - Verify: executable bit file runs, non-executable script reports clear
     status, `.exe` does not run from normal Open, and `.exe` runner actions
     launch or report missing Wine/Proton clearly.

4. Wire controller/UI behavior.
   - Replace direct `QDesktopServices::openUrl()` in `FilePanelController` with
     the launch service.
   - Add command palette actions if useful:
     - open/run current item;
     - open with default app;
     - run with Wine when applicable.
   - Keep launch availability disabled for providers that cannot materialize
     local files.
   - Verify: folders, archives, ISO images, providers, documents, and
     executables still route to the expected path.

Acceptance checks:

- Windows downloaded executable shows SmartScreen/UAC/MOTW prompts when the OS
  requires them.
- Windows non-executable documents still open with the default app.
- Linux ELF/script executables launch when executable and fail visibly when not.
- Linux `.exe` launches only through explicit Wine/Steam Proton context-menu
  actions and reports missing runners clearly.
- Provider URI paths do not get passed to native process launch.

## 3. Typography And Font Settings

Goal: replace hardcoded typography with user-configurable font family and size
settings. The app must provide at least minimal font selection and scaling,
available from Settings and the command palette.

Status: functionally closed after the font-scaling pass. Font family and scale
settings are implemented, exposed through Settings and command palette, and have
been manually checked on Windows and Linux. Future clipping or layout misses
should be fixed as targeted UI bugs instead of keeping typography as an active
near-term workstream.

Current state:

- `qml/style/Theme.qml` hardcodes `fontFamily` and size tokens.
- Many QML components use `Theme.font*` tokens, but some still hardcode
  `font.pixelSize`.
- There is no app setting for font family, base size, or UI scale.

Design constraints:

- Centralize typography in one QML-facing settings/theme layer. Do not add font
  choices per component.
- Use a base size plus derived tokens instead of arbitrary component-local
  sizes.
- Preserve dense file-manager ergonomics: size changes should scale rows and
  controls enough to avoid clipping, but should not randomly change layout
  density in unrelated places.
- Accessibility matters more than pixel-perfect old sizes.

Implementation plan:

1. Add typography settings model.
   - Add persisted settings for font family and base size or scale.
   - Suggested minimal settings:
     - `fontFamily`: default platform/app family.
     - `fontScale`: bounded range such as 85% to 140%.
     - Optional `compactMode` can remain separate from this task unless needed.
   - Verify: settings export/import and restart persistence.

2. Refactor `Theme.qml` typography tokens.
   - Derive `fontSizeH1`, `fontSizeBody`, `fontSizeSmall`, etc. from the
     configured base/scale.
   - Keep token names stable where possible to reduce QML churn.
   - Verify: existing components using tokens react automatically.

3. Audit hardcoded QML font sizes.
   - Replace hardcoded sizes in touched high-impact areas first:
     file delegates, sidebar, path bar, command palette, dialogs, preview facts,
     Settings.
   - Leave unrelated preview/theme mockup sizes for later only if they are
     intentionally illustrative; otherwise list them as debt.
   - Verify: `rg -n "font\\.pixelSize:" qml` trends down and remaining hits are
     justified.

4. Add font settings dialog.
   - Create a focused dialog reachable from Settings and command palette.
   - Controls:
     - font family chooser;
     - size/scale slider or spinbox;
     - reset to defaults;
     - small live preview using real app tokens.
   - Avoid explanatory marketing text; use direct labels and standard controls.
   - Verify: dialog works by keyboard and does not clip at large scale.

Acceptance checks:

- User can change font family and size without editing files.
- Settings persist across restart and export/import.
- File panels, sidebar, command palette, Settings, and common dialogs scale
  coherently.
- Text does not overlap or clip at min/max supported scale.
- New typography logic remains in settings/theme/controller code, not scattered
  QML JavaScript.

## 4. Linux Port Planning

Goal: continue Linux parity work with a clear order that improves real
file-manager behavior instead of accumulating isolated fallbacks.

Current state:

- `docs/linux-parity-roadmap.md` already covers places/mounts, native
  enumeration, watchers, copy performance, permissions, icons/MIME/thumbnails,
  system info, ISO/devices/eject, and archive runtime.
- Recent work added native Linux enumeration for panel scans, child path
  listing, file search, folder-size, and disk-usage paths.
- Recursive size behavior now follows `du -sb` semantics: apparent size,
  symlink as link, and hard-link de-duplication.
- Linux properties already show Unix owner/group/mode and effective access
  through POSIX calls.
- Linux volume display has initial QStorageInfo-based filtering and DriveUtils
  storage type hints, but no dedicated mountinfo provider yet.
- Remaining Linux work should keep platform details behind C++ helpers.

Near-term Linux sequence:

1. Places and mount cleanup.
   - Replace filtered `QStorageInfo::mountedVolumes()` usage with a dedicated
     mountinfo-backed provider.
   - Add de-duplication by device identity and mount root.
   - Keep `QStorageInfo` for byte counts and fallback only.
   - Verify on root filesystem, Home, USB/removable, and any network mount.

2. Harden native enumeration coverage.
   - Keep `LinuxFileEnumerator` as the shared helper for panel/search/folder
     size/disk usage.
   - Audit tree child loading and any remaining Qt fallback paths.
   - Add tests for symlink loops, permission denied folders, dotfiles,
     executable bits, hard links, and `/` mount boundaries.

3. Linux launching subsystem.
   - Treat this as both product work and Linux parity.
   - Add executable classification, explicit Wine/Steam Proton actions for
     Windows applications, `.desktop` handling, and clear failure UI.
   - Verify with ELF executable, shell script, AppImage, `.desktop`, and `.exe`
     through explicit Wine/Steam Proton actions with installed and missing
     runners.

4. Permissions/properties polish.
   - Treat display/effective-access support as mostly in place.
   - Add ACL detection and immutable/append-only flag reporting.
   - Consider chmod editing before chown/chgrp.
   - Verify properties for regular files, directories, symlinks, executables,
     and restricted paths.

5. Icons, MIME, thumbnails, and `.desktop` identity.
   - Treat Linux native icon baseline as complete for normal file-manager use:
     desktop-configured `QIcon::fromTheme`, MIME icons, `.desktop` icon parsing,
     and XDG special-folder theme icons are in place.
   - Parse `.desktop` enough for display and launch.
   - Add freedesktop thumbnail cache integration where useful.
   - Keep thumbnail work async.

6. Storage and file-operation performance.
   - Reuse existing DriveUtils Linux storage classification in OperationQueue.
   - Add reflink/copy_file_range local fast paths only behind local-path checks.
   - Preserve provider/archive semantics.

7. Devices, ISO, eject.
   - Add UDisks2/GIO strategy or a documented fallback strategy.
   - Keep friendly error handling for missing desktop services.

Cross-cutting verification:

- Release build from `build`.
- `ctest --test-dir build --output-on-failure` where available.
- Manual Linux smoke: Home, `/`, `/etc`, large source tree, hidden files,
  symlinks, permission-denied folders.
- Copy/move same filesystem and cross filesystem.
- Launch executable cases listed above.
- Check Places before and after plugging/removing a removable drive.

## Suggested Work Order

1. Application launch subsystem.
   - Reason: it fixes a known Windows bug, unlocks explicit Linux
     Wine/Steam Proton behavior, and touches a narrow controller path.

2. Gradient visual refresh.
   - Reason: should build on the completed typography/settings cleanup and needs
     careful visual QA across many surfaces.

3. Linux mount provider and enumeration hardening.
   - Reason: the main enumeration path is mostly in place, so the next value is
     de-duplicated mount data plus tests that keep the native path correct.

4. Linux remaining parity slices.
   - Reason: permissions display and generic MIME icons have a baseline; the
     remaining work is ACL/editing, `.desktop` identity, thumbnail cache,
     OperationQueue storage/copy integration, and UDisks2/device/eject support.
