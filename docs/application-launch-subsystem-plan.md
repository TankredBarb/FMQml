# Application Launch Subsystem Plan

This document is the implementation plan for replacing direct file launch calls
with a project-owned launch subsystem. It is intentionally detailed because the
feature crosses platform security, file-provider boundaries, context menus, and
double-click behavior.

## Decision Summary

- Build one C++ launch service with platform-specific backends.
- Keep all launch classification and policy decisions in C++.
- Preserve current folder, archive, ISO, and provider routing before native
  process launch.
- Support launching only local filesystem paths. Do not launch MTP, Google
  Drive, mock/remote providers, archive virtual paths, or managed ISO/archive
  internals through the native launch subsystem.
- On Windows, launch executable files through the shell in the same security
  model as Explorer: SmartScreen, UAC, Mark-of-the-Web, file association, and
  shell policy dialogs must be allowed to appear.
- On Linux, normal Open/double-click launches only native Linux targets and
  documents. Windows applications are never launched by normal Open or
  double-click.
- On Linux, expose two explicit context-menu actions for Windows applications:
  `Open with Wine` and `Open with Steam Proton`.
- If Wine or Steam Proton is unavailable, show a user-visible message box with
  a direct install/configuration hint. Do not fail silently and do not fall back
  to another runner.

## Current State

- `FilePanelController::openItem()` handles directories, ISO images, archives,
  nested archives, and provider URI rejection. On Windows, the final generic
  file launch branch now delegates to `LaunchService::openPath(...)`, which uses
  `ShellExecuteExW` through the Windows shell. On non-Windows platforms the
  launch service currently preserves the old `QDesktopServices::openUrl(...)`
  fallback until the Linux/macOS phases are implemented.
- Context menu `Open` calls `root.controller.openItem(contextRow())`.
- `FilePanelActionPolicy.qml` already has local-shell gating for providers and
  managed ISO mounts, but launch-specific availability is not represented.
- `docs/knownIssues.md` records the Windows SmartScreen/MOTW failure caused by
  launching without a valid parent `HWND`; the Windows launch subsystem work
  fixes that issue.
- Linux roadmap currently mentions Wine as a generic launch path, but the
  intended product behavior is stricter: Windows apps launch only through
  explicit Wine/Proton context-menu actions.

## Implementation Status

### Windows milestone completed on 2026-06-18

Implemented:

- `LaunchService` with a structured `LaunchResult` and stable error codes.
- Windows shell backend using `ShellExecuteExW` with verb `open`.
- Parent `HWND` resolution from the focused or visible top-level Qt window.
- Shell working directory set to the launched item's parent directory.
- Windows shell error mapping for common cases including file/path not found,
  access denied, no association, user cancellation, elevation/security blocked,
  invalid executable image, and unknown failure.
- `FilePanelController::openItem()` integration for the final generic file
  launch branch while preserving existing folder, ISO, archive, nested archive,
  and provider rejection flows.
- Windows shortcut semantics:
  - shortcuts to folders continue to navigate to the target folder;
  - shortcuts to files are launched as the `.lnk` itself so the shell preserves
    shortcut arguments, working directory, compatibility metadata, and launcher
    behavior.
- Focus handling after shell handoff:
  - cancelled or failed launches re-activate FMQml;
  - successful handoff briefly re-activates FMQml so slow-starting executables
    do not leave the app visually defocused before their own window appears.

Verified:

- Release build completed successfully.
- Local Windows executable smoke testing behaved as expected.

Still out of scope for the completed Windows milestone:

- Generic "Open with..." picker.
- Per-extension preferences.
- Linux native executable classification.
- Wine and Steam Proton runners.
- Remote/provider materialization for launch.

Keep as regression checks:

- Downloaded `.exe` with Mark-of-the-Web shows SmartScreen instead of silent
  failure.
- UAC-required launch shows UAC and returns focus cleanly when cancelled.
- `.msi`, `.bat`, `.cmd`, documents, and unknown extensions route through shell
  behavior.
- `.lnk` game/application shortcuts preserve saved profile behavior, arguments,
  and working directory.
- Provider paths and archive virtual paths do not reach `ShellExecuteExW`.

## Non-Goals

- Do not add generic "Open with..." application picker in this pass.
- Do not support remote/provider materialization for launch in this pass.
- Do not implement per-extension user preferences.
- Do not bypass SmartScreen, UAC, desktop trust checks, executable bits, or any
  other operating-system security mechanism.
- Do not run Windows applications on Linux from double-click or normal Open.
- Do not silently choose Wine when Proton is missing, or Proton when Wine is
  missing.
- Do not implement macOS launch behavior in this pass beyond preserving existing
  behavior where possible.

## Terminology

- **Local path:** A path that maps directly to the host filesystem and has no
  non-local explicit scheme. `file://` may be normalized to a local path.
- **Provider path:** A remote or virtual provider path such as MTP, Google
  Drive, mock provider, or any non-local scheme. These are not launchable.
- **Archive virtual path:** An `archive://` path or path inside an archive
  provider. These are not launchable until materialized by existing archive
  workflows.
- **Managed mount path:** A temporary/mounted path owned by the app for ISO or
  archive internals. These should be excluded unless a specific existing
  workflow already permits local shell actions for that path.
- **Native executable:** A platform-native executable target:
  - Windows: `.exe`, `.msi`, `.bat`, `.cmd`, `.ps1`, and shell-associated
    executable types where shell launch is expected.
  - Linux: executable ELF, executable shebang script, executable AppImage, and
    trusted/executable `.desktop` launcher.
- **Windows application on Linux:** A PE/COFF Windows executable, normally
  `.exe` or `.msi`, intended to run through Wine or Proton only by explicit
  context-menu action.

## Architecture

### Components

1. `LaunchService`
   - Public C++ entry point used by controllers.
   - Owns path normalization, local-provider gating, classification, and result
     mapping.
   - Exposes small invokable methods to QML/controllers:
     - `openPath(path, parentWindow)`
     - `openWithWine(path, parentWindow)`
     - `openWithSteamProton(path, parentWindow)`
     - `launchCapabilities(path)`
   - Returns structured results instead of boolean-only success.

2. `LaunchClassifier`
   - Pure C++ helper with no UI dependencies.
   - Classifies one local filesystem path into launch categories.
   - Performs cheap metadata reads only: `QFileInfo`, mode bits, suffix, small
     magic-header reads, optional `.desktop` parsing.
   - Has focused unit tests.

3. `WindowsLaunchBackend`
   - Windows-only implementation.
   - Uses `ShellExecuteExW` for shell launch with a valid parent `HWND`.
   - Keeps Explorer-like behavior by using shell verbs and not bypassing OS
     security prompts.

4. `LinuxLaunchBackend`
   - Linux-only implementation.
   - Handles document/default-app launch, native executable launch, `.desktop`
     launch policy, Wine launch, and Steam Proton launch.
   - Uses `QProcess::startDetached()` or narrowly scoped POSIX/Qt helpers with
     the working directory set to the target parent directory where applicable.

5. UI/controller integration
   - `FilePanelController::openItem()` delegates regular local file launch to
     `LaunchService`.
   - Context menu shows Wine/Proton actions only on Linux, only for local
     Windows applications, and only outside provider/archive/managed paths.
   - Failures are surfaced through a message box for missing tools and through
     status/dialog UI for other launch failures.

### Ownership Boundary

QML may decide whether to show a menu item based on exposed capabilities, but it
must not duplicate executable classification or runner discovery logic. The
source of truth is C++.

Recommended shape:

- Add `LaunchCapabilities` as a `Q_GADGET`/`QObject`-friendly value exposed as a
  `QVariantMap` if simple QML binding is preferred.
- Keep capability keys stable:
  - `canOpen`
  - `canOpenWithWine`
  - `canOpenWithSteamProton`
  - `isLocal`
  - `isWindowsApplication`
  - `openBlockedReason`
- Avoid per-row expensive QML calls while scrolling. Capabilities should be
  computed when opening the context menu for the current item, not for every
  delegate.

## Launch Policy Matrix

| Path type | Windows Open/double-click | Linux Open/double-click | Linux Wine menu | Linux Proton menu |
| --- | --- | --- | --- | --- |
| Local directory | Existing navigation | Existing navigation | Hidden | Hidden |
| Local document | Shell/default app | Desktop/default app | Hidden | Hidden |
| Windows native executable on Windows | `ShellExecuteExW` | N/A | Hidden | Hidden |
| Linux ELF executable | N/A | Native launch | Hidden | Hidden |
| Executable shebang script | N/A | Native launch | Hidden | Hidden |
| Non-executable script | N/A | Error/status; do not guess interpreter | Hidden | Hidden |
| AppImage executable | N/A | Native launch | Hidden | Hidden |
| `.desktop` trusted/executable | N/A | Desktop launcher policy | Hidden | Hidden |
| `.desktop` untrusted/non-executable | N/A | Error/status | Hidden | Hidden |
| Windows `.exe`/`.msi` on Linux | N/A | Blocked with explanatory message/status | Visible | Visible |
| Provider file | Blocked before launch | Blocked before launch | Hidden | Hidden |
| Archive virtual file | Blocked before launch | Blocked before launch | Hidden | Hidden |
| Managed ISO/archive internal | Blocked unless explicitly allowed by existing policy | Blocked unless explicitly allowed by existing policy | Hidden | Hidden |

## Windows Implementation Plan

### 1. Define Windows Launch Semantics

Windows should use the shell as the launch authority. The goal is not to start a
process directly; the goal is to ask Windows to open/run the file exactly as the
user would expect from Explorer.

Rules:

- Use `ShellExecuteExW`.
- Use verb `open` by default.
- For executable-like files, do not use `CreateProcess`.
- Pass a valid parent `HWND` from the focused/top-level Qt window.
- Preserve shell file association behavior for documents.
- Allow UAC, SmartScreen, MOTW, antivirus, file-association, and policy prompts
  to appear.
- Convert shell failure codes into user-visible messages.

### 2. Parent Window Resolution

Implement a helper:

- Prefer `QGuiApplication::focusWindow()`.
- Fall back to `QGuiApplication::activeWindow()`.
- Fall back to the first visible top-level window.
- Convert `QWindow::winId()` to `HWND`.
- If no window exists, launch may still be attempted, but result messaging must
  note that the parent window could not be resolved if the shell reports access
  denied or cancelled.

Verification:

- The SmartScreen dialog for a downloaded `.exe` is parented to the FMQml
  window.
- UAC prompts still appear when required.
- No hidden modal dialog leaves the app apparently frozen.

### 3. ShellExecuteExW Options

Recommended baseline:

- `SHELLEXECUTEINFOW info = {};`
- `info.cbSize = sizeof(info);`
- `info.hwnd = parentHwnd;`
- `info.lpVerb = L"open";`
- `info.lpFile = path;`
- `info.lpDirectory = parentDirectory;`
- `info.nShow = SW_SHOWNORMAL;`
- Use `SEE_MASK_NOASYNC` only if needed to keep error reporting deterministic.
- Do not use `SEE_MASK_NOCLOSEPROCESS` unless future code needs a process
  handle.

Failure handling:

- `ShellExecuteExW` returning false should capture `GetLastError()`.
- `HINSTANCE` shell error values should be mapped when available.
- Map common cases:
  - access denied/security blocked;
  - file not found/path not found;
  - no association;
  - cancelled by user;
  - invalid executable image;
  - generic shell failure.

### 4. Windows Classification

Classification is still useful, but Windows launch should not overrule the
shell. Use it for messaging and tests, not to bypass the shell.

Executable-like suffixes:

- `.exe`
- `.msi`
- `.bat`
- `.cmd`
- `.ps1`
- `.com`
- optionally `.lnk` if current shortcut handling does not already cover it.

Document files:

- Route through the same `ShellExecuteExW` path with verb `open`.
- This preserves file associations and avoids Qt-specific launch differences.

### 5. Windows Controller Integration

Replace only the final generic launch branch in `FilePanelController::openItem()`.

Keep existing earlier branches:

- directories navigate;
- ISO images request mount;
- archive files open through archive provider;
- nested archives ask for materialization approval;
- provider URI paths are rejected before launch.

New flow:

1. Resolve item path and shortcut target as today.
2. If folder/archive/ISO/provider branch applies, keep existing behavior.
3. Call `LaunchService::openPath(path, parentWindow)`.
4. If result is failure:
   - for security/tool/actionable failures, show dialog/message box;
   - for ordinary failure, set file panel status and optionally show details.

### 6. Windows Acceptance Tests

Manual:

- Downloaded `.exe` with Mark-of-the-Web shows SmartScreen instead of silent
  failure.
- UAC-requiring installer shows UAC prompt.
- `.msi` installer opens via Windows Installer.
- `.bat`/`.cmd` behavior matches Explorer policy.
- `.txt`, `.pdf`, image, and unknown extension route through default shell
  behavior.
- File with no association shows clear message.
- Provider path and archive virtual path do not reach `ShellExecuteExW`.

Automated where practical:

- Unit-test classification.
- Unit-test local/provider/archive gating.
- Unit-test failure-message mapping with synthetic error codes.

## Linux Implementation Plan

### 1. Define Linux Product Semantics

Linux has three distinct user actions:

1. `Open` / double-click
   - For documents: open with desktop default app.
   - For native Linux executables: run natively.
   - For Windows applications: do not launch; show a clear message/status that
     Windows apps must be launched from explicit context-menu actions.

2. `Open with Wine`
   - Visible only for local Windows applications.
   - Runs the target through Wine if Wine is installed.
   - If Wine is missing, shows a message box instructing the user to install
     Wine and retry.

3. `Open with Steam Proton`
   - Visible only for local Windows applications.
   - Runs the target through a discovered/configured Steam Proton runtime.
   - If Steam or Proton is missing, shows a message box explaining that Steam
     Proton must be installed/configured.

### 2. Linux Classification

For local regular files:

- Read suffix first for cheap hints.
- Read first bytes for magic:
  - ELF: `0x7F 'E' 'L' 'F'`
  - PE/COFF: `MZ` header plus, if implemented, PE signature at `e_lfanew`
  - script: `#!`
- Check executable bit with `QFileInfo::isExecutable()` or POSIX mode.
- Detect AppImage:
  - executable bit;
  - suffix `.AppImage` case-insensitive or AppImage magic/metadata if cheap.
- Detect `.desktop`:
  - suffix `.desktop`;
  - parse minimally as freedesktop desktop entry;
  - require `Type=Application`;
  - require executable/trusted local file according to current desktop
    conventions before running.

Classification categories:

- `Document`
- `NativeExecutableElf`
- `NativeExecutableScript`
- `NativeExecutableAppImage`
- `DesktopLauncherTrusted`
- `DesktopLauncherBlocked`
- `WindowsApplication`
- `NonExecutableScript`
- `UnknownExecutable`
- `Unsupported`

### 3. Linux Open / Double-Click Behavior

Documents:

- Use desktop/default app path, preferably `QDesktopServices::openUrl()` unless
  it causes known problems.
- Failure must be visible.

Native executables:

- Use `QProcess::startDetached(program, args, workingDirectory)`.
- Program is the target path.
- Working directory is the parent directory.
- Preserve executable bit requirements.
- Do not guess interpreters for non-executable scripts.

Scripts:

- Executable shebang script: start detached using the script path.
- Non-executable shebang script: do not run; show message/status indicating the
  file is not executable.

AppImage:

- Require executable bit.
- Start detached with parent directory as working directory.
- If not executable, show message/status instructing user to mark executable.

`.desktop`:

- Minimal parser should handle `Type`, `Name`, `Exec`, `Icon`, `Terminal`, and
  `NoDisplay` only if needed.
- Do not run untrusted/non-executable launchers.
- Handle `Exec` field codes safely. Strip/expand only supported field codes.
- If `Terminal=true`, either use the existing terminal launcher or show a
  targeted "terminal desktop launchers are not supported yet" message until
  implemented.
- If parsing is uncertain, block with a clear message rather than executing an
  incorrectly parsed command.

Windows applications:

- Normal Open/double-click does not launch.
- Message should be concise:
  - "This is a Windows application. Use Open with Wine or Open with Steam Proton
    from the context menu."
- Context menu provides the two explicit runners.

### 4. Wine Runner

Discovery:

- Check `wine` in `PATH`.
- Optionally check `wine64` as fallback only if product decision accepts it.
- Use `QStandardPaths::findExecutable()`.

Execution:

- Command: `wine <target-path>`.
- Working directory: target parent directory.
- Keep environment inherited from FMQml.
- Do not wrap in shell.
- Do not add Wine prefix management in this pass.

Missing tool UX:

- Show message box:
  - title: `Wine is not installed`
  - body: `Install Wine and try Open with Wine again.`
  - optional detail: include distro-specific install commands only if the app
    already has platform-specific help text; otherwise avoid guessing.

Failure handling:

- If `startDetached()` fails, show message:
  - "Could not start Wine for <file>."
- Include executable path and runner path in details/log output if a details UI
  exists.

### 5. Steam Proton Runner

This is riskier than Wine because Proton is normally managed by Steam and not a
generic system command. Implement in phases to keep behavior explicit and
testable.

Phase 1: Discovery and clear missing-tool UX

- Detect Steam installation:
  - Flatpak Steam: `flatpak` plus `com.valvesoftware.Steam` installed.
  - Native Steam: common Steam library roots under user home.
- Detect installed Proton compatibility tools:
  - Steam library `steamapps/common/Proton*`;
  - Steam compatibility tool manifests where available;
  - do not scan the entire filesystem.
- If no Proton runtime is found, show:
  - title: `Steam Proton is not available`
  - body: `Install Steam and a Proton compatibility tool, then try Open with
    Steam Proton again.`

Phase 2: Launch strategy

Preferred strategy must be decided during implementation research against a real
Steam installation. Candidate approaches:

1. Use Steam protocol for known Steam app IDs only.
   - Lowest risk for Steam-managed games.
   - Not enough for arbitrary local `.exe` files.

2. Use Proton executable directly.
   - Command resembles: `<proton>/proton run <target-path>`.
   - Requires Steam runtime environment and a compatibility data path.
   - Needs a deterministic `STEAM_COMPAT_DATA_PATH`.
   - May need `STEAM_COMPAT_CLIENT_INSTALL_PATH`.

For this project, arbitrary local Windows apps are the target, so direct Proton
execution is likely required. The implementation must create/use an app-owned
compat data directory under the app's cache/config location, not beside the
target executable.

Proposed direct execution:

- Choose latest compatible Proton directory by stable version sort.
- Set:
  - `STEAM_COMPAT_CLIENT_INSTALL_PATH` to Steam root when known.
  - `STEAM_COMPAT_DATA_PATH` to an app-owned per-target or shared directory.
- Command: `<proton>/proton run <target-path>`.
- Working directory: target parent directory.
- Do not invoke through shell.

Open questions to resolve before coding Proton:

- Whether to support Flatpak Steam Proton from outside the sandbox.
- Whether `pressure-vessel`/Steam runtime environment is required on target
  distros.
- Whether one shared compatdata directory is acceptable, or per-executable hash
  directories avoid prefix contamination better.
- Whether Proton-GE installations should be detected alongside Valve Proton.

Recommended first implementation:

- Implement menu action, detection, and missing-tool message.
- Implement direct Proton launch only after testing against at least one native
  Steam and one Flatpak Steam setup, or explicitly scope Flatpak out.
- If direct Proton launch cannot be made robust quickly, keep the action but
  report "Steam Proton launch is not configured yet" only if that is acceptable
  for the product milestone. Otherwise, do not merge partial Proton UI.

### 6. Linux Context Menu Integration

Add capability methods to `FilePanelController` or a `LaunchController` exposed
to QML:

- `launchCapabilitiesForPath(path)`
- `openPathWithWine(path)`
- `openPathWithSteamProton(path)`

Context-menu visibility:

- `Open with Wine`
  - platform is Linux;
  - path is local;
  - path is not provider/archive/managed mount;
  - classifier says `WindowsApplication`.
- `Open with Steam Proton`
  - same visibility as Wine.

Context-menu enabled state:

- Prefer always enabled when visible, even if tool is missing, because clicking
  gives an actionable message box.
- Alternatively disable only when classification is impossible, not when the
  runner is missing.

Placement:

- Put Wine/Proton actions directly after `Open` and before mutation actions.
- Add a separator only when at least one runner action is visible.

### 7. Linux Acceptance Tests

Manual:

- `.txt` opens in default app.
- ELF executable with executable bit launches.
- ELF file without executable bit does not launch and reports clearly.
- Executable shebang script launches.
- Non-executable shebang script does not launch and reports clearly.
- AppImage launches when executable.
- AppImage without executable bit reports clearly.
- Trusted executable `.desktop` launcher works, or blocks with explicit
  unsupported reason if not implemented in first pass.
- Untrusted/non-executable `.desktop` launcher does not run.
- Windows `.exe` double-click does not run and tells user to use Wine/Proton
  context actions.
- Windows `.exe` context menu shows `Open with Wine` and `Open with Steam
  Proton`.
- Missing Wine shows message box with install hint.
- Installed Wine starts the target with target parent as working directory.
- Missing Proton shows message box with install/configuration hint.
- Installed Proton starts the target or produces a clear runner failure message.
- Provider, MTP, Google Drive, archive virtual paths, and managed mount internals
  do not show Wine/Proton actions.

Automated:

- Unit-test classifier with fixture files:
  - ELF magic fixture;
  - PE/MZ fixture;
  - shebang fixture;
  - non-executable shebang metadata if test harness supports mode bits;
  - `.desktop` parser fixtures;
  - ordinary document.
- Unit-test path gating:
  - local path;
  - `file://` path;
  - `archive://`;
  - provider scheme;
  - `devices://`;
  - managed ISO path when mockable.
- Unit-test runner discovery by injecting PATH and fake Steam roots.
- Unit-test missing-tool result mapping.

## Result And Error Model

Use structured results so UI can make consistent choices.

Suggested fields:

- `ok`: bool
- `action`: `Open`, `OpenWithWine`, `OpenWithSteamProton`
- `category`: classifier category
- `errorCode`: stable enum
- `title`: short user-facing title
- `message`: user-facing message
- `details`: optional diagnostic details
- `showDialog`: bool

Suggested error codes:

- `None`
- `NotLocalPath`
- `ProviderPathUnsupported`
- `ArchivePathUnsupported`
- `ManagedMountUnsupported`
- `FileNotFound`
- `PermissionDenied`
- `NotExecutable`
- `NoAssociation`
- `UserCancelled`
- `SecurityBlocked`
- `RunnerUnavailable`
- `RunnerStartFailed`
- `DesktopLauncherUntrusted`
- `DesktopLauncherUnsupported`
- `WindowsAppRequiresExplicitRunner`
- `UnsupportedPlatform`
- `UnknownFailure`

Message policy:

- Missing Wine/Proton: message box.
- Windows app opened through normal Linux Open: status or message box; prefer
  message box if triggered by double-click because otherwise it looks like
  nothing happened.
- OS security/user-cancelled cases: do not treat cancellation as a scary error.
- Provider/archive unsupported: status message is enough unless a context-menu
  action somehow invoked it.
- Shell/native start failures: message box with concise details.

## Security And Safety Rules

- Never run a provider or archive virtual path as a process.
- Never pass user-controlled command lines through a shell.
- Always pass executable and arguments as separate process arguments.
- Always set working directory deliberately.
- Do not parse `.desktop Exec` with naive whitespace splitting if quotes or
  field codes are present; implement a minimal safe parser or block unsupported
  entries.
- Do not bypass Windows shell security mechanisms with `CreateProcess`.
- Do not auto-run Windows applications on Linux from double-click.
- Do not infer interpreters for non-executable scripts.
- Treat symlink targets according to filesystem metadata, but keep displayed
  path in messages.

## Suggested Implementation Order

1. Add classifier and path gating.
   - Verify with unit fixtures and provider/archive path tests.

2. Add `LaunchService` result model. **Done for Windows-first launch path.**
   - Verify controller can receive structured failure without UI regressions.

3. Implement Windows backend. **Done.**
   - Verify SmartScreen/MOTW, UAC, documents, and no-association failures.

4. Wire `FilePanelController::openItem()` to `LaunchService`. **Done for the
   generic Windows launch branch.**
   - Verify existing folder/archive/ISO/provider flows still behave as before.

5. Implement Linux native/document open.
   - Verify documents, ELF, scripts, AppImage, `.desktop` policy, and blocked
     Windows app normal-open behavior.

6. Add Linux Wine runner.
   - Verify missing Wine message and installed Wine launch.

7. Add Linux Proton discovery and runner.
   - Verify missing Proton message first, then direct launch on tested Steam
     installations.

8. Add Linux context-menu actions.
   - Verify visibility is local Windows-app only and not shown for providers,
     archives, folders, documents, or native Linux executables.

9. Polish messages and diagnostics.
   - Verify no silent failure remains for launch attempts.

10. Update roadmaps and known issue status.
    - Windows launch status and known issue state are updated. Keep MOTW,
      UAC-cancel, and shortcut launch behavior in the regression checklist.

## Final Acceptance Criteria

- Windows executable launch behaves like Explorer for SmartScreen, MOTW, UAC,
  and shell association flows. **Implemented for the Windows milestone.**
- Windows documents still open with default associated applications.
  **Implemented for the Windows milestone.**
- Linux Open/double-click launches documents and native Linux executables only.
- Linux Open/double-click does not launch Windows applications.
- Linux context menu exposes `Open with Wine` and `Open with Steam Proton` only
  for local Windows applications.
- Missing Wine and missing Proton produce clear message boxes.
- Provider, MTP, Google Drive, archive virtual paths, and managed ISO/archive
  internals do not enter native launch backends and do not show runner actions.
- No launch failure path is silent.
- Classification and policy are covered by focused automated tests where the
  current test harness supports them.
