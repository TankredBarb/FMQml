# Linux Admin Mode Implementation Plan

Goal: add a Linux-only admin mode that lets FMQml perform a small, explicit set
of privileged local file operations without relaunching the GUI as root. The GUI
process, QML, previews, plugins, thumbnails, launch/open logic, and normal file
panels stay unprivileged. Privileged work goes through a narrow helper after
system authentication.

The user experience must be reasonable: do not ask for the password every few
seconds during a planned batch, but also do not store the root password or turn
FMQml into a permanent root session.

## 1. Product Scope

### 1.1. User-facing behavior

Initial supported workflows:

- unlock admin mode through the system authentication agent;
- copy one or more local files into `/etc`, `/usr/local`, `/opt`, or another
  protected local directory;
- create a protected directory;
- atomically replace a protected file after explicit confirmation;
- retry a permission-denied operation as Administrator;
- see whether admin mode is locked, unlocking, active, expiring, expired, or
  unavailable;
- lock admin mode manually without restarting the app.

Later workflows:

- recursive directory copy;
- guarded same-filesystem rename/move;
- controlled protected-file editing through a user-owned temp copy plus
  privileged atomic replace;
- carefully reviewed delete support;
- privileged archive extraction only after the helper path policy is mature.

The intent is not to run FMQml as root. Admin mode is a route for selected file
operations, not a process-wide privilege change.

### 1.2. Non-goals

- Do not use `kdesu`, `gksu`, `beesu`, or desktop-specific wrappers.
- Do not relaunch the full GUI as root on Linux.
- Do not create a root shell, command runner, or generic file-provider backdoor.
- Do not let plugins perform privileged operations by default.
- Do not route archive/provider/remote paths directly to the privileged helper.
- Do not silently bypass permissions; every privileged write is visible in UI
  and operation history.
- Do not collect the root password in QML.
- Do not store the root password, Polkit tokens, PAM text, or sudo-like
  credentials on disk.

### 1.3. Relationship to Windows elevation

Windows can keep its native relaunch/elevation behavior. Linux admin mode is a
separate design: regular-user GUI plus a minimal privileged helper.

## 2. Architecture

### 2.1. High-level shape

Add a Linux-only privilege subsystem:

1. `AdminController`
   - Extends existing admin UI/API without changing Windows semantics.
   - Exposes admin-mode state, backend availability, timeout, and lock/unlock
     actions.

2. `LinuxAdminSession`
   - Owns app-side authorization state.
   - Tracks idle timeout, last privileged operation, active requests, manual
     lock, and expiry.
   - Never stores credentials.

3. `LinuxAdminBroker`
   - Unprivileged client-side IPC wrapper.
   - Sends typed operation requests and receives progress/result/error events.
   - Owns protocol version negotiation and request ids.

4. `fm-admin-helper`
   - Small privileged helper, not a GUI process.
   - Accepts only a fixed operation protocol.
   - Performs root-side path validation and the requested operation.
   - Never executes shell commands.

5. `LinuxAdminPolicy`
   - Central policy module for operation eligibility, path classes, symlink
     rules, overwrite rules, denylisted roots, and test fixtures.

### 2.2. Backend choice

Preferred backend: Polkit plus a custom helper.

Rationale:

- Polkit is the standard Linux authorization framework for desktop privileged
  actions.
- The system authentication agent owns the password prompt.
- FMQml avoids password handling and desktop-specific wrappers.
- Polkit can cache authorization according to system policy, while FMQml still
  keeps its own visible locked/active state.

Rejected for normal builds:

- `sudo`/`su` with password written to stdin.
- Running the whole GUI as root.
- A long-lived general-purpose root daemon.

Fallback:

- A setuid helper may be considered only behind an explicit build option, off by
  default, with strict install-time ownership/permission checks. It is not part
  of the MVP.

### 2.3. Helper lifetime

Prefer request-scoped or short-lived helper execution. The app may keep an
admin session active for user convenience, but the helper API must still require
each privileged request to declare:

- operation type;
- exact source/destination paths;
- conflict/overwrite policy;
- confirmation token when needed;
- session nonce/request id.

The helper must not become a broad root service that accepts arbitrary future
commands merely because the user authenticated once.

## 3. Authorization Model

### 3.1. Session states

Expose these states:

- `Unavailable`: backend/action/helper is missing or incompatible.
- `Locked`: backend exists, app-side admin session is inactive.
- `Unlocking`: system authentication or helper handshake is in progress.
- `Active`: admin mode is available for eligible operations.
- `ExpiringSoon`: idle timeout is close to expiry.
- `Expired`: app-side session expired and must be unlocked again.
- `Revoking`: manual lock/revoke is in progress.
- `Error`: last unlock or backend request failed.

### 3.2. Timeout policy

Default: idle timeout, 10 minutes.

Rules:

- The user authenticates once, then can perform a planned privileged batch
  without repeated prompts every few seconds.
- Each successful privileged operation refreshes the idle timer.
- Manual lock immediately disables app-side admin mode.
- Expiry happens in FMQml even if Polkit still has a cached authorization.
- On expiry, queued future privileged operations must stop and require a fresh
  unlock or explicit retry.
- Session-until-exit mode is not MVP. It may be added later only as an advanced
  setting or package override with very visible UI.

This balances the two risks: password spam is hostile UX, but permanent
root-like state is unsafe.

### 3.3. Password handling

- With Polkit, FMQml never sees the password.
- The authentication prompt belongs to the system Polkit agent.
- No password or auth token is stored in settings, logs, registry files, or QML
  state.
- If a non-Polkit fallback is ever added, password collection must happen in a
  minimal native component, not QML, and buffers must be wiped immediately after
  authentication.

## 4. Operation Routing

### 4.1. When admin mode is offered

Admin mode is offered when:

- preflight detects the current user cannot write/create/replace in the target
  local directory;
- a normal local operation fails with `EACCES`/`EPERM`;
- the user explicitly chooses an admin action such as `Paste as Administrator`;
- a retry dialog offers `Retry as Administrator`.

The normal operation queue remains the front door. Admin mode is a route inside
operation execution, not a duplicate file manager.

### 4.2. MVP privileged operations

MVP:

- copy regular local file to protected local destination;
- create protected directory;
- create protected regular file;
- rename one protected local item in place;
- delete one protected local item, including recursive directory delete;
- atomic replace regular file using destination-near `.part` staging and final
  rename;
- cancel an active privileged copy/replace and clean partial output;
- progress reporting for byte-copy operations;
- structured errors mapped to existing operation errors.

Explicitly not MVP:

- arbitrary chmod/chown UI;
- privileged symlink creation;
- privileged archive extraction;
- privileged preview/thumbnail generation;
- external editor launched as root;
- provider/cloud/plugin privileged writes.

### 4.3. Later operations

After MVP is stable:

- recursive directory copy with symlink policy;
- cross-filesystem move as copy + verify + remove;
- protected-file edit flow through user-owned temp materialization + privileged
  atomic replace;
- archive extraction to protected destinations, preferably by extracting to
  user-owned staging first and then performing privileged copy/finalize.

## 5. Helper Protocol

### 5.1. Transport

Acceptable options:

- Polkit-authorized D-Bus service with strict action methods;
- short-lived helper process launched after authorization with private pipes or
  a private Unix socket.

Requirements:

- versioned protocol;
- authenticated peer UID;
- per-session nonce/request id;
- no shell strings;
- bounded message sizes;
- helper refuses requests from unexpected UID/session where possible.

### 5.2. Request schema

Every request includes:

- protocol version;
- operation id;
- operation type;
- source path(s);
- destination path;
- overwrite/conflict policy;
- expected source/destination file type;
- metadata preservation flags;
- confirmation token for high-risk operations;
- session nonce.

Every response includes:

- operation id;
- status: started, progress, completed, cancelled, failed;
- processed bytes/items and total bytes/items when known;
- normalized error code;
- failed path when relevant;
- user-readable error message.

### 5.3. Error model

Normalize helper errors into existing operation errors:

- authentication required;
- authorization expired;
- backend unavailable;
- permission denied;
- invalid path;
- symlink policy denied;
- not found;
- already exists;
- not a directory;
- directory not empty;
- read-only filesystem;
- no space left;
- cancelled;
- protocol/version mismatch.

## 6. Path and Safety Policy

### 6.1. Root-side validation

The helper must validate paths after resolving them with root-owned logic:

- reject empty paths, relative paths, embedded NUL, URLs, provider paths, and
  archive paths;
- operate only on local filesystem paths supported by the specific operation;
- canonicalize parent directories with `openat`/`fstatat` style APIs where
  practical;
- avoid time-of-check/time-of-use races by operating relative to file
  descriptors;
- reject pseudo-filesystems by default: `/proc`, `/sys`, `/dev`, `/run/user`,
  and similar runtime mounts;
- reject mount roots and top-level system roots for destructive operations.

### 6.2. Symlink rules

Default:

- copy symlinks as symlinks only when the operation explicitly supports that;
- do not traverse destination symlinks for replace;
- create new destination files with `O_NOFOLLOW` where applicable;
- for recursive operations, do not follow symlinked directories by default;
- show a clear policy error when denied.

### 6.3. Destructive operations

Destructive privileged recursive delete is supported only for one local item at
a time.

It requires:

- exact target path in the confirmation UI;
- operation count when known;
- denylist for `/`, `/etc`, `/usr`, `/bin`, `/sbin`, `/lib`, `/lib64`, `/boot`,
  `/var`, `/home`, mount roots, and pseudo-filesystems;
- no recursive delete without explicit confirmation;
- no root-owned trash/staging unless cleanup is robust and tested.

## 7. UI/UX Plan

### 7.1. Entry points

- operation failure dialog: `Retry as Administrator`;
- context menu/action for protected destinations: `Paste as Administrator`,
  `Create Folder as Administrator`;
- toolbar/status area: admin mode indicator;
- command palette: `Unlock Admin Mode`, `Lock Admin Mode`, `Show Admin Mode`;
- settings: default timeout duration.

### 7.2. Visual state

Admin mode should be visible but not make the whole app look like it is running
as root:

- compact toolbar/status indicator;
- tooltip/menu with state, backend, and remaining idle time;
- operation queue rows marked `Administrator`;
- warning accent only for destructive privileged confirmations.

### 7.3. Confirmation copy

Use operation-specific text:

- `Copy 2 files to /etc/nginx as Administrator?`
- `Create /usr/local/share/example as Administrator?`
- `Replace /usr/local/bin/tool as Administrator?`

Avoid vague prompts like `Run as root?`.

### 7.4. Safety disclosure

Show a concise safety disclosure before the first unlock in an app session, and
again only after the user explicitly asks for details or the app restarts.

The disclosure should say:

- administrator actions can overwrite or damage system files;
- FMQml itself will remain unprivileged;
- only the confirmed file operation will be sent to the privileged helper;
- the user can lock admin mode at any time.

Do not show this warning before every single privileged operation. Repeated
warnings become noise and encourage blind confirmation. High-risk operations
such as delete or recursive replace still need their own operation-specific
confirmation.

Suggested copy:

`Administrator mode can modify protected system files. FM will stay running as
your normal user, but confirmed file operations can change root-owned paths.
Review each target carefully.`

## 8. Integration Points

### 8.1. `AdminController`

Keep Windows behavior intact:

- existing `isElevated` / `relaunchAsAdmin` semantics remain Windows-specific;
- Linux adds `adminModeAvailable`, `adminModeState`,
  `adminModeRemainingSeconds`, `adminModeBackendName`, and unavailable reason;
- Linux adds `unlockAdminMode()`, `lockAdminMode()`, `refreshAdminMode()`.

### 8.2. `OperationQueue`

Add privileged execution as an operation route:

- preflight decides whether admin route is eligible;
- conflict resolution remains in the normal operation flow where possible;
- privileged operations are serialized initially;
- cancellation sends a cancel request to the broker/helper;
- operation history records `as Administrator`.

### 8.3. `LocalFileProvider`

The provider may expose preflight helpers:

- can current user write destination parent?
- would create/replace/delete likely fail with `EACCES`/`EPERM`?
- is the path local and eligible for admin routing?

The provider itself should not become root-aware everywhere.

### 8.4. Editing and preview

Protected editing is later-phase work:

1. read normally if possible;
2. otherwise optionally materialize through a privileged read-to-temp request;
3. edit a user-owned temp copy;
4. replace through privileged atomic replace.

Quick Look and thumbnails remain unprivileged for MVP.

## 9. Packaging

### 9.1. CMake options

- `FM_ENABLE_LINUX_ADMIN_MODE`: default `ON` on Linux only when dependencies are
  found.
- `FM_ENABLE_LINUX_ADMIN_SETUID_HELPER`: default `OFF`.
- `FM_LINUX_ADMIN_DEFAULT_TIMEOUT_MINUTES`: package override, default `10`.

### 9.2. Installed files

Polkit backend:

- `fm-admin-helper` in trusted libexec path;
- D-Bus service file if using D-Bus activation;
- Polkit action file, e.g. `io.fmqml.fm.admin.policy`;
- helper version metadata.

Backend detection:

- verify action/helper exist;
- verify protocol version compatibility;
- expose unavailable reason in `AdminController`;
- do not show admin mode as active if backend checks fail.

## 10. Security Checklist

Before enabling by default:

- no shell execution in helper;
- no relative paths accepted;
- no QML password handling;
- helper protocol versioned and schema-validated;
- caller UID/session verified;
- file descriptor based operations where practical;
- symlink policy tested;
- protected-root denylist tested;
- provider/archive/remote paths cannot route to helper;
- cancellation cannot leave ambiguous partial files;
- `.part` cleanup is tested;
- operation history identifies privileged actions;
- logs do not include secrets or auth text.

## 11. Testing Strategy

### 11.1. Unit tests

- `LinuxAdminSession` state transitions and idle timeout;
- app-side lock despite Polkit cache;
- operation eligibility/preflight;
- request serialization/schema validation;
- path policy: relative paths, symlinks, mount roots, pseudo-filesystems,
  denylisted directories;
- error mapping;
- controller state formatting.

### 11.2. Fake-helper integration tests

Use an unprivileged fake helper in normal CI:

- successful copy/mkdir/atomic replace;
- progress events;
- cancellation;
- authorization expired;
- backend unavailable;
- permission denied;
- invalid path;
- protocol version mismatch.

### 11.3. Opt-in privileged tests

Run only in a container/VM:

- create root-owned temp directory;
- unlock through test backend;
- copy user-owned file into root-owned directory;
- atomic replace root-owned file;
- cancel large privileged copy and verify `.part` cleanup;
- attempt denied symlink traversal;
- attempt denied protected-root destructive operation.

### 11.4. Manual E2E scenarios

1. Paste into `/etc/test-fmqml` while locked; verify `Retry as Administrator`.
2. Unlock once and paste multiple files without repeated prompts.
3. Wait past idle timeout; verify the next privileged operation requires unlock.
4. Lock manually; verify future privileged operations require unlock.
5. Replace a protected file; verify atomic result and no leftover `.part`.
6. Cancel a large privileged copy; verify cleanup.
7. Verify Windows admin relaunch behavior is unchanged.

## 12. Implementation Phases

### Current implementation status

Completed so far:

- `AdminController` exposes Linux admin-mode state, timeout, backend status, and
  `adminModeActive`, safety-warning acknowledgement, and unavailable reason
  while keeping Windows relaunch behavior separate.
- `LinuxAdminSession` owns app-side state transitions, idle timeout, manual
  lock, expiry, and refresh-after-operation semantics.
- `LinuxAdminBroker` defines a versioned request schema and has an unprivileged
  fake backend for copy file, create directory, and atomic replace.
- `fm-admin-helper` now exists as a Linux helper-process scaffold. It reads the
  versioned JSON request from stdin, validates it through the shared broker
  parser and policy, executes local file operations for mkdir, copy file, and
  atomic replace, applies helper-side symlink-component rejection for
  destination paths, copies file data through `O_NOFOLLOW` descriptors, and
  returns a structured JSON result.
- `LinuxAdminBroker` detects the helper in build-dir and installed layouts, and
  accepts it only after a `--probe` protocol-version check succeeds.
- Installed helper launches are routed through `pkexec`, with a generated
  Polkit policy bound to the configured libexec helper path. The policy uses
  `auth_admin` rather than `auth_admin_keep`; FMQml owns the short admin window
  by keeping a helper session alive, and Polkit must not extend access after FM
  locks or expires that session.
- `Unlock administrator mode` now starts a privileged `pkexec` helper session
  before the app-side session becomes active; subsequent administrator
  operations reuse that session instead of launching a new `pkexec` prompt.
- The persistent helper `QProcess` is owned from a dedicated broker session
  thread so UI unlocks, worker-thread file operations, and session revocation do
  not touch the same socket notifiers from different Qt threads.
- Broker shutdown now explicitly stops the privileged helper session from that
  same session thread before the application exits.
- Installed helper sessions now carry a generated per-session nonce; brokered
  administrator operations must present the active nonce and the session helper
  rejects mismatches.
- The build-dir direct helper is treated as a test/development helper only and
  no longer makes Linux administrator mode available in the UI.
- `LinuxAdminPolicy` performs conservative app-side validation for local
  absolute paths, provider/archive paths, pseudo-filesystems, and symlink
  policy.
- `OperationQueue` routes copied local regular files, conflict replace through
  atomic replace, and protected folder creation through the active administrator
  helper session when `fm-admin-helper` is installed and unlocked.
- Administrator mode also supports creating a single protected file, renaming a
  single local item in place, and deleting one local file or directory.
- UI entry points exist for `Paste as Administrator` and
  `Create Folder as Administrator` in the command palette and file-panel
  context menus, gated by `AdminController::adminModeActive`. The user must
  explicitly unlock administrator mode first through the command palette; after
  manual lock or idle expiry, administrator file actions disappear again.
- `Create Folder as Administrator` now matches the normal local folder naming
  behavior by creating `New Folder (1)`, `New Folder (2)`, and so on when the
  requested folder name already exists.
- Normal CI covers broker, policy, and session behavior without requiring root.
- Normal CI also covers the helper CLI for mkdir, copy file, atomic replace,
  session nonce enforcement, cancellation, recursive delete, invalid path, and
  symlink destination rejection without requiring root.

MVP status:

- The administrator route supports copied regular files/directories, protected
  mkdir, protected file creation, in-place rename, atomic replace, and deleting
  one local item including recursive directory delete.
- The command palette and context menus hide administrator file-operation
  entries unless administrator mode is currently active or expiring soon.
- Running from the build directory does not provide administrator mode unless
  the app is installed into the Polkit-enabled layout.
- Manual lock and idle expiry revoke the privileged helper session.
- Successful administrator operations refresh the visible app-side idle timeout.
- After manual lock or idle expiry, the next administrator session must require
  a fresh system authentication prompt before any privileged operation runs.
- Administrator file actions do not prompt for unlock themselves; the command
  palette remains the explicit entry point for unlocking administrator mode.
- `Paste as Administrator` supports copied items only, not cut/move.
- Recursive delete, helper progress events, and helper cancellation are
  implemented for the current helper-process route.

Post-MVP hardening/backlog:

- The helper copy/replace data path uses POSIX descriptors with `O_NOFOLLOW`,
  but parent traversal is not fully `openat`/descriptor based yet.
- Add opt-in privileged install/E2E tests in a container or VM.
- Archive extraction and cut/move through the administrator route are not
  implemented yet.
- Broaden administrator delete to multi-item workflows only if that becomes a
  product requirement.

### Phase 0. Backend spike

- Choose Polkit D-Bus service vs short-lived helper process.
- Prototype authentication and one `mkdir` request.
- Record package dependencies and install paths.
- Confirm default idle timeout behavior.

Output: backend decision note and minimal protocol draft.

### Phase 1. UI/session skeleton

- Extend `AdminController` with Linux state.
- Implement `LinuxAdminSession` with idle timeout and manual lock.
- Add toolbar/status indicator and command palette actions.
- Use fake backend only.

Output: UI can show locked/active/expired/unavailable state without privileged
writes.

### Phase 2. Protocol and fake-helper route

- Define versioned request/response schema.
- Implement `LinuxAdminBroker` with fake helper.
- Wire `OperationQueue` admin route for copy file, mkdir, atomic replace.
- Add unit and fake-helper integration tests.

Output: CI can exercise admin operation routing without root.

### Phase 3. Real Polkit MVP

- Implement `fm-admin-helper` for copy file, mkdir, atomic replace.
  - Current status: helper-process scaffold implements mkdir, copy file, and
    atomic replace through local file operations with JSON request/result
    protocol, including helper-side destination symlink-component rejection and
    `O_NOFOLLOW` source/destination file opens.
  - Current status: the helper also implements create file, in-place rename,
    recursive delete, progress events, session nonce enforcement, and
    cancel-marker based cancellation for persistent helper sessions.
  - Next: harden parent traversal with `openat`, add stronger root-side cleanup
    guarantees, and add privileged install/E2E tests.
- Add Polkit action/service packaging.
  - Current status: a generated pkexec policy is installed to
    `share/polkit-1/actions`, and the installed helper path is launched through
    `pkexec`.
- Add backend detection/version checks.
  - Current status: helper-process detection checks executable presence and
    protocol version through `fm-admin-helper --probe`, and unlock requires a
    privileged `pkexec fm-admin-helper --session <nonce>` before activating the
    session.
- Add structured error mapping.

Output: protected file writes work on Linux without running the GUI as root.

### Phase 4. Cancellation and hardening

- Add robust cancel handling.
- Verify partial cleanup.
- Harden path/symlink handling.
- Add opt-in privileged container tests as release hardening.

Output: MVP is safe enough for real protected local writes.

### Phase 5. Later workflows

- Recursive directory copy.
- Guarded move/rename.
- Broaden delete workflows only if multi-item admin delete becomes a product
  requirement.
- Protected editing flow.
- Archive extraction to protected destinations after separate review.

Output: admin mode grows only after the core helper contract is proven.

## 13. MVP Closure Checklist

Status: complete for the current Linux admin-mode MVP.

Closure criteria:

- Linux admin UI appears only when backend is installed and compatible;
- GUI remains unprivileged;
- unlock uses Polkit/system auth, not QML password collection;
- one unlock supports a short planned batch without prompt spam;
- idle timeout and manual lock work;
- copy file to protected local directory works with progress/cancel;
- protected mkdir works;
- protected atomic replace works with safe `.part` cleanup;
- protected file creation, in-place rename, and recursive delete work for one
  local item at a time;
- active helper copy/delete operations can be cancelled;
- provider/archive/remote paths cannot route to helper;
- symlink and denylist tests pass;
- broker, policy, session, and helper CLI tests run in normal CI;
- Windows admin relaunch behavior is unchanged.
