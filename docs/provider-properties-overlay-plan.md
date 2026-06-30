# Provider Properties Overlay Plan

Goal: add a provider-only properties overlay for Google Drive, MEGA, and future
non-local providers. This overlay does not replace the local `PropertiesDialog`.
It exposes the best available provider metadata and, most importantly, an exact
recursive folder size when the provider can enumerate the folder tree.

## Problem

The current Properties action is effectively local-only:

- `App.showActiveProperties()` rejects non-local paths with
  `Properties are available for local files only`.
- Local properties depend on local filesystem APIs, drive stats, Unix
  permissions, checksums, and attribute editing. Those concepts do not map
  cleanly to cloud provider objects.
- Provider entries already carry some metadata through `FileEntry`, but there is
  no focused user surface for a selected cloud item.
- For cloud storage, folder size is a core decision-making value. Today a user
  can browse a Google Drive or MEGA folder without a clear way to know how much
  storage that folder consumes.

The overlay should be honest about provider limitations. It should show exact
values where exact values are available, show calculated values while scanning,
and label unavailable fields as unavailable instead of pretending to be a full
local properties dialog.

## Non-Goals

- Do not retrofit the existing local `PropertiesDialog` into a universal
  provider dialog.
- Do not add chmod, hidden/read-only toggles, checksums, local path reveal, or
  terminal actions for provider items.
- Do not download file contents to compute metadata.
- Do not block the UI thread while calculating folder size.
- Do not use provider-to-local materialization just to calculate folder size.
- Do not make remote streaming, thumbnails, or previews part of this feature.

## Current Data Sources

Shared provider contract:

- `FileProvider::entryInfo(path)` returns cached `FileEntry` metadata.
- `FileProvider::childPaths(path, includeHidden)` returns known children.
- `FileProvider::storageInfo(path)` returns provider/account storage quota
  where implemented.
- `FileEntry` includes:
  - `name`, `path`, `suffix`, `mimeType`;
  - `size`, `sizeText`;
  - `modified`, `modifiedText`, `created`, `createdText`;
  - `isDirectory`, `isReadOnly`, `isShortcut`;
  - provider-specific presentation text such as `providerCapabilitiesText`.

Google Drive:

- File size comes from Drive file `size` and is populated for files.
- Google Drive folders do not expose aggregate folder size in normal listing
  metadata.
- Folder size must be calculated by recursively enumerating descendant files and
  summing their file sizes.
- Shortcuts need special handling:
  - file shortcut size can mirror target metadata when target is resolvable;
  - folder shortcuts should be labelled as shortcuts and should not silently
    double-count target content unless the UI explicitly says it is following
    the shortcut target.

MEGA:

- Files expose size through SDK node size.
- Account tree traversal already caches children and file sizes.
- Account-folder entries currently store `0` for directory size in some paths,
  so folder size should be calculated recursively from cached/enumerated
  descendants instead of trusting the folder entry size.
- Public link roots may expose SDK folder node sizes differently; the calculator
  should still prefer recursive child aggregation for consistency.

## UX Shape

Add a new overlay:

```text
qml/components/ProviderPropertiesOverlay.qml
src/controllers/ProviderPropertiesController.{h,cpp}
```

Open behavior:

- The existing Properties command remains the command entry point.
- If the selection is local, keep opening `PropertiesDialog`.
- If the selection is a provider path (`gdrive://`, `mega://`, later
  `portable://` etc.), open `ProviderPropertiesOverlay`.
- If the selection is mixed local/provider, show a clear unsupported message for
  this first phase.
- Multi-selection provider properties can be a later phase; start with one
  selected provider item.

Overlay layout:

- Title: item name and provider badge.
- Primary facts:
  - Type: file, folder, shortcut, provider root, shared folder, trash item where
    available.
  - Size:
    - files: direct provider file size;
    - folders: recursive calculated total;
    - unknown: `Unavailable`.
  - Items for folders: files and folders counted during recursive scan.
  - Modified / Created where available.
  - Provider path, with copy button.
- Provider section:
  - Provider: Google Drive / MEGA.
  - Account/root quota if `storageInfo()` is available: used, free, total.
  - Capabilities from `providerCapabilitiesText` or normalized capability rows.
- Status line:
  - `Calculating folder size...`
  - `Exact size calculated`
  - `Partial result; refresh required`
  - `Unavailable: <provider error>`
- Actions:
  - Copy summary.
  - Copy provider path.
  - Refresh metadata / recalculate size.
  - Close.

The overlay should visually be a lightweight information panel, not a full
local properties window. It should not include local-only tabs.

## Controller Contract

Create `ProviderPropertiesController` as a QML-facing controller.

Suggested properties:

```cpp
Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
Q_PROPERTY(bool calculatingSize READ calculatingSize NOTIFY stateChanged)
Q_PROPERTY(QString providerName READ providerName NOTIFY propertiesChanged)
Q_PROPERTY(QString path READ path NOTIFY propertiesChanged)
Q_PROPERTY(QString name READ name NOTIFY propertiesChanged)
Q_PROPERTY(QString typeText READ typeText NOTIFY propertiesChanged)
Q_PROPERTY(QString sizeText READ sizeText NOTIFY propertiesChanged)
Q_PROPERTY(QString itemCountText READ itemCountText NOTIFY propertiesChanged)
Q_PROPERTY(QString modifiedText READ modifiedText NOTIFY propertiesChanged)
Q_PROPERTY(QString createdText READ createdText NOTIFY propertiesChanged)
Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
Q_PROPERTY(QVariantList propertyGroups READ propertyGroups NOTIFY propertiesChanged)
Q_PROPERTY(QVariantList quotaProperties READ quotaProperties NOTIFY propertiesChanged)
Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
```

Suggested invokables:

```cpp
Q_INVOKABLE void load(const QString &path);
Q_INVOKABLE void refresh();
Q_INVOKABLE void cancel();
Q_INVOKABLE QString exportableText() const;
Q_INVOKABLE QString exportableJson() const;
```

The controller should:

- resolve the provider through `FileProviderFactory`;
- normalize the path through the provider;
- read cheap metadata from `entryInfo()` immediately;
- start recursive size calculation only for directories;
- use generation guards so stale async results cannot update the overlay;
- cancel work when the overlay closes or a new path is loaded;
- never touch local filesystem-specific `PropertiesController` state.

## Recursive Folder Size Calculation

Add a reusable provider folder size worker:

```text
src/core/ProviderFolderSizeCalculator.{h,cpp}
```

Input:

- provider scheme/path;
- generation id;
- optional limits:
  - maximum items before yielding progress;
  - maximum wall-clock interval between progress emissions;
  - cancellation flag.

Output:

- total bytes;
- file count;
- folder count;
- unavailable/error count;
- exact/partial status;
- last error string.

Algorithm:

1. Resolve provider for the root path.
2. Use `entryInfo(root)` to establish whether the item is a file or directory.
3. For a file, return its direct `FileEntry::size`.
4. For a directory:
   - traverse breadth-first or stack-based depth-first;
   - call `childPaths(current, true)`;
   - for each child path, call `entryInfo(child)`;
   - sum file sizes;
   - enqueue directories;
   - count folders and files;
   - periodically emit progress.
5. If a directory's children are not cached/available:
   - phase 1: mark result partial and request the user to refresh/open the
     folder first;
   - later: provider-specific async deep listing can fill gaps.

Important: `FileProvider` methods are synchronous and provider-backed. The
worker must run off the GUI thread. It must not call QML or mutate provider UI
state directly.

### Exactness Policy

The overlay must clearly label size results:

- `Exact`: every descendant was enumerated and every file size was known.
- `Partial`: some directories or files could not be read from provider cache or
  provider API.
- `Unavailable`: root item metadata cannot be read.

For Google Drive, exact folder size requires complete recursive enumeration.
For MEGA account roots, exact folder size is usually possible from the cached
account tree after the provider has loaded the account. If the cache is dirty,
the overlay should say so and offer Refresh.

## Provider API Extensions

Phase 1 can be built with existing provider methods, but the plan should allow
provider-specific faster paths.

Add optional virtuals later if needed:

```cpp
struct ProviderItemMetadata {
    FileEntry entry;
    QVariantMap providerFields;
};

struct ProviderFolderSizeResult {
    qint64 bytes = 0;
    int files = 0;
    int folders = 0;
    bool exact = false;
    QString error;
};

virtual bool supportsRecursiveSize(const QString &path) const;
virtual ProviderFolderSizeResult recursiveSize(const QString &path,
    const std::function<bool(qint64 bytes, int files, int folders)> &progress) const;
```

Do not add these until the generic calculator proves insufficient. Keeping
phase 1 generic reduces provider API churn.

## Integration Points

App/QML:

- Update `App.showActiveProperties(tabIndex)`:
  - local selection -> existing `PropertiesDialog`;
  - provider single-selection -> `ProviderPropertiesOverlay`;
  - provider multi-selection -> show unsupported/phase-later message.
- Add `ensureProviderPropertiesOverlay()` to `WorkspaceOverlays.qml`.
- Include the overlay in `workspaceOverlayOpen`.
- Add copy/export behavior only for the provider overlay, not through the local
  properties controller.

C++:

- Add `ProviderPropertiesController` to app services and QML context, matching
  how `PropertiesController` is exposed.
- Add `ProviderFolderSizeCalculator` to core.
- Add provider-safe formatting helpers or reuse `DriveUtils::formatSize`.

Provider behavior:

- Google Drive:
  - use `entryInfo()` for cheap file metadata;
  - use recursive scan for folders;
  - show shortcut fields and resolved target info when available;
  - show quota through `storageInfo()`.
- MEGA:
  - use cache for cheap metadata;
  - recursive scan should be fast when the account tree is already cached;
  - if remote cache is dirty, show status and allow Refresh.

## Performance And Safety

- Run recursive size calculation on a worker thread.
- Emit progress at a throttled interval, e.g. 100-250 ms.
- Use generation guards for every async completion.
- Use cancellation when:
  - overlay closes;
  - user selects another item;
  - provider refresh invalidates the current path.
- Avoid recursive QML calls.
- Avoid provider file content reads.
- Avoid staging/temp files.
- Do not scan all cloud roots automatically. Only scan the selected item after
  explicit Properties action.

## Failure Modes

- Provider is signed out:
  - show `Provider unavailable` and auth hint if provider exposes one.
- Entry missing:
  - show `Item not found; refresh the folder`.
- Permission denied / shared item cannot list children:
  - show direct metadata and mark folder size unavailable.
- Stale cache:
  - show cached direct fields;
  - mark folder size partial/unavailable;
  - offer Refresh.
- Long-running huge folder:
  - keep progress visible;
  - allow cancel;
  - keep last partial byte/file/folder counts visible but labelled partial.

## Tests

Unit tests:

- `ProviderFolderSizeCalculator` with an in-memory mock provider:
  - file root returns direct size;
  - nested folder sums descendants;
  - empty folder returns zero exact size;
  - missing child metadata marks partial;
  - cancellation stops traversal and returns partial/cancelled status;
  - generation guard prevents stale controller update.

Controller tests:

- provider path loads overlay state from mock provider.
- local path is rejected by `ProviderPropertiesController`.
- refresh restarts calculation and increments generation.

Manual QA:

1. Google Drive file:
   - Properties opens provider overlay.
   - Size matches Drive file size.
   - Path and provider details can be copied.
2. Google Drive folder:
   - overlay shows calculating status;
   - recursive size becomes exact after traversal;
   - file/folder counts are displayed.
3. MEGA folder:
   - folder size is calculated from cached descendants;
   - large folder remains responsive and cancellable.
4. Shared/shortcut Google Drive item:
   - shortcut status is visible;
   - target metadata is shown only when resolved.
5. Provider unavailable/signed out:
   - overlay shows an actionable unavailable state, not a blank dialog.
6. Local file:
   - existing `PropertiesDialog` still opens unchanged.

## Implementation Phases

### Phase 1: Overlay Shell And Routing

- Add `ProviderPropertiesOverlay.qml`.
- Add `ProviderPropertiesController` with cheap `entryInfo()` metadata only.
- Route single provider selection from Properties action to the new overlay.
- Keep local properties unchanged.

### Phase 2: Generic Recursive Size Worker

- Add `ProviderFolderSizeCalculator`.
- Calculate exact folder size from `childPaths()` and `entryInfo()`.
- Add progress, cancellation, generation safety, and partial status.
- Show bytes, files, folders, exact/partial state in the overlay.

### Phase 3: Provider-Specific Polish

- Google Drive: shortcut/target fields, quota rows, shared/trash labels.
- MEGA: dirty-cache status, account root labels, quota rows if available.
- Normalize provider display names and capability text.

### Phase 4: Tests

- Add mock-provider tests for recursive size and controller state.
- Add regression tests for local Properties routing remaining unchanged.

### Phase 5: UX Polish

- Add copy/export summary.
- Add refresh/recalculate action.
- Add progress/cancel affordance for huge folders.
- Tune empty/error states.

## Acceptance Criteria

- Local Properties behavior is unchanged.
- Single Google Drive and MEGA selections open the provider overlay.
- Provider files show direct size and basic metadata without downloading.
- Provider folders show recursive byte size, file count, and folder count when
  provider enumeration is complete.
- Folder size calculation never blocks UI navigation or selection painting.
- Large folder calculations show progress and can be cancelled.
- Stale/partial/unavailable results are explicitly labelled.
- Provider auth/cache failures show a clear message.
- No provider file content is downloaded just for properties.
- Tests cover generic recursive size calculation with a mock provider.

## Open Questions

- Should provider multi-selection be phase 2 or a later feature?
- Should folder shortcuts count target content by default, or only show target
  metadata without recursive size?
- Should calculated provider folder sizes be cached per provider path, and if
  yes, what invalidation signal should clear them?
- Should quota rows live in this overlay or in provider root/Places UI as well?
