# External Drag And Drop Plan

## Implementation Status - 2026-06-19

Goal: add OS/application interoperability without weakening the deliberately
limited internal panel-to-panel drag/drop workflow.

Phase 1 incoming local-file drops are implemented and manually checked.
FMQml now accepts external local `file://` URL drops into writable local panel
folders, with copy-only execution and skip-only destination-name conflict
handling.

Manual QA notes from 2026-06-19:

- Explorer and local applications copied local files into the target panel.
- Left-panel and right-panel drops targeted the panel under the pointer.
- Archive destinations rejected the drop and showed the platform disallowed
  cursor.
- Browser image drops did not copy, which is acceptable for Phase 1 because
  browser-provided image data is commonly a web URL, blob, or promised data
  rather than a local `file://` URL.
- A status-scope bug was fixed: rejected external drops now show the rejection
  message only in the target panel instead of both panels.
- Outgoing drag from FMQml into other applications remains intentionally
  unimplemented. It should be treated as a separate feature decision, not a
  small extension of the incoming-drop path.

This plan covers two directions, but the first implementation should cover only
incoming external files:

- accepting files dragged from other applications into an FMQml panel;
- dragging FMQml local files out to other applications.

The existing internal opposite-panel drag/drop is a separate feature and should
remain separate.

The external incoming local-file drop path and the tested internal
opposite-panel workflow are both enabled by default. They remain separate
interactions with independent routing and destination rules.

## Original State

`qml/components/filepanel/FilePanelDropOverlay.qml` already has a basic external
drop path:

- `DropArea.keys: ["text/uri-list"]`;
- disabled for archive paths and managed ISO mounts;
- on drop, checks `drop.hasText`;
- treats `drop.text` as one path;
- calls `workspaceController.operationQueue.copyTo(paths, currentPath)`.

Problems with the current receiver:

- `text/uri-list` can contain multiple URLs, but the code reads one text value;
- it does not use `DragEvent.hasUrls` / `DragEvent.urls`;
- it does not normalize `file://` URLs to local filesystem paths in C++;
- it has no explicit capability check or status reason for rejected drops;
- it does not distinguish Copy/Move/Link proposed actions;
- it lives entirely in QML even though path policy belongs in controllers.

There is still no external drag source for dragging FMQml selections into
Explorer, editors, upload dialogs, or other file consumers. That remains a
future phase and may be skipped if the product does not need it.

## Relevant Qt Behavior

Primary docs checked:

- Qt Drag and Drop overview:
  `https://doc.qt.io/qt-6/dnd.html`
- QML `Drag`:
  `https://doc.qt.io/qt-6/qml-qtquick-drag.html`
- QML `DragEvent`:
  `https://doc.qt.io/qt-6/qml-qtquick-dragevent.html`
- QML `DropArea`:
  `https://doc.qt.io/qt-6/qml-qtquick-droparea.html`
- `QMimeData`:
  `https://doc.qt.io/qt-6/qmimedata.html`

Important points:

- `QMimeData::setUrls()` / `urls()` maps to `text/uri-list`.
- QML `DragEvent` exposes `hasUrls`, `urls`, `formats`,
  `proposedAction`, `supportedActions`, and `accept(action)`.
- Qt's drag threshold should follow platform style hints where possible.
- With `MoveAction`, the drag source is responsible for deleting originals if
  the target accepted a move.
- Qt maps platform drag formats, but Windows virtual file formats such as mail
  attachments may appear as platform-specific MIME formats rather than local
  `file://` URLs.

## Scope

### Phase 1 Scope: Incoming Local File Copy Only

Accept external local files into writable FMQml folders:

- source: OS/apps that provide local `file://` URLs;
- destination: current folder of the panel under the pointer;
- operation: copy only;
- multiple files/folders supported;
- destination must be handled by the local filesystem provider only;
- reject destination virtual roots, archives, managed ISO mounts, remote/provider
  paths, and unwritable current paths;
- keep destination at panel current folder, not row/folder under pointer.

Conflict policy:

- reject only source items whose basename already exists in the destination
  current folder;
- copy the remaining non-conflicting source items;
- do not show conflict/rename/overwrite UI from the drop path;
- if every item conflicts, reject the operation and copy nothing;
- report a concise status summary, for example:
  `Copied 8 items. Skipped 2 existing items.`

Reason: external drag/drop is not a good place for the first implementation to
open an interactive conflict resolver. Per-item auto-reject keeps the operation
useful for batches while preventing accidental overwrite.

### Explicit Non-Goals For Phase 1

- No external move semantics.
- No outgoing drag from FMQml to other applications.
- No dragging remote/provider files by staging temporary local copies.
- No dragging archive-internal entries by extracting temporary copies.
- No Windows virtual file ingestion from `FileGroupDescriptor/FileContents`.
- No dropping onto subfolders inside a panel.
- No sidebar/tree/pathbar external drop targets.
- No custom promised-file provider implementation.

## Recommended Architecture

### 1. Add C++ External Drop API

Add explicit receiver methods to `WorkspaceController` first. If this grows,
extract a small controller later.

```cpp
Q_INVOKABLE QVariantMap externalDropCapabilities(
    const QVariantList &urls,
    int destinationPanel,
    const QString &destinationPath);

Q_INVOKABLE bool copyExternalUrlsToPanel(
    const QVariantList &urls,
    int destinationPanel,
    const QString &destinationPath);
```

Capability result contract:

- `canCopy: bool`
- `reason: string`
- `destinationPath: string`
- `acceptedPaths: QStringList`
- `rejectedPaths: QStringList`
- `conflictCount: int`
- `invalidCount: int`

Validation:

- destination panel exists;
- destination path still equals the panel current path;
- operation queue is not busy;
- destination panel is not virtual root;
- destination current path is local filesystem only:
  - no URI scheme other than empty / local file;
  - no `archive://`;
  - no managed ISO mount;
  - no provider path such as `gdrive://`;
- destination can create children through existing access checks;
- incoming URLs are non-empty local files only;
- each normalized source exists and is readable/copyable;
- reject if every source is already in the destination folder;
- classify any source basename conflict with an existing destination child path
  as rejected, but keep non-conflicting sources accepted.

Execution:

- re-run capability validation;
- reject if destination path changed;
- copy only `acceptedPaths`;
- do not call the operation queue if `acceptedPaths` is empty;
- show a skipped/conflict summary if some items were rejected.

Reason: URL parsing, local-file validation, provider/archive decisions, and
copy/move/delete policy belong in C++ rather than in QML glue.

### 2. Replace Receiver Parsing

Update `FilePanelDropOverlay.qml`:

- accept only when `drop.hasUrls`;
- pass `drop.urls` to the C++ capability method;
- call `drop.accept(Qt.CopyAction)` for allowed external local file drops;
- on `onDropped`, pass the same URL snapshot and destination path to C++;
- show allowed/denied overlay using controller-provided reason.

Keep the existing internal `FilePanelOppositeDropOverlay.qml` separate.

### 3. Defer Outgoing Drag

Dragging FMQml files to other applications remains a follow-up feature. Do not
touch delegate drag initiation for Phase 1 incoming drops.

If this is implemented later, keep it separate from the current internal
opposite-panel drag coordinator:

- source should be limited to selected local filesystem paths;
- provider, archive, managed ISO, and virtual-root entries should not be
  exported unless a later staging/materialization design exists;
- use `QMimeData::setUrls()` / `Qt.url` data for local file URLs;
- support copy semantics first;
- do not implement external move until ownership/deletion semantics are tested
  against Explorer and at least one non-FM target application;
- internal opposite-panel drag must keep using the explicit Copy/Move/Cancel
  menu and must not turn into a native OS drop.

### 4. Keep Internal And External Paths Distinct

Internal panel-to-panel drag currently provides:

- custom preview;
- opposite-panel target overlay;
- explicit Copy/Move/Cancel menu;
- snapshot-based execution.

Incoming external drop should:

- remain available whenever no internal drag is active;
- be disabled or visually suppressed only while an enabled internal drag is
  active;
- avoid showing the internal opposite-panel menu;
- always copy into the panel current folder under the pointer;
- never target a child folder under the pointer;
- never use `activePanel` to infer destination.

### 5. Optional Phase 2 Move Support

Only after copy-only export/import is stable:

- allow `Qt.MoveAction` for external drops from apps that propose move;
- for incoming external move, copy first; source app owns deletion if it is the
  drag source and accepted move semantics apply;
- for outgoing external move, only support selected local paths with delete
  capability and only after confirming that Qt returns `MoveAction`;
- record operation history only for moves FMQml actually performs itself.

This phase needs careful manual testing against Explorer, another FMQml
instance, archive tools, and common editors/upload dialogs.

## Proposed Implementation Phases

### Completed: Phase 1A Harden Incoming External Drops

Files:

- `qml/components/filepanel/FilePanelDropOverlay.qml`
- `src/controllers/WorkspaceController.h`
- `src/controllers/WorkspaceController.cpp`

Implemented tasks:

1. Add URL normalization helper in C++.
2. Add external drop capability method.
3. Add copy execution method.
4. Update QML receiver to use `drop.hasUrls` / `drop.urls`.
5. Accept only `Qt.CopyAction`.
6. Reject conflicting items before queueing copy.
7. Add status messages for rejected drops.

Manual verification:

- Explorer single file -> panel copies.
- Explorer multiple files/folders -> panel copies all.
- Drop into left panel targets left panel current folder.
- Drop into right panel targets right panel current folder.
- Drop into archive/ISO/virtual root rejects.
- Drop into remote/provider path rejects.
- Drop into unwritable folder rejects.
- Drop while operation queue busy rejects.
- Drop with a mixed conflict copies only non-conflicting items and reports the
  skipped count.
- Drop where every item conflicts rejects the operation and copies nothing.
- Dropping text/URLs or browser image data does not create bogus paths.

### Completed: Phase 1B QA Documentation

Added a focused incoming external drag/drop block to
`docs/qa-regression-suite.md`.

Covered cases:

- Explorer -> FMQml copy single file.
- Explorer -> FMQml copy multiple files/folders.
- Drop into each panel targets that panel's current folder.
- Reject read-only destination import.
- Reject archive/ISO/provider/virtual destination.
- Skip destination name conflicts while copying non-conflicting items.
- Reject all-conflict drops.
- Reject non-local URL/text/browser drops.
- Internal panel-to-panel drag still shows explicit menu.
- External drag does not show internal drop menu.

## Risks / Watch Points

- QML `DropArea.keys` can hide useful drops if too restrictive; prefer capability
  checks over broad QML assumptions.
- `drop.text` is not a safe replacement for `drop.urls`.
- External MoveAction can cause data loss if implemented prematurely.
- Conflict handling should be skip-only until there is a dedicated conflict
  resolver that is safe to open from a drop flow. Never overwrite or auto-rename
  in this first implementation.
- Provider and archive entries are not necessarily local files. Exporting them
  requires staging/materialization, which should be a separate feature.
- Windows virtual file drops are a different feature from local file URL drops.
- Avoid sharing state with the internal drag coordinator except for explicit
  cancellation.

## Recommended Next Step

Before starting outgoing drag, fix any unrelated behavior regressions found in
manual use. The next drag/drop feature should be a separate outgoing local-file
drag phase, not an expansion of incoming browser, promised-file, or virtual-file
drops.

As of 2026-06-19, there is no commitment to implement outgoing drag. The current
stable scope is:

- incoming external local-file copy drops into FMQml panels.

The internal opposite-panel drag/drop workflow is enabled by default.
