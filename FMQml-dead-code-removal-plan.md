# FMQml Dead-Code Removal Plan

## Audit baseline

- Repository: `TankredBarb/FMQml`
- Branch: `main`
- Audited commit: `cea3e4d9f5ddbdb4c92e4773c9cfc026d67b5e88`
- Commit subject: `implemented tool to compare folders, fixed local thumbs to update on change in external app`
- Audited source set: 135 `.cpp`, 111 `.h`, and 149 `.qml` files.
- Scope: C++ and QML code only. Developer tools and benchmarks are not considered dead merely because the main executable does not reference them.
- Goal: remove code with no reachable in-tree consumer while preserving runtime behavior exactly.

## Safety rule

A candidate is eligible for removal only when all of the following are true:

1. It has no direct C++ or QML caller.
2. It has no string-based caller (`QMetaObject::invokeMethod`, `setProperty`, `Loader`, component URL, signal/slot macro, or JavaScript lookup).
3. It is not a Qt callback, model role, default property, QML singleton, plugin entry point, conditional platform implementation, or test/tool entry point.
4. Removing it does not alter a live state transition, signal notification, persistent setting, plugin ABI, or serialized format.
5. The application builds and the relevant smoke tests pass after the smallest possible deletion batch.

Do not combine dead-code removal with naming cleanup, refactoring, formatting, or behavior changes. Small isolated commits are essential for bisectability.

## Expected result

The first structural batch removes 848 lines in six wholly unreachable files. The confirmed unused-method set contains another 45 definitions totaling about 463 lines, plus declarations and now-unused signals/helpers. QML member cleanup and obsolete meta-object exposure should remove additional code. A realistic conservative result is roughly 1.3–1.5 kLOC without changing behavior.

## Phase 0 — establish a baseline

Before deleting anything:

1. Start from a clean tree at the audited commit or re-run the reference scan if `main` moved.
2. Configure the normal Release build with `FM_WARNINGS_AS_ERRORS=ON` and `QT_ENABLE_QML_DEBUG=OFF`.
3. Build `fm`, all enabled provider plugins, the Linux admin helper where applicable, and all test targets.
4. Run `ctest --test-dir build --output-on-failure`.
5. Launch once with the QML disk cache disabled so every QML file is parsed again:

   ```bash
   QML_DISABLE_DISK_CACHE=1 ./build/fm
   ```

6. Save the current test result and any pre-existing QML warnings. New warnings after a deletion are a failure.

## Phase 1 — delete wholly unreachable files

This is the safest and highest-value batch. Make it one commit.

### 1.1 Obsolete Windows provider prototype

Delete:

- `src/core/WinLocalFileProvider.cpp` — 256 lines
- `src/core/WinLocalFileProvider.h` — 29 lines

Evidence:

- Neither file is present in the root `CMakeLists.txt` or any nested build target.
- `WinLocalFileProvider` has no external reference.
- Both files explicitly say that they are not registered and are deletion candidates.
- The implementation is obsolete: current Windows native enumeration already lives in `LocalFileProvider.cpp`.
- The prototype cannot be reintroduced as written because it derives from the current `final` `LocalFileProvider`.

### 1.2 Orphaned QML components

Delete:

- `qml/components/filepanel/FilePanelSelectionBadge.qml` — 50 lines
- `qml/components/filepanel/FilePanelStatusBar.qml` — 112 lines
- `qml/components/filepanel/FilePanelStatusRail.qml` — 112 lines
- `qml/components/preview/MetadataPreview.qml` — 289 lines

Also remove the two registered files from `MY_QML_FILES`:

- `FilePanelSelectionBadge.qml`
- `FilePanelStatusBar.qml`

Evidence:

- None of the four component type names is instantiated or referenced by URL anywhere in C++, QML, tests, or plugin code.
- `FilePanelStatusRail.qml` and `MetadataPreview.qml` are not even included in the QML module.
- `FilePanelStatusRail.qml` is byte-for-byte equivalent in purpose and content to the unused `FilePanelStatusBar.qml` copy.
- The live replacements are `FilePanelFooter.qml`, `FilePanelStatusMessagePolicy.qml`, `FilePanelSelectionActions.qml`, and the current preview component hierarchy under `qml/components/preview/`.

### Phase 1 verification

- Build the QML module from scratch, not incrementally.
- Open both panels in details, grid, brief, and resize-adaptive modes.
- Exercise zero selection, one selection, and multi-selection.
- Trigger loading, archive loading, an error banner, and a transient status message.
- Open Quick Look and the docked preview for a folder, image, text file, archive, audio file, and unsupported file.
- Confirm no `Type ... unavailable`, import, or binding warnings appear.

## Phase 2 — remove unused QML members

Make this a separate commit. Remove only the named members; do not reorganize the surrounding QML.

### 2.1 Uncalled functions

- `qml/App.qml`
  - Remove `ensureAdminModeForAction()`.
  - Remove `pendingAdminAction` and the now-unreachable pending-action branches in `confirmAdminSafetyAndUnlock()` and the safety dialog rejection path. Keep the actual unlock/safety-warning flow unchanged.
- `qml/components/ThemeEditorDialog.qml`
  - Remove `areaHighlighted()`.
  - Remove `areaChanged()`.
  - Remove `areaMarkerLabel()`.
- `qml/components/FileWorkspace.qml`
  - Remove `collapseToActivePanel()`; `expandSinglePanel()` is the live implementation.
- `qml/components/PropertiesDialog.qml`
  - Remove `openExportMenuAtCursor()`.
  - Remove `silentExportJson()`; keep the used generic `silentExport(type)`.
  - Remove `activePanelCurrentPath()`; keep `activePanelController()` because other live code uses it.
- `qml/components/FavoritesView.qml`
  - Remove `selectedList()`.
- `qml/components/app/AppShortcuts.qml`
  - Remove the unused JavaScript `isReadOnlyContainerPath()` helper. Do not touch the live C++ method of the same name.
- `qml/components/filepanel/FilePanelContextMenu.qml`
  - Remove `canOpenContextWithWine()`.
  - Remove `canOpenContextWithSteamProton()`.

`startupShellReady()` in `App.qml` must not be removed: it is invoked by name from `SplashController.cpp`.

### 2.2 Unread properties

Remove these properties, each of which occurs only at its declaration and has no dynamic string consumer:

- `qml/App.qml`: `transientInfoBottomInset`, `typeToSearchEnabled`
- `qml/components/StorageView.qml`: `sourcePathRole`
- `qml/components/FileWorkspace.qml`: `operationsDrawerWidth`
- `qml/components/FilePanel.qml`: `dropTargetActive`, `dropTargetAllowed`, `dropTargetDeniedReason`, `briefColumnWidth`, `totalOtherColumnsWidth`
- `qml/components/ThemeEditorPreviewCard.qml`: `pSurfHov`
- `qml/components/preview/ZoomableImagePreview.qml`: `visibleContentWidth`
- `qml/components/preview/BookPreview.qml`: `softPaperColor`
- `qml/components/preview/PreviewRenderer.qml`: `mediaType`
- `qml/components/filepanel/FileHoverPreviewCard.qml`: `mediaFactsText`, `cardAccentSoft`
- `qml/components/filepanel/FilePanelDropOverlay.qml`: `deniedReason`

Remove the following unused `Theme.qml` tokens:

- `glassSurface`, `glassBorder`
- `motionSlow`
- `fontSizeH1`, `fontSizeH2`
- `fontLight`, `fontNormal`, `fontMedium`, `fontSemiBold`, `fontBold`
- `surfaceOpacity`
- `spacingXs`, `spacingSm`, `spacingMd`, `spacingLg`, `spacingXl`

### 2.3 Explicit QML false positives — keep

- `App.qml` and `Splash.qml` are roots loaded by module/type name.
- `Theme.qml` and `TextColors.qml` are registered singletons.
- `FileSearchDialog.qml::displayParentPath` is a required model role populated by QML model injection.
- `FilePanelShell.qml::contentData` is a default property alias used implicitly by child declarations.
- Every `function onXChanged/onSignal` inside `Connections` is a live Qt handler even if the handler name appears only once.

### Phase 2 verification

- Repeat application startup with QML cache disabled.
- Exercise admin unlock, safety confirmation, lock, expiry, and relaunch-as-admin entry points.
- Open the theme editor, hover tokens, change tokens in every category, save, reload, and cancel.
- Export properties as JSON and text using the visible menu flow.
- Exercise Wine and Steam Proton through the current Open With dialog/context-menu paths.
- Test drag/drop allowed and denied overlays and hover preview cards.

## Phase 3 — remove confirmed unused C++ functions

Every function below has no in-tree caller and no string/meta-object reference. Remove declaration and definition together. Keep each subsystem group as its own commit or, at minimum, its own reviewed diff section.

### 3.1 Controllers

- `QuickLookController`: `syncImageInfo()` — obsolete synchronous image metadata path; the live path uses async metadata and `syncImageProperties()`.
- `FilePanelController`:
  - `childPathForCurrent()`
  - `isIsoImageFilePath()`
  - `archiveExtractionFolderNameForPath()`
  - `getDirectorySuggestions()`
  - `requestDirectorySuggestions()`
  - `openRow()`
  - `openPathWithApplication()`
  - `openPathWithWine()`
  - `ejectDrive()`
- After removing the old string-only suggestion API, also remove:
  - `directorySuggestionsForInput()`
  - `suggestionPaths()`
  - `directorySuggestionsReady`
  - Keep `requestDirectorySuggestionEntries()`, `directorySuggestionEntriesForInput()`, and `directorySuggestionEntriesReady`; these are the live autocomplete path.
- `WorkspaceController`: `deleteActiveSelection()`
- `ThumbnailController`: `cancelThumbnail()`, `warmThumbnails()`, `thumbnailMetrics()`
- `ThemeController`: `saveThemeToFile()`, `currentThemeState()`, `applyThemeState()`; keep the live draft/state read/write/import/export APIs used by the theme editor.

### 3.2 Core

- `OperationQueue`: `compressToSevenZip()`, `totalEntryCountForPath()`
- `ArchiveFileProvider`: `toArchiveToken()`, `joinRelativePath()`, `currentBrowsePathFromPath()`, `visibleEntriesForState()`
- `LinuxAdminBroker`: `backendMode()`; keep `setBackendModeForTesting()` and the internal backend state.
- `IsoSupport`: `displayNameForImagePath()`
- `FileProviderPluginRegistry`: `loadedPluginIds()`
- `VolumeMonitor`: `hasVolumeRoot()`, `isKnownEjectableRoot()`, `isKnownReadyRoot()`, `rootForPath()`, `recentlyRemovedRootForPath()`
- `IsoMountManager`: `availableDriveLetters()`
- `FileAccessResolver`: `invalidateAll()`
- `CleanupSubsystem`: `activeLeases()`, `registrySnapshot()`

### 3.3 Models and provider caches

- `FileSearchModel`: `matchKindAt()`
- `TreeModel`: `isTopLevelIndex()`
- `DirectoryModel`: `clearFilters()`
- `MegaCache`: `hasKey()`, `isLinkLoading()`, `isLinkLoaded()`, `linkError()`, `getChildrenIfCached()`
- `TelegramCache`: `storeSavedPagination()`, `savedPagination()`

### 3.4 Obsolete enum/declaration-only code

- Remove the unused `BatchRenameEngine::RuleType` enum. Runtime rules are string-valued maps; no enum value is read.
- Remove `OpenWithCandidateKind::SystemChooser`; no candidate creation or switch uses it.
- Remove the unused `FilePanelController::revealBatchRename` signal.
- Remove the entire unused `FM_DEBUG_LOAD_TIMING` declaration block from `DirectoryModel.h`: `dumpLoadTiming()`, `m_loadTimingTimer`, `m_loadTimingFirstRowInserted`, and `m_loadTimingRailShown`.
- Remove the unused Windows-only free helper `windowsDriveType()` from `VolumeMonitor.cpp`.

### 3.5 Emitted signals with no consumer

Remove the signal declarations and their emissions:

- `ThumbnailController::thumbnailUnavailable`
- `VolumeMonitor::volumeAdded`
- `PlacesModel::lowDiskSpaceWarning`

These are not `Q_PROPERTY` notification signals and have no C++ connection, QML handler, or string-based consumer.

### Phase 3 verification

- Run the complete C++ test suite after every subsystem group.
- Path autocomplete: local paths, archive paths, provider paths, cancellation, and stale request IDs.
- Open With: default app, chosen app, multi-selection, Wine, Proton, remembered preference, and clearing preference.
- Preview: image metadata, large image, provider image, archive image, cache hit/miss, and preview cancellation.
- Operations: copy/move/delete, archive compression/extraction, provider transfer, cancellation, and conflict resolution.
- Plugins: load/unload/reload and error display; test Mock plus every enabled real provider.
- Devices: add/remove/eject/unmount, ISO mount/unmount, and managed-mount navigation.
- Cleanup: startup cleanup, lease acquisition/release, and stale artifact cleanup.

## Phase 4 — prune unused QObject/QML exposure

This phase removes only unused meta-object surface. Preserve internal fields where they still feed grouped models, caches, or behavior.

### 4.1 Remove property plus getter/state when the entire state is dead

- `SystemInfoProvider`: remove `gpuName`, `motherboard`, and `displayInfo` properties, getters, backing fields, and `displayInfoChanged`. None is populated or consumed.
- `QuickLookController::canonicalPath`: remove the property, getter, signal, backing member, worker-result field, assignments, clears, and emissions. The value is written only for this unused property and is never read internally.

### 4.2 Remove unused property exposure/accessors but keep live internal state

- `AdminController`: remove exposed `adminModeAvailable`, `adminModeState`, `adminModeBackendName`, and `adminModeTimeoutMinutes` accessors/properties. Remove the unused timeout setter and `adminModeTimeoutMinutesChanged`. Keep internal state, active/name/reason/remaining-time properties, and the live availability/state signals.
- `FilePanelController`: remove `detailsSortRole` and `detailsSortOrder` properties, getters, setters, dedicated signals, and their emissions. Current sorting is driven by the live panel/model sort API.
- `ProviderPropertiesController`: remove direct `sizeExact`, `itemCountText`, and `quotaProperties` properties/getters. Keep their fields because grouped/exported property data still uses them.
- `WorkspaceController`: remove the `clipboardCount` property/getter. Keep `clipboardChanged`, which notifies the live clipboard properties.
- `DiskUsageController` and `FileSearchController`: remove direct `skippedPaths` and `reparsePaths` properties/getters. Keep counters, detail lists, progress, and cache serialization.
- `FolderCompareController`: remove `leftRoot` and `rightRoot` properties/getters plus `rootsChanged` and its emissions. Keep the roots as internal execution state.
- `PropertiesController`: remove direct `accessed`, `accessProperties`, `attributeProperties`, and `unixProperties` properties/getters. Keep the backing data because it feeds the live grouped properties model.
- `HistoryManager`: remove only the unused `canUndo` and `canRedo` `Q_PROPERTY` declarations. Keep the C++ methods and shared change signals; internal undo/redo logic and visible stack counts use them.
- `DirectoryModel`: remove the unused `hasActiveFilters` `Q_PROPERTY` declaration but keep the method used internally. Remove `activeFiltersSummary` property, declaration, and definition.

### Phase 4 verification

- Run with `QML_DISABLE_DISK_CACHE=1` to catch every removed property reference.
- Verify admin status/commands, sorting in all views, provider properties, clipboard cut/copy/paste summaries, disk usage, file search, folder compare, properties tabs/export, undo/redo counts, and filters.
- Confirm settings import/export JSON is unchanged; none of the removed properties may be serialized.

## Phase 5 — compiler/resource cleanup

Only after Phases 1–4 are green:

1. Remove includes made unused by the deleted definitions.
2. Remove obsolete forward declarations.
3. Regenerate the QML type/resource metadata through a clean CMake configure/build.
4. Search again for every removed symbol and filename; expected result is zero hits outside this plan and Git history.
5. Re-run the unused-symbol scan because deleting wrapper functions can expose second-order dead helpers.
6. Do not remove a second-order candidate until it passes the same five-part safety rule.

## Final cross-platform gate

The cleanup is complete only after both supported platforms pass.

### Linux

- Clean Release build with GCC or Clang and warnings-as-errors.
- All tests, admin helper tests, provider tests available on the machine.
- X11/Wayland startup smoke if both are routinely supported.
- Local enumeration, inotify refresh, admin browsing, Open With/Wine/Proton, taskbar/tray, device and ISO flows.

### Windows

- Clean MSVC Release build with `/W4 /WX`.
- All tests and enabled plugins.
- Large-directory enumeration to confirm the active native implementation remains in `LocalFileProvider`.
- Shell icons/thumbnails, Open With, taskbar progress, drive add/remove/eject, ISO, and portable-device flows.

### Acceptance criteria

- No build, linker, QML import, binding, or runtime warning introduced.
- All existing tests pass on both platforms.
- No visible UI, shortcut, operation, provider, preview, admin, theme, or persistence behavior changes.
- No plugin ABI/interface class or dynamic plugin entry point is removed.
- Each deletion batch is independently revertible and bisectable.

## Confirmed false positives — do not delete

- Concrete plugin classes with no textual constructor call: they are created by `QPluginLoader` through `Q_PLUGIN_METADATA`.
- `App.qml`, `Splash.qml`, QML singletons, and conditionally included PDF/Multimedia preview components.
- `[[maybe_unused]]` platform helpers that are used on another OS branch.
- Virtual overrides and native callbacks such as `nativeEventFilter` and MEGA SDK listeners.
- C++ signals consumed by QML `on...` handlers.
- Properties that are model roles or default aliases even when no explicit property read exists.
- `tools/winsxs-scan-bench/main.cpp`: it belongs to its own `tools/winsxs-scan-bench/CMakeLists.txt` and is a developer benchmark, not orphaned application code.

## Recommended commit sequence

1. `cleanup: remove orphaned source and QML files`
2. `cleanup: remove unused QML members and theme tokens`
3. `cleanup: remove obsolete controller APIs`
4. `cleanup: remove obsolete core helpers and cache APIs`
5. `cleanup: remove unconsumed signals and enum remnants`
6. `cleanup: trim unused QObject property exposure`
7. `cleanup: remove newly unused includes and declarations`

Do not squash these until the full Linux and Windows gate passes.
