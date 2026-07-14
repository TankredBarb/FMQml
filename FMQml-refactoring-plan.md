# FMQml Large-File Refactoring Research and Plan

## Audit baseline

- Repository: `TankredBarb/FMQml`
- Branch: `main`
- Audited commit: `6f6fd48f0edb79c17b1a607ec2a04e122d4071cb`
- Commit subject: `eliminated some deadcode`
- Scope: C++/headers and QML, with special attention to the largest files and their responsibility boundaries.
- Goal: improve structure, ownership, readability, and future change safety without changing runtime behavior, public QML contracts, plugin behavior, persistence, timing semantics, or performance.

The current tree contains 148,809 lines across tracked `.cpp`, `.h`, and `.qml` files. C++ implementation files account for 82,455 lines and QML for 59,295 lines. Forty files exceed 1,000 lines and twelve exceed 2,000 lines. Size alone is not the deciding factor: the priority below also considers mixed responsibilities, long functions, asynchronous state, platform branches, and how widely a file affects the application.

## Executive conclusion

There are seven genuine architectural monsters:

1. `src/core/OperationQueue.cpp` — 5,662 lines.
2. `qml/components/FilePanel.qml` — 4,371 lines.
3. `src/plugins/gdrive/GDriveFileProviderPlugin.cpp` — 4,855 lines.
4. `src/controllers/FilePanelController.cpp` — 3,803 lines.
5. `src/controllers/QuickLookController.cpp` — 3,346 lines.
6. `src/core/ArchiveFileProvider.cpp` — 3,589 lines.
7. `src/models/DirectoryModel.cpp` — 3,086 lines.

The most important finding is that these files need two different kinds of work:

- First, **safe structural decomposition**: move existing definitions into logical translation units or QML components without changing ownership, control flow, signal order, or public APIs.
- Only after that, **selective responsibility extraction**: introduce small internal collaborators where the code already has a stable seam.

Trying to perform both at once would create unnecessary regression risk. The first pass should be deliberately boring and mechanically reviewable.

## Priority matrix

| Priority | File | Lines | Main problem | Recommended first action | Risk |
| --- | ---: | ---: | --- | --- | --- |
| P0 | `src/core/OperationQueue.cpp` | 5,662 | Queue/UI state, execution, copy matrix, provider batching, archives, admin mode, platform tuning all coexist | Split method definitions into subsystem `.cpp` files while keeping `OperationQueue` intact | High |
| P0 | `qml/components/FilePanel.qml` | 4,371 | 238 properties, 206 functions, focus, views, selection, rename, scrolling, hover, DnD, context menus and rendering in one scope | Extract existing visual roots and coordinators with explicit contracts; retain root facade | Very high |
| P0 | `src/plugins/gdrive/GDriveFileProviderPlugin.cpp` | 4,855 | Plugin shell, provider, HTTP API, transfers, retries, caches, exports and thumbnails in one file | Make the plugin file a thin factory/action shell and move private implementation out | Medium-high |
| P1 | `src/controllers/FilePanelController.cpp` | 3,803 | Navigation, autocomplete, launch, mutation, admin, filter, metadata and path formatting mixed together | Multi-TU split, then extract pure navigation/suggestion helpers | Medium-high |
| P1 | `src/controllers/QuickLookController.cpp` | 3,346 | Format classification, file reading, provider materialization, metadata, FB2 parsing and UI state application mixed together | Extract loaders/helpers first; break up the 869-line `previewPath()` second | High |
| P1 | `src/core/ArchiveFileProvider.cpp` | 3,589 | Provider adapter, archive catalog/cache, extraction engine, nested resolution, temp lifecycle and process throttling mixed | Separate extraction/internal infrastructure from browse/provider code | High |
| P1 | `src/models/DirectoryModel.cpp` | 3,086 | Model storage, loading, watchers, mutations, selection, filtering and sorting in one implementation | Multi-TU split without changing model ownership or index semantics | High |
| P2 | `qml/components/PropertiesDialog.qml` | 2,596 | Five tab bodies plus fourteen inline controls | Extract controls and complete tab roots | Low-medium |
| P2 | `qml/components/SettingsDialog.qml` | 2,092 | Settings UI, provider authentication flows and maintenance dialogs | Extract sections and provider-account panels; keep backend calls unchanged | Low-medium |
| P2 | `qml/components/StorageView.qml` | 1,871 | System summary, drive cards, quick access, model snapshots and keyboard navigation | Extract visual cards and navigation coordinator | Medium |
| P2 | `src/controllers/WorkspaceController.cpp` | 2,066 | Panel orchestration, DnD, clipboard, delete, archive/ISO/volume and history | Multi-TU split after OperationQueue/FilePanel seams stabilize | Medium |
| P2 | `qml/App.qml` | 1,959 | Composition root also owns persistence, command facade, admin mode and preview suppression | Extract state coordinators, but keep root action wrappers | Medium-high |

## Refactoring rules that must not be violated

1. Freeze all public `Q_PROPERTY`, `Q_INVOKABLE`, signals, QML component names, plugin interfaces, model roles, settings keys and serialized formats during the structural pass.
2. Do not rename functions while moving them. Do not change signatures, defaults, exception behavior, signal order, queued/direct connection types, timer intervals or async generation checks.
3. Do not mix formatting, dead-code removal, performance tuning or feature work into a structural move.
4. Move one responsibility group per commit. Every commit must build and be independently revertible.
5. Preserve `#ifdef` placement and validate both Linux and Windows compilation paths at the steps that touch platform-specific code. A Linux-only successful build is not enough for files containing native Windows code.
6. For QML, preserve the visual root type when extracting a subtree. Avoid adding wrapper `Item`s that change coordinates, clipping, focus propagation, implicit size or stacking.
7. QML `id`s do not cross component boundaries. Every extracted component must receive an explicit `required property`, callback, signal or alias; it must not reach through a large untyped context object unless that is only a temporary first-pass seam.
8. Do not introduce a generic “operation framework”, “preview framework” or similar abstraction merely to reduce line counts. Extract concrete responsibilities already visible in the code.
9. Line-count targets below are guidance, not acceptance criteria. A cohesive 1,200-line implementation is preferable to fifteen coupled 100-line wrappers.

## P0.1 — `OperationQueue.cpp`

### Why it is the first C++ target

The file is not just long. It combines:

- the QObject/QML-facing queue facade and status properties;
- request scheduling and `QFutureWatcher` lifecycle;
- conflict synchronization with `QMutex`/`QWaitCondition`;
- metrics, speed, ETA and provider timing diagnostics;
- drive/storage detection and platform-specific copy tuning;
- request dispatch in `execute()` (about 731 lines);
- archive extraction and compression;
- all local/provider copy combinations;
- staged provider-to-provider transfer;
- admin copy/create/delete operations;
- low-level provider/path helpers.

The largest individual regions are `copyPath()` at roughly 884 lines, `execute()` at roughly 731 lines, provider-to-provider staged directory transfer at roughly 472 lines, and provider-file staged transfer at roughly 485 lines. This makes reviews nearly impossible and encourages cross-subsystem state access.

### Phase A — mechanical multi-translation-unit split

Keep the class and `OperationQueue.h` public surface unchanged. Split only definitions:

- `OperationQueue.cpp`
  - constructor/destructor;
  - queueing (`enqueue`, `runNext`, `finishCurrent`);
  - QObject properties, errors, retry/cancel and conflict UI bridge;
  - target size: roughly 700–900 lines.
- `OperationQueueExecution.cpp`
  - `execute()` and request result aggregation;
  - request-level setup/teardown and abort reporter scope.
- `OperationQueueCopy.cpp`
  - `copyPath`, `movePath`, duplicate naming and generic provider/path operations.
- `OperationQueueProviderTransfers.cpp`
  - all five batch/staged local/provider transfer methods;
  - `CopyFrame`, batch wave constants and provider transfer timing helpers.
- `OperationQueueArchive.cpp`
  - total-byte estimation for archives;
  - archive extraction and 7-Zip compression entry points.
- `OperationQueueAdmin.cpp`
  - admin copy/create/delete methods and Linux admin nonce handling.
- `OperationQueuePlatform.cpp`
  - drive type detection, buffer sizing and OS-specific copy primitives.
- `OperationQueuePrivate.h`
  - internal-only shared structs/constants needed by more than one translation unit;
  - never installed and never exposed to QML.

Important: this phase must not create new runtime objects. It is a source-layout change only.

### Phase B — reduce the two giant decision trees

After Phase A is green, extract concrete internal helpers:

- `OperationExecutionContext`: immutable request plus callbacks for abort, byte progress, label/status and conflict resolution.
- `OperationResultAccumulator`: succeeded/failed paths and first-error aggregation currently repeated across request branches.
- `ProviderTransferEngine`: batch upload/download/staging only; it receives providers and context, but does not own queue/UI state.
- `ArchiveOperationRunner`: extraction/compression orchestration only.
- `AdminOperationRunner`: privileged operations only.

Keep queue state, timers, properties and signals in `OperationQueue`. Do not turn every operation type into a QObject or inheritance hierarchy.

### Characterization tests required before Phase A

- Request ordering: enqueue two operations and verify strict serial execution.
- Cancel before start, cancel during copy, cancel while waiting for conflict, cancel during provider batch.
- Conflict results: Replace, Skip, KeepBoth, Cancel and Apply to all.
- Partial failure counts and `failedPaths` ordering.
- Copy/move matrix: local→local, local→provider, provider→local, provider→same provider, provider→different provider.
- Exact-destination copy and duplicate naming.
- Archive extract/compress and admin operations.
- Signal trace: `operationStarted`, busy/progress/status changes, detailed finish and `operationFinished` in their current order.

### Acceptance criteria

- No public header change in Phase A.
- Same labels, progress monotonicity, conflict behavior, staging cleanup and partial-failure reporting.
- No measurable regression on small-file batches or large local copy.
- The supported unity-build configuration compiles cleanly. A non-unity build is not a project target and must not be introduced solely for this refactoring campaign.

## P0.2 — `FilePanel.qml`

### Why it is the most dangerous QML target

The root currently owns 238 properties and 206 functions. The file contains:

- details-column sizing and persistence;
- fourteen timers and multiple `Connections` blocks;
- loading and view-reuse policies;
- scroll restoration/current-index/reveal state machines;
- hover preview and context-menu lazy creation;
- inline rename and create-then-rename recovery;
- rubber-band selection and auto-scroll;
- internal drag affordances;
- keyboard routing;
- list/grid/brief/details/storage/favorites visual hosts and delegates.

There are already good policy components in `qml/components/filepanel/`, which means the intended architecture exists, but the root still acts as both composition shell and implementation dump.

### Phase A — extract leaf visual components, no state migration

Start with pieces that already have a visual root and do not own focus/navigation:

- Extract lazy menu loaders into `FilePanelMenuHost.qml`.
- Extract loading/error/hover overlay composition into `FilePanelOverlayHost.qml`.
- Inventory the large entry/delegate body's dependencies on root `id`s, selection, focus, rename, hover and DnD before extracting it into `FilePanelEntryDelegate.qml`. Proceed only if it can receive a bounded explicit contract while preserving its current root item type; otherwise defer it until the related coordinators are stable.
- Keep existing root functions as callbacks initially; pass only the exact functions/properties used by each leaf.

This should remove visual bulk without changing the panel state machine.

### Phase B — complete the existing policy decomposition

Move coherent function/state clusters into named QML coordinators:

- `FilePanelCurrentIndexPolicy.qml`
  - current-index initialization, reveal target and pending navigation commit.
- `FilePanelScrollCoordinator.qml`
  - scroll capture/restore, view-mode anchors and pending restore retries.
- `FilePanelRenameCoordinator.qml`
  - pending inline focus, create-rename session and recovery.
- `FilePanelHoverCoordinator.qml`
  - hover suppression, hover card positioning/actions and thumbnail source requests.
- `FilePanelContextMenuCoordinator.qml`
  - lazy menu creation, pending request and focus restoration.
- Expand the existing `FilePanelRubberBandPolicy.qml`
  - absorb candidate-row calculation, rectangle mapping, selection commit and auto-scroll currently still in the root.

Each coordinator should expose a small explicit state and action API. The root may keep compatibility wrapper functions so `App.qml`, shortcuts and other components do not change in the same commit.

### Phase C — extract the main view host

Replace the existing `FilePanelShell { ... }` subtree with a separate `FilePanelContent.qml` whose root is still `FilePanelShell`. It should expose aliases for the active list/grid/brief/storage/favorites views required by the root facade. This avoids an additional scene-graph wrapper.

Target end-state for `FilePanel.qml`: composition, public properties/signals, policy wiring and compatibility wrappers, ideally around 1,000–1,400 lines.

### QML-specific regression gate

- All view modes: details/list, grid, brief, storage and favorites.
- Mouse and keyboard selection, range selection, invert and rubber band in every file view.
- Current index after navigation, refresh, filter, rename, create and view-mode switch.
- Scroll restoration per path and per view mode.
- Inline rename focus, selection range, Escape/cancel and create-then-rename.
- Hover previews during normal use, scroll, external scroll and resize.
- Internal DnD both disabled and enabled, plus external incoming drops.
- Context menu on item/empty space and focus restoration after closing.
- 144 Hz scrolling/resize sanity and large-folder thumbnail behavior.
- Run with `QML_DISABLE_DISK_CACHE=1`; new binding-loop, undefined-id, anchor, focus or type warnings are failures.

## P0.3 — `GDriveFileProviderPlugin.cpp`

### Diagnosis

This file contains several independently testable subsystems:

- thumbnail worker threads and byte cache near the start;
- filename/MIME/export-format helpers;
- shared entry, shortcut and capability mapping;
- synchronous HTTP request helpers;
- download, multipart upload and resumable upload with retry/cooldown logic;
- blocking listing and account quota requests;
- the 1,200+ line `GDriveFileProvider` class;
- plugin metadata/action methods at the end.

The existing `GDriveAuth`, `GDriveCache`, `GDrivePath` and `GDriveTypes` files show that this plugin already has modular foundations. The monster file is mostly uncompleted decomposition.

### Target split

- `GDriveFileProviderPlugin.cpp`
  - plugin metadata, factory and action dispatch only; target below 400 lines.
- `GDriveFileProvider.h/.cpp`
  - concrete `FileProvider` adapter, instance state and provider overrides.
  - Header remains private to the plugin target; no ABI promise.
- `GDriveApiClient.h/.cpp`
  - authenticated request construction, JSON parsing, list/create/trash/restore/quota calls and retry/cooldown policy.
- `GDriveTransferClient.h/.cpp`
  - download, multipart/resumable upload, progress/cancel and batch transfer results.
- `GDriveThumbnailLoader.h/.cpp`
  - worker pool, network fetch, size/timeout policy and byte cache.
- `GDriveExportPolicy.h/.cpp`
  - Google Apps export format and filename/MIME decisions; prefer pure functions.
- Reuse `GDriveCache` for shared entry/child/capability state instead of adding a second cache abstraction.

### Safe sequence

1. Move the thumbnail block unchanged and verify provider thumbnail identity/cache behavior.
2. Move pure export/MIME/filename helpers.
3. Move HTTP and transfer helpers without changing retry counts, timeout constants or log text.
4. Move the provider class declaration/definitions.
5. Leave plugin action behavior as the final thin shell.

### Verification

- Auth status, login and sign-out.
- My Drive, Shared with me, Shortcuts and Trash browsing.
- Shortcut target metadata and read-only capability mapping.
- Native thumbnail cache hit/miss, timeout, oversize, 401 and 404 handling.
- Google Apps export naming and “Download as PDF”.
- Small multipart upload, resumable upload, retry-after/rate limiting and cancellation.
- Batch upload/download and provider-to-provider staging.
- Create, rename, trash, restore, quota refresh and error text.

## P1.1 — `FilePanelController.cpp`

### Diagnosis

The first 1,133 lines are free helpers before the constructor. They cover admin launch materialization, launch-result mapping, path schemes, navigation resolution, autocomplete, nested archives and category-filter formatting. The class then mixes navigation/history, opening, Open With/Proton, wallpaper/terminal, rename/batch rename/create, properties/metadata, category filters, storage info and recovery.

### Phase A — split member definitions, keep one class

Status: completed on 2026-07-14. The public controller API and state owner were kept unchanged; shared implementation-only helpers are declared in `FilePanelControllerInternal.h`.

- `FilePanelController.cpp`: construction, wiring, simple properties and facade.
- `FilePanelControllerNavigation.cpp`: open/navigation/history/recovery/nested archive/password.
- `FilePanelControllerSuggestions.cpp`: async suggestion request/cancel and result delivery.
- `FilePanelControllerLaunch.cpp`: normal launch, Open With, Proton, terminal, reveal and wallpaper.
- `FilePanelControllerMutations.cpp`: rename, batch rename, create and admin variants.
- `FilePanelControllerProperties.cpp`: properties/metadata/storage information.
- `FilePanelControllerFilters.cpp`: category filter scope/state.

### Phase B — extract pure/internal helpers

- `FilePanelNavigationResolver`: `NavigationResolution`, virtual/provider/archive normalization and missing-path fallback.
- `DirectorySuggestionScanner`: native Windows/Linux enumeration and provider/archive suggestions, with current cancellation/time/entry limits unchanged.
- `LaunchResultPresentation`: conversion from launch/Open With results to the current `QVariantMap` schema.
- `NestedArchiveNavigationPolicy`: scope key, depth/status text and approval target helpers.

Do not split the public controller into multiple QML-facing controllers yet. That would force a broad QML API migration for little immediate gain.

### Verification

- Local/provider/archive navigation, load-more paths, back/forward/up and missing-path recovery.
- Autocomplete on Windows, Linux, archive and provider paths; stale request IDs and cancellation.
- Open With, preferred application, multi-open, Proton and launch-as-admin materialization.
- Single/batch rename, create and admin operations.
- Category-filter suspension/resume across navigation.

## P1.2 — `QuickLookController.cpp`

### Diagnosis

The file has about 1,570 lines of free loaders/helpers before the controller. These include remote preview cleanup, file classification, audio cover extraction, FB2 parsing/pagination, image validation/metadata and local/provider snapshot loading. `previewPath()` alone is roughly 869 lines and interleaves classification, async work, state mutation and signal emission.

### Phase A — extract loaders, preserve controller flow

Status: completed on 2026-07-14. The controller remains the state/signal owner and now delegates to internal classifier, text/local loader, FB2 loader, audio-cover extractor, image inspector and provider materializer modules.

Create an internal `src/preview/` group:

- `PreviewData.h`: `PreviewData`, `LocalPreviewData`, `ImageMetadataData`, `Fb2PreviewData` and related value types.
- `PreviewClassifier.h/.cpp`: text/office/image/video/Google Apps and materialization-suffix decisions.
- `TextPreviewLoader.h/.cpp`: bounded local/admin/archive text reads and chunk calculations.
- `Fb2PreviewLoader.h/.cpp`: XML parsing, ZIP/archive entry handling and pagination.
- `AudioCoverExtractor.h/.cpp`: TagLib cover extraction and cleanup-file creation.
- `ImagePreviewInspector.h/.cpp`: image completeness/usability and metadata.
- `ProviderPreviewMaterializer.h/.cpp`: cleanup lease, remote size policy and local materialization.

Keep `QuickLookController` state and exact signal behavior intact in this phase.

### Phase B — decompose `previewPath()` without changing its external timing

Break it into concrete private methods:

- `beginPreviewRequest(path, forceReload)`;
- `previewVirtualRoot(...)`;
- `previewDirectory(...)`;
- `previewArchiveEntry(...)`;
- `previewLocalOrMaterializedFile(...)`;
- `startTextPreview(...)`;
- `applyBasePreviewData(...)` and format-specific apply methods.

Retain the existing generation checks and `QPointer` guards. Do not replace all signals with a blanket “emit everything” or a new single signal; current QML may rely on notification timing.

### Verification

- Empty selection, multi-selection, local folder, drive, favorites and provider roots.
- Local/provider/archive text, full text and chunks.
- Image metadata requested by docked preview vs Quick Look popup.
- SVG, raster, PDF, fonts, executable/shortcut, audio, video, office, archives and unsupported files.
- FB2 and FB2.ZIP cover, metadata, page size and page navigation.
- Rapid A→B→C preview changes, hide during load, refresh and deleted source.
- Remote preview cap and cleanup lease release on success, cancellation and stale generation.

## P1.3 — `ArchiveFileProvider.cpp`

Status: mechanical multi-TU split completed on 2026-07-14. Provider browsing/scanning remains in the facade, while extraction, catalog/open-read/state construction and cache/password ownership are separated without changing the public provider API.

### Diagnosis

The first 1,314 lines implement extraction infrastructure: filesystem-aware throttling, process priority, temporary leases, recursive cleanup, 7-Zip pipe extraction, nested-container resolution and archive format selection. The class then contains browsing, cache/password management, multiple extraction entry points and state building. It also calls `OperationQueue` static thread callbacks, creating a circular conceptual dependency.

### Target split

- `ArchiveFileProvider.cpp`: provider construction, scan/cancel, `FileProvider` browse/metadata methods.
- `ArchiveCatalog.cpp`: state building, item record conversion and visible entry calculation.
- `ArchiveStateCache.cpp`: LRU state cache, password cache and invalidation.
- `ArchiveExtractionService.cpp`: physical/entry/multi-entry extraction and move-to-destination logic.
- `NestedArchiveResolver.cpp`: token parsing, depth policy, temporary nested container materialization.
- `ArchiveProcessControl.cpp`: 7-Zip process execution, duty-cycle throttle and background priority.
- `ArchiveTemporaryStorage.cpp`: temporary directory leases and cleanup.

Introduce a small `ArchiveOperationCallbacks` value containing abort/progress functions so archive code no longer needs to include `OperationQueue.h`. In the first split, keep the existing static callback bridge; replace it only in a later isolated commit.

### Verification

- ZIP/7z/RAR/tar and compressed tar listing/extraction.
- Nested archives through the depth limit, password request/cache/clear and invalidation.
- Single entry, directory subtree, multi-selection and whole-archive extraction.
- Large archive cancellation, throttling and destination-near/source-near staging rules.
- Cache hit/miss/eviction and external archive modification.
- No leaked `.fm-*` directories or cleanup leases.

## P1.4 — `DirectoryModel.cpp`

Status: mechanical multi-TU split completed on 2026-07-14. The `QAbstractListModel` remains the sole state owner; loading, watching, mutations, selection and filtering/sorting member definitions are separated without moving model mutation to helper objects.

### Diagnosis

Individual methods are less monstrous than in `OperationQueue`, but the class owns too many state machines: provider replacement, incremental batches, fresh async loads, watch restart/debounce/event application, filters/sorting, mutations, selection and thumbnails. Because it is a `QAbstractListModel`, careless extraction can break row/index and selection invariants even if the app still launches.

### Safe structural split

- `DirectoryModel.cpp`: constructor, roles, `rowCount`, `data`, simple properties.
- `DirectoryModelLoading.cpp`: open/cancel, provider lifecycle, pending inserts and fresh-load commit.
- `DirectoryModelWatching.cpp`: watch start/restart/suppression and directory/parent event application.
- `DirectoryModelMutations.cpp`: upsert/insert/remove/rename and thumbnail invalidation.
- `DirectoryModelSelection.cpp`: all selection operations and selected-path queries.
- `DirectoryModelFiltering.cpp`: filter state, filtered index mapping, sort policy and comparison.
- `DirectoryModelAlgorithms.h/.cpp`: pure filter/sort helpers currently in the large anonymous namespace.

Keep the backing vectors, path index, selection anchor and all `begin.../end...` model calls owned by `DirectoryModel`. Do not move actual model mutation into an asynchronous helper object.

### Characterization invariants

- Every `beginInsertRows`, `beginRemoveRows`, `beginResetModel` pair is balanced.
- `m_entries`, filtered view, path index and selected count agree after each mutation.
- Selection survives sort/filter/rename according to current behavior.
- Stale scanner generations and stale watcher batches are ignored.
- Large-directory fast finish produces the same ordering and roles.
- Watch suppression around local operations does not swallow later external changes.

## P2 — QML decomposition wins

### `PropertiesDialog.qml`

This is the safest large-file refactor and a good template for later QML work. It has five clear pages and fourteen inline component types.

Extract first:

- reusable controls: `PropertyRow`, `AttributeToggleRow`, permission/special-mode toggles, identity field, capability row, action pill, progress ring, drive metric/info rows and tab button;
- complete page roots: `PropertiesGeneralPage.qml`, `PropertiesDetailsPage.qml`, `PropertiesAccessPage.qml`, `PropertiesOwnershipPage.qml`, `PropertiesHashesPage.qml`.

Keep `currentTab`, availability logic, export dialogs and controller mutation functions in the dialog root. Pass `propertiesController` and explicit action callbacks to pages. Do not duplicate Unix mode/ownership state across pages.

### `SettingsDialog.qml`

Extract visual sections:

- `SettingsWorkspaceSection.qml`;
- `SettingsProvidersSection.qml`;
- `SettingsTypographySection.qml`;
- `SettingsFilesSection.qml`;
- `SettingsPerformanceSection.qml`;
- `SettingsThemesSection.qml`;
- `SettingsStateSection.qml`.

Extract provider account panels/dialogs for Google Drive, MEGA, Instagram and Telegram. Keep a single owner for each login state in the dialog during the first pass; the extracted panels emit actions such as `loginRequested`, `signOutRequested` and `forgetDataRequested`.

### `StorageView.qml`

Extract roots matching current visual sections:

- `SystemSummaryCard.qml`;
- `StorageDriveGrid.qml` and `StorageDriveCard.qml`;
- `QuickAccessGrid.qml` and `QuickAccessCard.qml`;
- `StorageKeyboardNavigator.qml` for index snapshots and section traversal.

Keep model-derived snapshots and volume/ISO event handling in the root until cards are stable. Preserve keyboard order, staggered animations and `ensureVisible()` behavior.

## P2 — application coordinators

### `WorkspaceController.cpp`

Status: Phase A mechanical multi-TU split completed on 2026-07-14. The public facade remains unchanged, with member definitions separated into panel orchestration, drops, clipboard, delete policy, archive/ISO actions, volume lifecycle and history implementation files.

After the operation and panel refactors, split member definitions into:

- panel/split orchestration;
- drop capability and drop execution;
- clipboard/paste;
- delete request/confirmation;
- archive and ISO actions;
- volume lifecycle;
- undo/redo history.

Do not create separate QML controllers in the first pass. `WorkspaceController` is a valid facade; the problem is implementation concentration.

### `App.qml`

Status: coordinator extraction completed on 2026-07-14. Workspace persistence/restore and administrator-mode lifecycle now have dedicated coordinators, preview suppression/release is owned by the existing `PreviewCoordinator`, and the root keeps compatibility wrappers for commands, shortcuts and overlays.

`App.qml` is allowed to be larger than an ordinary component because it is the composition root, but 132 functions still indicate excess logic. Extract:

- `WorkspaceStateCoordinator.qml`: save scheduling, capture and restore;
- `AdminModeCoordinator.qml`: unlock/lock/safety/expiry interaction;
- extend the existing `PreviewCoordinator.qml` to fully own preview suppression/release;
- keep thin root action wrappers consumed by `CommandRegistry`, `AppShortcuts` and overlays.

Do not move every one-line command wrapper; those form a useful stable facade.

## Large files that are not first-priority monsters

- `qml/components/app/CommandRegistry.qml` (1,464 lines) is large but mostly cohesive declarative command registration. Split by command families only after its API stabilizes; do not replace it with opaque generated JavaScript.
- `src/core/LocalFileProvider.cpp` (1,625 lines) contains platform-specific enumeration and provider operations but has a clearer single responsibility than the P0/P1 files. A later platform-helper split is reasonable.
- `src/plugins/mega/MegaFileProviderPlugin.cpp` (2,540 lines), `src/plugins/portable_device/PortableDeviceFileProviderPlugin.cpp` (2,052 lines), `src/plugins/telegram/TelegramClient.cpp` (1,758 lines) and `src/plugins/instagram/InstagramFetcher.cpp` (1,581 lines) should follow the GDrive pattern later. They are important, but less globally coupled.
- `qml/components/ThemeEditorDialog.qml` and `qml/components/Sidebar.qml` are valid P3 targets. Both need care because theme draft state and sidebar focus/scroll state are behaviorally dense.

## Recommended implementation order

### Wave 0 — safety baseline

1. Clean Release build with warnings-as-errors.
2. `ctest --test-dir build --output-on-failure`.
   - At audited commit `6f6fd48`, `telegram_provider_skeleton_test` is a known baseline failure: it expects browse/read-metadata/transfer capabilities while the current implementation also exposes create. Fix the stale expectation in a separate commit or keep the failure explicitly recorded until then; do not attribute it to the refactoring.
3. Confirm the supported unity-build configuration. Non-unity builds are outside project scope and are not an acceptance gate.
4. Save baseline QML warnings with `QML_DISABLE_DISK_CACHE=1`.
5. Record representative timings: startup, 5–6k file folder, local copy, provider batch transfer, archive open/extract and Quick Look latency.
6. Add the characterization tests needed for the subsystem before its first structural move, not only before deeper responsibility extraction.

### Wave 1 — structural-only decomposition

1. `OperationQueue` multi-TU split.
   - Move one independently buildable responsibility group per commit: archive, admin, platform, provider transfers, generic copy/move, execution dispatch, then facade cleanup.
   - Start with the most isolated groups; do not move `execute()` and `copyPath()` first.
2. `PropertiesDialog` controls and pages as the first low-risk QML contract calibration.
3. GDrive thumbnail/export helper split.
4. `FilePanelController` multi-TU split.
5. `QuickLook` helper-loader split.
6. `ArchiveFileProvider` multi-TU split.
7. `DirectoryModel` multi-TU split.

Run the relevant Windows build/checkpoint immediately after batches touching `OperationQueuePlatform`, admin paths, native suggestion enumeration or directory watching. Do not postpone platform validation until the end of the campaign.

No new runtime architecture should appear in this wave.

### Wave 2 — low-risk QML extraction

1. `SettingsDialog` sections/provider panels.
2. `StorageView` cards/grids.
3. `FilePanel` menu and overlay hosts, followed by other leaf visuals whose dependency inventories show bounded explicit contracts.

### Wave 3 — stateful responsibility extraction

1. `OperationQueue` execution context/result accumulator/provider transfer engine.
2. `QuickLook` `previewPath()` decomposition.
3. Archive extraction callback decoupling.
4. `FilePanel` current-index, scroll, rename, hover and context-menu coordinators.
5. `DirectoryModel` pure algorithms and optional watch coordinator.

### Wave 4 — secondary monsters

Workspace/App, MEGA, portable device, Telegram, Instagram, Sidebar and Theme Editor.

## Status checkpoint — 2026-07-14

The implementation waves have not been completed strictly in numerical order. Wave 2 is complete and most Wave 3 responsibility seams are present, but several mechanical Wave 1 splits remain unfinished. Do not treat the earlier `refactoring of the base components of FM (part1)` commit as proof that every Wave 1–3 item is closed.

### Completed

- `OperationQueue` mechanical multi-TU split: facade, execution, copy/move, provider transfers, archives, administrator operations and platform code.
- `OperationQueue` execution context, result accumulator and provider transfer engine.
- `PropertiesDialog`, `SettingsDialog` and `StorageView` QML decomposition.
- GDrive decomposition into a thin plugin/action shell, concrete provider, API client, transfer client, request policy, entry mapper, export policy and thumbnail loader; committed as `dd7a2e4` and runtime-tested without observed regressions.
- `FilePanelController` Phase A mechanical multi-TU split into facade, navigation, suggestions, launch, mutations, properties and filters, with its public API unchanged.
- Quick Look loader extraction into the internal `src/preview/` modules; `QuickLookController.cpp` reduced from about 3,187 to about 1,670 lines without changing its public API or signal owner.
- `ArchiveFileProvider` mechanical multi-TU split into provider facade, extraction service, catalog/open-read/state construction and cache/password implementation.
- `DirectoryModel` mechanical multi-TU split into facade, loading, watching, mutations, selection and filtering/sorting implementation.
- Quick Look `previewPath()` decomposition into concrete controller methods while preserving the controller state owner.
- Archive operation callback decoupling.
- Directory model pure algorithms and watch policy extraction.
- FilePanel menu/overlay hosts and initial current-index, scroll, rename, hover and context-menu coordinator state/timers.

The supported unity build passes with `cmake --build build -j 12`, and all 24 registered tests pass at this checkpoint.

### Deferred behavioral migration

The remaining FilePanel state-machine ownership transfer is not a mechanical split: it changes QML ownership around focus, scrolling, rename, hover and context menus and therefore requires its own runtime QA cycle. It is explicitly deferred and does not block starting Wave 4 after the completed structural split set is committed. `FilePanel.qml` remains about 4,226 lines.

### Resume order

1. Commit and runtime-test the completed GDrive change. Done: `dd7a2e4`, no observed regressions.
2. `FilePanelController` multi-TU split. Done; supported unity build and all 24 tests pass.
3. Quick Look loader extraction. Done; supported unity build and all 24 tests pass.
4. `ArchiveFileProvider` multi-TU split. Done; supported unity build passes.
5. `DirectoryModel` multi-TU split. Done; supported unity build passes.
6. Re-run the supported unity build and full test suite, runtime-test the accumulated structural split, commit it, then begin Wave 4 with `WorkspaceController`/`App.qml`.

Provider-only Wave 4 research such as MEGA may use the completed GDrive pattern earlier, but the general Workspace/App transition should wait for the panel/controller seams above. Non-unity builds remain outside project scope and are not an acceptance gate.

## Suggested commit sequence

1. `test: characterize OperationQueue behavior`
2. `refactor(operations): move archive implementation`
3. `refactor(operations): move administrator implementation`
4. `refactor(operations): move platform implementation`
5. `refactor(operations): move provider transfer implementation`
6. `refactor(operations): move copy and move implementation`
7. `refactor(operations): move request execution implementation`
8. `refactor(qml): extract properties dialog pages`
9. `refactor(gdrive): isolate thumbnails and export policy`
10. `refactor(gdrive): separate provider and API transfer implementation`
11. `refactor: split FilePanelController implementation`
12. `refactor(preview): extract Quick Look loaders`
13. `refactor(archive): separate extraction and cache implementation`
14. `refactor: split DirectoryModel implementation`
15. `refactor(qml): extract settings sections`
16. `refactor(qml): extract storage view cards`
17. `refactor(qml): extract FilePanel leaf components`
18. `refactor: isolate provider transfer execution`
19. `refactor(preview): decompose preview request handling`
20. `refactor(qml): move FilePanel state machines into coordinators`

Each commit should contain a short “behavior preserved” checklist naming the scenarios actually exercised.

## Global acceptance gate

The refactoring campaign is successful only if all of the following remain true:

- No user-visible feature, label, shortcut, menu, view, operation, provider or preview behavior changes.
- No new C++ warning, QML import warning, undefined-property warning, binding loop or runtime assertion.
- All existing tests pass; new characterization tests pass on the pre-refactor and post-refactor code.
- Linux and Windows builds remain green, including optional plugins available in each environment.
- File operations retain exact conflict, cancellation, progress and partial-failure semantics.
- Directory loading/watching/selection stays correct under rapid navigation and external changes.
- Provider transfer throughput, large-folder responsiveness, scrolling and startup do not regress beyond measurement noise.
- Temporary files and cleanup leases remain balanced.
- Public headers/QML API and settings serialization are unchanged until an explicitly separate migration is approved.

## Final recommendation

Start with `OperationQueue` as a **mechanical multi-file split**, not a redesign. In parallel planning terms, the safest quick QML win is `PropertiesDialog`, but it should be a separate commit series. Do not begin the deep `FilePanel.qml` state migration until the panel QA matrix is captured: it is the file most likely to produce subtle focus, scroll, selection and high-refresh-rate regressions.

The desired result is not merely smaller files. It is a set of clear facades whose implementations can be reviewed and changed independently, while FMQml behaves exactly as it does at `6f6fd48`.
