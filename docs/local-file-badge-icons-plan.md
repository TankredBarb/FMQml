# Local File Badge Icons Plan

This document is the implementation plan for extending the existing icon-overlay
mechanism from provider-specific folders and avatars to useful local filesystem
state. It deliberately keeps the first pass small: badges must communicate an
actionable fact which is otherwise easy to miss, not decorate every file type.

## Decision Summary

- Reuse the overlay composition already owned by
  `qml/components/filepanel/FileIconCell.qml`; do not create three independent
  badge implementations for grid, brief, and table delegates.
- Add local badges only for:
  - symbolic links, including a distinct broken-link state;
  - local items inaccessible to the current user;
  - local filesystem mount points;
  - items pinned in Favorites;
  - archive files supported by the existing archive navigation flow.
- Keep filesystem classification in C++ and expose compact, stable model roles.
  QML may map a role to a visual asset and tooltip, but must not probe paths,
  test permissions, enumerate mounts, or query Favorites per delegate.
- Show one **primary state badge** in the existing bottom-right overlay position.
  A separate pinned star may occupy the top-right corner. This makes a pinned
  locked symlink legible without turning each icon into a stack of symbols.
- For the primary badge use this fixed priority:

  1. broken symbolic link;
  2. symbolic link;
  3. mount point;
  4. locked;
  5. archive.

  The full state remains available in the attributes column, tooltip, and
  Properties dialog. No item receives more than one bottom-right badge.
- Do not add Git, RAW, operation-progress, hidden, executable, MIME, or
  arbitrary user-tag badges in this pass. They either need another asynchronous
  subsystem or repeat information the file icon already communicates.

## Current State

- `FileIconCell.qml` already composes a native-folder icon with a bottom-right
  provider overlay, including a circular background and optional avatar. It is
  the correct shared rendering seam.
- `DirectoryModel` already calculates local attributes during enumeration:
  `QFileInfo::isSymLink()`, `!QFileInfo::isWritable()`, and an attributes
  text containing `L` and `R`. These facts must become explicit roles rather
  than being reconstructed from the display string.
- `DirectoryModel` already exposes `isArchiveFile`; archive detection must
  continue to use `ArchiveSupport::isArchiveExtension()`, not a second suffix
  list in QML.
- `FavoritesController` / `FavoritesStore` already own persisted pin state
  and expose `isPinned(path)`. That is the only source of truth for the star.
- `VolumeMonitor` and `QStorageInfo` already provide mounted-volume data for
  the storage UI. Local mount-point classification should be a small shared
  helper backed by that information, not an ad hoc scan in every model.

## Non-Goals

- Do not replace the main native/bundled type icon with a badge.
- Do not show every POSIX/Windows attribute as a corner glyph.
- Do not make badges clickable. Selection, context menu, drag, and double-click
  semantics must remain exactly as they are today.
- Do not inspect remote/provider paths through local `QFileInfo` or
  `QStorageInfo`.
- Do not refresh an entire folder synchronously merely to update one badge.
- Do not use raw component-local colours. Badge surfaces and warning treatment
  must use Theme tokens.
- Do not create a generic tag/label system as a side effect of the Favorites
  star. Existing Favorites tags remain a separate feature.

## Badge Semantics

| Kind | Applies to | Meaning | Primary icon treatment |
| --- | --- | --- | --- |
| `brokenLink` | Local symlink whose target cannot be resolved | Opening/copying may not do what the user expects | Link glyph with error treatment |
| `link` | Local symlink or Windows reparse-point link that resolves | Item is a reference, not an ordinary file/directory | Link/arrow glyph |
| `mountPoint` | Local directory equal to a mounted filesystem root | Entering it crosses into another filesystem/device | Small drive glyph |
| `locked` | Local item inaccessible to the current user/process | The location cannot be entered or the file cannot be read | Lock glyph |
| `archive` | Local regular archive supported by `ArchiveSupport` | Enter opens FM's archive navigation flow | Archive/container glyph |
| `pinned` | Path recorded as pinned by Favorites | User deliberately marked the item for quick access | Small star in the top-right corner |

Rules:

- `brokenLink` takes precedence over `link`.
- A broken link must not be called locked just because its target cannot be
  opened. Link state is based on link metadata; writability is independent.
- `mountPoint` is only for a directory itself, never its descendants.
- A mount root shows `mountPoint`, not a second lock; Properties and the
  attributes column retain the permission detail.
- `archive` is for physical local archive files only. Archive-provider
  children and `archive://` paths do not receive it.
- Pinned state is additive and always uses the top-right location. It can coexist
  with any one primary state badge.

## Architecture

### 1. Data Contract

Extend `FileEntry` with explicit inexpensive local-state fields. Suggested
initial shape:

- `isSymLink`
- `isBrokenSymLink`
- `isReadOnly` (already stored; keep it)
- `isLocked`
- `isMountPoint`
- `isPinned` is **not** stored by the local enumerator because it belongs to
  Favorites and can change independently.

Expose these `DirectoryModel` roles:

- `isSymLink`
- `isBrokenSymLink`
- `isReadOnly`
- `isLocked`
- `isMountPoint`
- `primaryBadgeKind`
- `isPinned`

`primaryBadgeKind` should be one stable string from:
`""`, `"broken-link"`, `"link"`, `"mount-point"`, `"locked"`, or
`"archive"`. It keeps priority policy in C++, gives QML a simple rendering
contract, and prevents the delegates from drifting.

Provider models may leave all local roles false/empty. A future provider may
supply its own semantic badge only through an explicitly designed provider
metadata field; do not infer local state for a provider URI.

### 2. Local State Resolver

Add a small C++ helper, for example `LocalFileBadgeResolver` under
`src/core`. It receives a local path plus existing `QFileInfo` metadata and
returns local badge facts. It must not know about QML, icon assets, or delegate
layout.

Responsibilities:

- preserve the existing platform-specific reparse-point/symlink detection;
- resolve a link target only when the item is already being enumerated or
  refreshed, then mark a link broken when its target is absent/unresolvable;
- classify accessibility for the effective user: traversal for directories and
  read access for files; keep `isReadOnly` as a separate attribute;
- ask a cached mount-point index whether an absolute directory path is a mount
  root;
- compute the primary-badge priority;
- never touch a path with a non-local scheme.

The mount-point index should be shared and refreshed when `VolumeMonitor`
reports volume changes. On Windows it may be based on volume roots/reparse
mounts; on Linux it should follow the project mount-info roadmap instead of
treating every `QStorageInfo` result as authoritative. During the first
implementation, root filesystem, mounted ISO, removable drive, and network
mount roots are sufficient.

### 3. Favorites Integration

Do not call `favoritesController.isPinned(path)` from each pooled QML
delegate. That would create needless QML-to-C++ work during scrolling.

Instead:

1. `DirectoryModel` receives a narrow pinned-path lookup/snapshot API from
   `FavoritesController` or a small signal-driven adapter.
2. On initial scan, the model marks each visible local entry from that snapshot.
3. On pin/unpin, Favorites emits normalized changed paths.
4. Each open `DirectoryModel` updates only affected rows and emits
   `dataChanged()` for `isPinned`; no directory rescan is needed.
5. Normalize through the same path-key rules as `FavoritesStore`, including
   platform case behaviour, before comparing paths.

Provider and virtual paths may keep their existing pin behaviour if supported
by Favorites, but this plan only promises local-file icon stars.

### 4. Shared QML Renderer

Generalize the provider-only overlay in `FileIconCell.qml` into two visual
layers:

1. `PrimaryBadgeOverlay`, bottom-right:
   - receives `primaryBadgeKind`;
   - uses the existing circular/rounded backing treatment;
   - maps the stable kind to a shipped badge asset;
   - has accessible/tooltip text supplied by a central mapping;
   - remains above native icons and below the selection control.

2. `PinnedBadgeOverlay`, top-right:
   - receives `isPinned`;
   - draws a small star and no text;
   - remains non-interactive and must not change the icon cell's implicit size.

Provider overlays and avatars must continue to work. Make the relationship
explicit:

- provider folder/avatar overlays use their current provider path logic;
- local primary badges use `primaryBadgeKind`;
- a provider overlay wins the bottom-right corner for provider paths;
- local paths use only local primary badges;
- the top-right pinned star can coexist with either.

Pass the new roles through `FileDelegate.qml`, `FileBriefDelegate.qml`,
`FileTableDelegate.qml`, and the grid delegate in `FilePanel.qml` to the
single `FileIconCell`. Do not duplicate badge geometry in those delegates.

### 5. Visual Assets and Theme

Add a small cohesive asset set, e.g.:

- `badge-link.svg`
- `badge-link-broken.svg`
- `badge-mount.svg`
- `badge-lock.svg`
- `badge-archive.svg`
- `badge-pinned.svg`

The assets should be recognisable at 16 px and 24 px, with no embedded text.
The broken-link variant may use the existing semantic warning/error treatment;
ordinary link, mount, lock, archive, and star remain quiet. Background, border,
and warning colours must be Theme-owned tokens. Verify dark and light themes;
do not hardcode new colours in a delegate.

## Implementation Plan

### Phase 1: Model Roles and Local Classification

1. Extend `FileEntry` and the local enumeration path with explicit link and
   broken-link facts.
2. Preserve existing Windows reparse-point behaviour while distinguishing a
   resolvable link from a broken one.
3. Add model roles for link, broken link, locked, and primary badge kind.
4. Derive archive primary kind only from the existing archive helper.
5. Add unit tests for priority and local/provider gating.

Verify:

- normal file has no badge;
- a valid file symlink and directory symlink receive `link`;
- a dangling symlink receives `broken-link`;
- an inaccessible regular file receives `locked`;
- an inaccessible symlink still shows `link`;
- ZIP/7z/RAR supported by current archive support receive `archive`;
- `archive://`, `gdrive://`, MTP, and Favorites virtual paths do not acquire
  local filesystem badges.

### Phase 2: Mount-Point Index

1. Extract/introduce a cached local mount-root index near `VolumeMonitor` or
   the planned mount-info helper.
2. Normalize roots and compare them to normalized local directory paths.
3. Refresh the cache on the existing volume-change path.
4. Add `isMountPoint` and include it in primary-badge priority.

Verify:

- filesystem root is classified as a mount point;
- a mounted USB/removable drive root is classified;
- a mounted ISO root is classified where the platform supports it;
- a normal subdirectory beneath a mount root is not classified;
- removal/eject refreshes the visible badge without stale mount labels;
- scanning a large folder does not call `QStorageInfo` per entry.

### Phase 3: Favorites Star Data Flow

1. Add a normalized pinned-path snapshot/query at the C++ boundary.
2. Inject/bind it into each `DirectoryModel` without introducing a dependency
   from the low-level local enumerator to QML.
3. Add `isPinned` model role.
4. Subscribe models to pin/unpin changes and update only matching rows.
5. Keep context menu, command palette, sidebar Favorites, and multi-selection
   pin actions as the existing authority for changing state.

Verify:

- pinning one visible file or folder makes its star appear immediately;
- unpinning removes it immediately;
- pinning a path in the other panel updates it too when visible;
- a renamed/moved pinned path follows existing Favorites semantics and never
  leaves a star on a different path;
- scrolling a large directory does not issue one Favorites lookup per delegate.

### Phase 4: Shared Rendering

1. Add the two generic overlay layers to `FileIconCell.qml`.
2. Route model roles through all three file views and the grid view.
3. Retain provider folder overlays and Telegram avatar composition unchanged.
4. Ensure badge placement scales with `iconSize` and stays inside the cell at
   minimum and maximum UI font scales.
5. Keep all badge layers non-interactive so existing selection-toggle hit tests,
   drag initiation, and context menus are unaffected.

Verify:

- grid, brief, and table show the same semantic badge;
- thumbnails still render above their base image while the badge remains legible;
- provider folder overlays and avatars are unchanged;
- pin star coexists with a link/locked/archive/mount badge;
- no badge intercepts a click, drag, double-click, or right-click.

### Phase 5: Tooltips, Properties, and Accessibility

1. Extend the existing item tooltip composition with concise state text:
   `Symbolic link`, `Broken symbolic link`, `Locked`, `Mount point`,
   `Archive — Enter to browse`, and `Pinned in Favorites`.
2. Keep translation strings in English source through `qsTr()`/C++ `tr()`.
3. Ensure Properties continues to expose the authoritative detailed attributes;
   a badge must never be the only way to discover a state.
4. Add accessible names/descriptions to badge layers where Qt accessibility
   exposes them.

Verify:

- keyboard-only selection and tooltip/help path can reveal the state;
- English source strings are translatable;
- a broken link is clearly distinguishable from a valid link without relying
  only on colour.

### Phase 6: Regression and Performance Pass

1. Add focused unit tests for `LocalFileBadgeResolver`, badge priority,
   normalized mount-root comparison, and Favorites update routing.
2. Add manual cases to `docs/qa-regression-suite.md`.
3. Measure a large local folder with badges enabled against the existing
   baseline. Badge calculation must occur during enumeration or targeted
   updates, never in a QML binding that performs filesystem I/O.
4. Run the normal release build and tests.

## QA Cases

- Create valid file and directory symlinks, then delete their targets. Expected:
  valid links show the link badge; dangling links change to broken-link after
  refresh/watch update.
- Make a directory non-traversable or a file unreadable for the current user.
  Expected: lock badge; a writable but traversable directory has no lock badge.
- Open a mounted USB drive and mounted ISO. Expected: only each root directory
  shows the mount badge.
- Pin a file and folder from the context menu, then unpin from Favorites.
  Expected: stars update in every visible panel without full navigation reload.
- View ZIP, 7z, and RAR files. Expected: archive badge and existing Enter/open
  archive navigation still work; an item inside `archive://` has no extra local
  archive badge.
- Check grid, brief, table, native icons on/off, thumbnails on/off, dark theme,
  light theme, minimum and maximum supported font scale.
- Check a large source tree while rapidly scrolling. Expected: no new blocking
  filesystem work, binding loops, delegate churn, or click/drag regressions.
- Check provider folders, Telegram avatars, Google Drive special folders, and
  Instagram/Telegram provider badges. Expected: unchanged visual composition.

## Acceptance Criteria

The feature is complete when:

- every local badge is produced from C++ model data, not QML filesystem probes;
- valid links, broken links, locked items, mount roots, archives, and pinned
  items display the defined semantics;
- only one bottom-right primary state badge is shown, while pin state can coexist
  in the top-right;
- provider overlay/avatar behaviour remains intact;
- all file-panel views render the same state;
- no badge changes normal mouse, keyboard, drag, selection, or archive-open
  behaviour;
- mount and Favorites updates are targeted and asynchronous-friendly;
- release build, targeted tests, QA cases, and `git diff --check` pass.
