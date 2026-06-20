# Cleanup Subsystem Plan

Goal: make every FM-owned temporary or staged payload explicit,
destination-aware, tracked, and eventually removed. FM must not silently spend
space on the system drive when the user selected another destination drive for
the operation.

This plan expands `suggest/19-staging-and-large-temporary-io.md` into an
implementable subsystem.

## Policy

Temporary storage is part of the file operation contract.

For any temporary file, partial file, extracted tree, materialized provider
file, preview materialization, archive staging tree, upload/download staging
file, or compatibility/runtime payload that is not normal app settings:

- prefer the destination parent when there is a known local destination;
- create FM-owned hidden staging roots under that destination parent;
- if the destination parent is unavailable or not meaningful, use the nearest
  user-chosen local storage root;
- use a default app-owned writable location only when no reasonable local
  destination/source candidate exists;
- never assume the application install directory is writable;
- never silently fall back to system temp for large user-data payloads;
- record every created artifact before or immediately after creation;
- remove artifacts on success, cancel, and failure;
- recover after crashes by cleaning recorded stale artifacts on a later start.

The guiding rule is: the disk that benefits from the operation should pay for
the temporary IO. Copying from `E:` to `D:` should not fill `C:`.

## Definitions

- **Artifact:** any FM-created file or directory that is not user content and
  not persistent settings/state.
- **Payload artifact:** an artifact that can scale with user data size.
- **Destination-near staging:** a hidden FM-owned directory under the
  destination parent, such as `<destination-parent>/.fm-tmp/<operation-id>/`.
- **Default cleanup root:** an app-owned writable location selected through
  `QStandardPaths::writableLocation(QStandardPaths::CacheLocation)` or another
  explicit writable app data path, never the install directory.
- **Lease:** a cleanup record with lifecycle state, creation time, last touch,
  kind, root, artifact paths, and deletion rules.

## Non-Goals

- Do not change app settings, theme files, auth tokens, persistent provider
  caches, or user-created files into cleanup-managed temp files.
- Do not implement outgoing OS drag staging in this pass.
- Do not expose staging paths in breadcrumbs, history, clipboard, recent files,
  or normal user-facing errors.
- Do not add broad automatic cleanup of arbitrary `.part` files not known to be
  created by FM.

## Current Inventory

This is a first-pass code inventory, not an exhaustive audit.

### OperationQueue

Relevant file: `src/core/OperationQueue.cpp`.

Current good patterns:

- local destination copies write to `<target>.part` beside the final target and
  then rename/move into place;
- archive extraction to a local destination can target `<target>.part`;
- many cancel/error paths remove the temporary output immediately.

Current risks:

- provider-to-provider transfer uses `QTemporaryFile` under `QDir::temp()`
  through `providerTransferTempTemplate(...)`;
- stale provider transfer cleanup scans the global temp folder for
  `fm-provider-transfer-*`;
- `.part` files are operation-owned but not persistently registered, so a
  process kill can leave them behind;
- provider fallback paths may remove temp files inline but cannot recover after
  crash unless a later startup scan can identify them.

Target behavior:

- local-destination transfers keep using destination-near `.part` or
  `.fm-tmp/<operation-id>/`;
- provider-to-provider copies must stage through a cleanup-managed default root
  only when no destination-local root exists;
- every `.part` and provider-transfer staging file must be registered before
  large writes begin.

### ArchiveFileProvider

Relevant file: `src/core/ArchiveFileProvider.cpp`.

Current good patterns:

- extraction to local destinations already creates temp extraction folders under
  or near the destination parent in several paths;
- temporary directories are often released asynchronously to avoid UI stalls;
- nested archive APIs accept a `temporaryParentPath` in some flows.

Current risks:

- fallback archive temp parent uses an app data `temporary-archives` location;
- some `QTemporaryDir()` calls have no explicit parent and therefore may land in
  system temp;
- async deletion is not backed by a persistent cleanup registry;
- crash during archive browse/materialization can leave extracted trees behind.

Target behavior:

- extraction uses destination-near staging whenever a destination exists;
- nested archive browse/materialization uses a caller-provided physical parent
  where available, preferably beside the physical source archive or
  destination;
- fallback archive roots are cleanup-managed default roots with size limits,
  leases, and startup recovery.

### QuickLookController

Relevant file: `src/controllers/QuickLookController.cpp`.

Current good patterns:

- remote previews are size-limited;
- materialized preview directories are removed when preview state is cleared;
- deletion validates that a directory is inside the expected preview root.

Current risks:

- `remotePreviewRoot()` is under `QCoreApplication::applicationDirPath()`,
  which may be read-only on Linux or installed deployments;
- cleanup is in-memory only, so process death can leave preview files;
- the preview root is remote-preview specific rather than using a shared
  cleanup registry.

Target behavior:

- use CleanupSubsystem to allocate remote-preview leases;
- for provider preview with no destination, use a default cleanup root that is
  explicitly writable;
- preserve size caps and cleanup validation;
- recover stale remote preview materializations on next start.

### ThumbnailProvider

Relevant file: `src/core/ThumbnailProvider.cpp`.

Current risks:

- archive/provider thumbnail paths can write `QTemporaryFile` without an
  explicit destination-aware parent;
- Windows thumbnail extraction fallback can use `QDir::tempPath()` with
  `fm-thumb-*`;
- thumbnail temps are usually small but can still be user-data-derived and must
  not assume system temp or install-dir writability.

Target behavior:

- keep pure in-memory thumbnail caches out of cleanup registry;
- any temporary file created to adapt an archive/provider path for a thumbnail
  must use a cleanup-managed short-lived lease;
- use source-near staging if the physical source parent is known, otherwise use
  default cleanup root.

### Provider Plugins

Relevant files include:

- `src/plugins/gdrive/GDriveFileProviderPlugin.cpp`
- `src/plugins/portable_device/PortableDeviceFileProviderPlugin.cpp`
- future FTP/SFTP/remote provider implementations

Current good patterns:

- Google Drive `copyToLocalFile(...)` writes to a caller-provided destination
  path, so `OperationQueue` can place downloads destination-near;
- Google Drive action "Download as PDF" targets the opposite panel local folder
  and does not need default temp staging.

Current risks:

- portable device `openRead(..., stagingParentPath)` falls back to
  `QDir::tempPath()` when no staging parent is provided;
- provider APIs do not clearly distinguish "caller supplied destination-near
  staging" from "provider picked fallback temp";
- future remote providers may accidentally introduce default temp downloads.

Target behavior:

- provider `openRead(path, stagingParentPath)` must fail or use a
  cleanup-managed fallback when `stagingParentPath` is empty and a local temp
  file is required;
- provider copy APIs should prefer streaming directly to caller-owned output;
- any provider-created local materialization must be registered as a cleanup
  artifact.

### Launch and Runtime Compatibility Data

Relevant files include:

- `src/core/LaunchService.cpp`
- `src/core/TerminalLauncher.cpp`

Current risks:

- launch compatibility data may use fallback temp roots;
- terminal session files use `QTemporaryFile` under `QDir::tempPath()`.

Target behavior:

- classify tiny control files separately from payload artifacts;
- tiny short-lived session files may use default writable cleanup root, but
  should still avoid install-dir assumptions;
- compatibility data that persists by design must be documented as persistent
  app data, not cleanup-managed temp.

## Architecture

Add a small C++ cleanup subsystem under `src/core`, with no QML ownership of
cleanup policy.

Suggested types:

- `CleanupSubsystem`
- `CleanupLease`
- `CleanupArtifact`
- `StagingLocationPolicy`
- `CleanupRegistry`

### CleanupSubsystem

Responsibilities:

- choose staging roots from a policy request;
- create operation-scoped directories;
- create registered file paths for `.part`, provider transfer files, archive
  extraction roots, preview materializations, and thumbnails;
- persist the registry atomically;
- delete artifacts asynchronously;
- run conservative stale cleanup at startup;
- expose diagnostics for tests and troubleshooting.

It should not know how to copy files, extract archives, or download remote
content. It only owns location selection, registration, and deletion.

### StagingLocationPolicy

Inputs:

- kind: copy, move, archive-extract, archive-browse, nested-archive,
  provider-transfer, remote-preview, thumbnail-adapter, launch-session, other;
- destination path if known;
- source path if known;
- provider scheme/source kind;
- expected payload size if known;
- whether the artifact must be on the same filesystem as the final target for
  atomic rename;
- whether default cleanup root is allowed.

Resolution order:

1. destination parent, when known, local, writable, and semantically correct;
2. source physical parent, when destination does not exist and source-near is
   the only user-chosen storage root;
3. caller-provided staging parent;
4. cleanup-managed default writable root;
5. fail with a clear error.

The policy must not fall back silently. If the chosen root cannot be created or
written, return an explicit error that can be shown in operation status.

### Cleanup Registry

Persist leases in an app-owned writable data location, separate from the
artifacts themselves. A JSON file is enough initially if writes are atomic.

Each lease should record:

- lease id;
- subsystem/kind;
- creation time and last touch time;
- state: active, finalize-pending, delete-pending, deleted, failed;
- process/session id when created;
- cleanup root;
- one or more artifact paths;
- path type: file, directory, glob pattern only if unavoidable;
- deletion safety root;
- whether recursive deletion is allowed;
- expected owner marker path or owner token;
- optional final destination path for diagnostics only.

Safety rules:

- recursive deletion is allowed only for directories inside an FM-owned staging
  root that contains an owner marker;
- never delete a path that is equal to a destination parent, source parent,
  drive root, home directory, temp root, or app data root;
- never delete arbitrary files by extension or age unless they are registered or
  inside a known FM-owned staging root;
- deletion should be best-effort but failures must stay visible in diagnostics
  and be retried later.

### Owner Markers

Every staging directory should contain a small marker file, for example:

`<staging-root>/.fm-cleanup-owner.json`

The marker should include:

- app name and schema version;
- lease id;
- created timestamp;
- purpose/kind;
- cleanup registry path or registry instance id.

Startup cleanup may delete an unregistered directory only if it is under a known
FM-owned parent and has a valid owner marker. This handles registry write
failures and partially created staging directories.

## Cleanup Semantics

### Success

- final output is moved into place;
- lease is marked finalize-pending, then delete-pending;
- cleanup runs asynchronously;
- registry marks deleted only after deletion succeeds or path no longer exists.

### Cancel or Error

- operation stops producing data;
- partial outputs are closed;
- lease is marked delete-pending;
- cleanup runs asynchronously;
- user-visible operation status reports the operation failure separately from
  cleanup retry status.

### Crash, SIGKILL, BSOD, Power Loss

At next startup:

- load registry;
- mark leases from dead sessions as stale candidates;
- delete stale active/delete-pending artifacts after safety validation;
- scan known FM-owned staging parent directories for marker files not present in
  the registry;
- never delete unmarked directories;
- throttle cleanup so startup is not blocked by large recursive deletes.

### App Uninstall

The app cannot guarantee cleanup after uninstall. The subsystem should keep
artifacts in recognizable FM-owned roots so a user or uninstaller can identify
them. Runtime cleanup still handles normal restarts.

## Migration Plan

### Phase 1: Audit and Tests

1. Add a temporary-file inventory test or script that scans for:
   - `QTemporaryFile`
   - `QTemporaryDir`
   - `QDir::tempPath`
   - `QDir::temp`
   - `.part`
   - `.fm-tmp`
   - `removeRecursively`
2. Classify each hit as:
   - payload artifact;
   - tiny control artifact;
   - persistent app data;
   - in-memory cache only;
   - false positive.
3. Add the inventory to docs and keep it current during migration.

Verify:

- inventory lists `OperationQueue`, `ArchiveFileProvider`,
  `QuickLookController`, `ThumbnailProvider`, portable device provider,
  `LaunchService`, and `TerminalLauncher`;
- every payload artifact has an owner plan.

### Phase 2: CleanupSubsystem Skeleton

1. Add `CleanupSubsystem` with staging root resolution and registry persistence.
2. Add owner marker creation.
3. Add validated async deletion.
4. Add startup stale cleanup entry point.
5. Add unit tests for path safety and registry recovery.

Verify:

- cannot recursively delete outside a registered FM-owned root;
- default root is writable and does not use application install path;
- simulated stale lease is removed on startup;
- failed deletion remains pending and is retried.

### Phase 3: OperationQueue Migration

1. Register destination-near `.part` files before writes begin.
2. Replace provider-to-provider `QDir::temp()` staging with cleanup-managed
   provider-transfer leases.
3. Remove global-temp scanning for `fm-provider-transfer-*`.
4. Ensure cancel/error/success transitions update leases.

Verify:

- copy `E:` to `D:` creates temporary payloads under `D:`;
- provider-to-local download stages under the local destination parent;
- provider-to-provider transfer uses default cleanup root only when no local
  destination exists;
- crash with an active `.part` file is cleaned on next start.

### Phase 4: Archive Migration

1. Route archive extraction temp directories through CleanupSubsystem.
2. Route nested archive materialization through source-near or destination-near
   staging when possible.
3. Replace implicit `QTemporaryDir()` fallbacks with explicit policy decisions.
4. Keep async deletion but make it registry-backed.

Verify:

- extracting archive entries to `D:` creates staging under `D:`;
- browsing nested archive materialization beside a physical archive does not use
  system temp when a source parent is available;
- killing the app during extraction leaves only registered FM-owned artifacts
  that are removed on next start.

### Phase 5: Preview, Thumbnail, and Provider Migration

1. Move remote preview materialization off `applicationDirPath()` and into a
   cleanup-managed default writable root.
2. Register remote preview directories and audio-cover materializations.
3. Route thumbnail adapter temp files through CleanupSubsystem.
4. Update portable device provider fallback staging.
5. Add provider API guidance: providers should stream to caller-owned paths and
   avoid private temp roots.

Verify:

- installed Linux build can preview remote files without writing under `/bin` or
  the install directory;
- remote preview cleanup happens when preview changes and after restart;
- thumbnails do not create untracked temp files;
- provider fallback temp files are registered.

### Phase 6: Diagnostics and QA

1. Add debug logging behind an environment flag, such as `FM_CLEANUP_TRACE=1`.
2. Add a developer command or test hook to list active/stale leases.
3. Add QA cases to `docs/qa-regression-suite.md`.
4. Add regression tests for startup cleanup.

Verify:

- trace logs show root selection reason and cleanup result;
- QA can intentionally kill the app mid-copy and verify cleanup on next start;
- cleanup never deletes user-created files placed near `.fm-tmp` but outside
  owned staging roots.

## QA Cases

Add focused cases when implementation starts:

- Copy a large file from `E:` to `D:`. Expected: temporary payload appears under
  `D:` or destination parent, not `C:` or AppData.
- Cancel that copy. Expected: temporary payload is removed asynchronously.
- Kill the app during that copy. Expected: next start removes the registered
  partial payload.
- Extract a large archive to `D:`. Expected: extraction staging is destination
  near and is removed after success/failure.
- Browse nested archives. Expected: materialization uses source-near staging
  when there is a physical local source.
- Preview a Google Drive file in an installed Linux build. Expected: staging
  uses a writable default cleanup root, not the install directory.
- Preview a remote file larger than the preview cap. Expected: no large staging
  file remains.
- Provider-to-provider copy. Expected: default cleanup root is used only because
  no local destination exists, and stale files are registered.
- Put a user file named similarly to FM temp files outside an FM-owned staging
  root. Expected: cleanup does not delete it.
- Simulate deletion failure by locking a temp file. Expected: cleanup retries on
  a later run and reports the pending lease in diagnostics.

## Open Questions

- Should local destination-near `.part` files be placed directly beside the
  final target or under `.fm-tmp/<operation-id>/` for all operations? Direct
  `.part` simplifies atomic rename; `.fm-tmp` simplifies ownership markers.
- How aggressive should startup cleanup be for very large stale directories?
  A background delayed cleanup is safer for startup latency.
- Should the user get a visible notification if cleanup repeatedly fails, or is
  developer diagnostics enough initially?
- Should default cleanup root size be capped for remote previews/provider
  transfers, and if so where should the limit be configured?

## Acceptance Criteria

The feature is complete when:

- all payload temporary files are allocated through CleanupSubsystem;
- destination-known operations stage on the destination side;
- default fallback staging uses an explicitly writable app-owned root;
- every payload artifact has a persistent cleanup lease;
- success/cancel/error paths schedule cleanup;
- startup cleanup recovers from process kill and crash scenarios;
- recursive deletion is safety-checked and marker-based;
- docs and QA cover the policy and known exceptions.
