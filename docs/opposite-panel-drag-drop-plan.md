# Opposite Panel Drag And Drop Plan

## Implementation Status - 2026-08-31

Current state: the deliberately limited internal opposite-panel drag/drop flow
is implemented and builds. The feature is intentionally scoped as a
panel-to-panel command shortcut, not general OS drag/drop.

The internal mouse-driven workflow is now enabled by default after extended
testing. Its former `useLimitedDragNDrop` experimental setting has been removed;
the normal split-view, capability, rename, and operation-state guards remain.

Implemented files:

- `src/controllers/FilePanelController.h`
- `src/controllers/FilePanelController.cpp`
- `src/controllers/WorkspaceController.h`
- `src/controllers/WorkspaceController.cpp`
- `qml/components/FileWorkspace.qml`
- `qml/components/FilePanel.qml`
- `qml/components/FileDelegate.qml`
- `qml/components/FileBriefDelegate.qml`
- `qml/components/FileTableDelegate.qml`
- `qml/components/filepanel/FilePanelDragCoordinator.qml`
- `qml/components/filepanel/FilePanelDragPreview.qml`
- `qml/components/filepanel/FilePanelOppositeDropOverlay.qml`
- `qml/components/filepanel/OppositePanelDropMenu.qml`

### Completed: snapshot capability helpers

`FilePanelController` now exposes C++ helpers for validating an explicit
snapshot path list:

```cpp
bool canCopyPaths(const QStringList &paths) const;
bool canDeletePaths(const QStringList &paths) const;
```

These helpers reuse the same private path checks already used by the existing
selection methods:

- `pathCanCopy(...)`
- `pathCanDelete(...)`
- virtual-root/read-only-container checks

Existing behavior is preserved by making:

```cpp
canCopySelection()   -> canCopyPaths(selectedPaths())
canDeleteSelection() -> canDeletePaths(selectedPaths())
```

Reason: internal drag must operate on the drag-start snapshot, not on live
selection at menu-trigger time.

### Completed: explicit WorkspaceController drop API

`WorkspaceController` now exposes explicit QML-callable methods that do not
depend on `activePanel`:

```cpp
Q_INVOKABLE QVariantMap oppositePanelDropCapabilities(
    int sourcePanel,
    const QStringList &sources,
    int destinationPanel);

Q_INVOKABLE bool copyDroppedSelectionToPanel(
    int sourcePanel,
    const QStringList &sources,
    int destinationPanel,
    const QString &destinationPath);

Q_INVOKABLE bool moveDroppedSelectionToPanel(
    int sourcePanel,
    const QStringList &sources,
    int destinationPanel,
    const QString &destinationPath);
```

Panel side convention:

- `0` = left panel
- `1` = right panel

`WorkspaceController::panelBySide(int side)` was added as the private lookup
helper.

### Capability result contract

`oppositePanelDropCapabilities(...)` returns a `QVariantMap` with:

- `canCopy: bool`
- `canMove: bool`
- `reason: string`
- `copyReason: string`
- `moveReason: string`
- `destinationPath: string`

The method validates:

- split view is enabled;
- source and destination panels exist;
- source and destination are different;
- destination is exactly the opposite panel;
- sources are not empty;
- operation queue is not busy;
- source and destination are not virtual roots;
- source can copy the snapshot paths;
- destination can create in its current path;
- copy rejects all-sources-already-in-destination;
- move rejects remote-provider source/destination parity with existing
  Shift+F5 behavior;
- move requires source delete capability for the snapshot paths.

Destination path is reported as the destination panel's current path at the
time capabilities are queried.

### Execute method behavior

`copyDroppedSelectionToPanel(...)`:

- re-runs capability validation;
- rejects if `destinationPath` no longer matches the destination panel's
  current path;
- then calls existing `copyPathsToPanel(...)`.

`moveDroppedSelectionToPanel(...)`:

- re-runs capability validation;
- rejects if `destinationPath` no longer matches the destination panel's
  current path;
- then calls `OperationQueue::moveTo(...)`.

Destination comparison uses `pathsReferToSameDropDestination(...)`:

- URI paths compare trimmed strings case-insensitively;
- local paths compare through existing `normalizedLocalPath(...)`.

This is important because the first QML menu implementation should capture the
destination at menu-open/drop time and pass it back unchanged. If either panel
navigates before the user chooses Copy/Move, the operation is rejected instead
of silently targeting a new folder.

### Existing actions now use the new path

Existing active-panel commands were converted to thin wrappers:

- `copyActiveSelectionToOpposite()`
- `moveActiveSelectionToOpposite()`

They still read the active panel only at command entry to capture:

- source panel side;
- selected path snapshot;
- destination panel side;
- destination current path.

They then call the new explicit drop execute methods. This keeps existing
keyboard/menu copy-to-opposite and move-to-opposite behavior aligned with the
future drag/drop path.

### Verification performed

Build command:

```powershell
cmd.exe /s /c "`"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat`" -arch=x64 -host_arch=x64 && cmake --build build\6_11_1_MS-Release --config Release --target fm"
```

Result: build completed successfully and linked `fm.exe`.

Note: running `cmake --build ...` directly from the default PowerShell session
previously failed because the MSVC standard include environment was not loaded.
Use the VS Developer Command Prompt wrapper above for reliable verification in
this workspace.

### Completed: initial QML drag coordinator and target overlay

`FilePanelDragCoordinator.qml` now owns one internal drag snapshot:

- `sourcePanelSide`
- `destinationPanelSide`
- `sourceController`
- `destinationController`
- `paths`
- `itemCount`
- `destinationPath`
- `canCopy`
- `canMove`
- `reason`
- `copyReason`
- `moveReason`

It exposes the Phase 2 helper surface:

- `canStartDrag(panelSide, path)`
- `startDrag(panelSide, path)`
- `isOppositePanel(panelSide)`
- `canDropOn(panelSide)`
- `cancelDrag(reason)`

`startDrag(...)` captures `selectedPaths()` from the source panel and calls
`workspaceController.oppositePanelDropCapabilities(...)`. This keeps the QML
state aligned with the explicit C++ drop API and avoids using live selection
after session start.

Cancellation is wired for:

- split view closing;
- operation queue becoming busy;
- either panel changing path;
- inline rename becoming active at the workspace layer.

`FileWorkspace.qml` creates one coordinator and passes it to both panels.
`FilePanel.qml` now has:

- `panelSide`
- `dragCoordinator`
- `dropTargetActive`
- `dropTargetAllowed`
- `dropTargetDeniedReason`

`FilePanelOppositeDropOverlay.qml` shows internal opposite-panel target
feedback only with tint and outline, without an explanatory text label. It is
separate from `FilePanelDropOverlay.qml`, which remains the external `DropArea`
path.

### Completed: delegate drag wiring and selection interaction

Normal delegates now start the internal drag session after the shared movement
threshold:

- `FileDelegate.qml` for list rows;
- `FileBriefDelegate.qml` through the adaptive path;
- `FileTableDelegate.qml` through the adaptive path;
- the normal grid delegate embedded in `FilePanel.qml`.

Lightweight resize delegates remain non-draggable.

Important event-routing behavior:

- press on empty panel space starts the existing rubber-band selection path;
- press/drag on an already selected item can start drag from the full item
  surface, because this matches the expected "drag selected things" workflow;
- first drag from an unselected item is deliberately narrow so rubber-band
  selection remains easy to start;
- list/details initial drag starts only from the file icon area;
- brief initial drag starts only from a compact center handle inside the visual
  icon frame, so larger density-driven icons do not make the first-drag hitbox
  too greedy;
- grid initial drag starts from the visible icon/thumbnail surface only;
- adaptive brief/details delegates proxy `isPointOnDragSurface(...)` to their
  loaded full delegate so selection and drag rules stay consistent across view
  mode reuse;
- when the item is already selected, list/details/brief/grid allow dragging
  from the full row/tile, which avoids forcing the user to re-hit the icon for
  an already selected selection;
- delegates use `preventStealing` while a drag candidate is active so the
  view/rubber-band layer does not steal the mouse before the threshold;
- rubber-band selection and internal item drag use the same movement threshold;
- starting rubber-band selection explicitly cancels any active internal drag
  session;
- plain click still uses the existing selection path;
- dragging an unselected item first calls the existing click selection path,
  then starts a drag snapshot for the resulting selection.

Current visual behavior:

- dragging selected items toward the opposite panel highlights that whole panel;
- release over the opposite panel opens the explicit Copy/Move/Cancel menu;
- releasing elsewhere cancels the drag without executing an operation.

Manual usability fixes completed after the initial implementation:

- details and brief views no longer let the full row greedily capture the first
  drag from an unselected item;
- click selection in details and brief remains visible and active after the drag
  routing changes;
- grid first-drag hitbox was tightened to the visible icon/thumbnail surface;
- selected items in all normal views can still be dragged from the whole
  selected item surface;
- brief view density now changes spacing independently from text size, while
  the visual icon can grow into available row height without expanding the
  initial drag hitbox.

### Completed: initial drop action menu

`OppositePanelDropMenu.qml` now shows the explicit drop confirmation menu after
releasing an internal selection drag over the opposite panel.

The menu freezes the coordinator snapshot at popup time:

- source panel side;
- destination panel side;
- source path list;
- item count;
- destination path;
- copy/move capability flags and reasons.

It shows exactly:

- `Copy N items`
- `Move N items`
- `Cancel operation`

Copy and Move call the explicit controller APIs:

- `copyDroppedSelectionToPanel(...)`
- `moveDroppedSelectionToPanel(...)`

If the active panel changes while the drop menu is open, the menu closes and
the pending drag/drop operation is canceled. This keeps a stale confirmation
menu from executing after the user has moved context to another panel.

`FilePanel` now receives the release mouse point from normal delegates, checks
whether the release point is inside the opposite panel, and opens the menu only
for an allowed opposite-panel drop. Releasing elsewhere cancels the drag
without executing an operation.

### Completed: initial drag preview visual

`FilePanelDragPreview.qml` now shows a floating internal drag affordance while
an internal selection drag is active.

The preview:

- follows the pointer across the workspace;
- shows a compact stack of up to three selected item icons;
- always shows a numeric badge with the drag snapshot item count;
- stays separate from Qt external drag/drop visuals.

Because the floating drag preview and numeric badge now communicate what is
being dragged, the opposite-panel overlay intentionally has no central text
label such as `Drop to copy or move to this panel`.

`FilePanelDragCoordinator.qml` now stores preview item metadata for the drag
snapshot and current pointer position in workspace coordinates. Normal list,
brief, table, and grid delegates update the pointer position while dragging.

### Completed: drag cursor feedback

Internal selection drag now uses explicit allowed/disallowed cursor feedback:

- while an internal drag is active, the cursor is forbidden outside the allowed
  opposite panel target;
- over the allowed opposite panel target, the cursor returns to the normal
  arrow;
- source/active panel areas therefore do not suggest that dropping there is
  valid;
- cursor state is driven by the shared `FilePanelDragCoordinator` pointer
  position, not by `activePanel`;
- `WorkspaceController::setDragCursorShape(...)` and
  `WorkspaceController::clearDragCursorShape()` use Qt override cursor APIs so
  feedback still works while the mouse is grabbed by the delegate that started
  the drag;
- `FileWorkspace.qml` clears the override cursor when the drag ends and when
  the workspace is destroyed.

Reason: QML `MouseArea.cursorShape` and overlay `HoverHandler` were not enough
for this interaction because the delegate that starts the drag can keep the
mouse grab during the whole gesture.

### Not implemented yet

Remaining work for this internal opposite-panel feature:

- run the focused panel-to-panel QA block from `docs/qa-regression-suite.md`.

Manual smoke note: basic use in the normal views was reported working on
2026-06-19, including Copy, Move, Cancel, drag preview, initial drag hitboxes,
selected-item full-surface drag, rubber-band selection, and cursor feedback.

QA regression documentation now includes focused panel-to-panel drag/drop
coverage, including rubber-band interaction and menu cancellation on panel
switch.

The existing `qml/components/filepanel/FilePanelDropOverlay.qml` remains the
incoming external `DropArea` path. Keep internal selection drag separate from
this external drop behavior.

### Continue From Here

Next recommended step: run the default-off QA block first. Only after
default-off behavior is clean should the experimental-on panel-to-panel QA block
be run.

### Implemented: guard internal drag/drop behind `useLimitedDragNDrop`

Reason: the risky part of this feature is not the file operation execution
path. The risky part is the mouse UX: delegate `MouseArea` routing,
`dragCandidate`, `preventStealing`, drag hitbox checks, cursor overrides,
floating previews, and opposite-panel overlays. These can interfere with the
baseline rubber-band selection rectangle, which is a core file-manager feature.

The required default-off rule is strict:

- `FilePanelDragCoordinator` must not be created.
- `FilePanelDragPreview` must not be created.
- `FilePanelOppositeDropOverlay` must not be created.
- `OppositePanelDropMenu` must not be created.
- The workspace-wide cursor capture/override overlay must not be created.
- `FilePanel.dragCoordinator` must be `null`.
- Normal delegates must not enter the internal drag-candidate path.
- `preventStealing` must not be enabled because of internal drag state.
- `beginRubberBandPress(...)` must not run the drag-vs-rubber-band routing
  branch.
- External local-file drops into FMQml must continue to work through
  `FilePanelDropOverlay.qml`.

Start-here implementation checklist:

- `src/controllers/AppSettingsController.h`
- `src/controllers/AppSettingsController.cpp`
- `qml/components/SettingsDialog.qml`
- `qml/components/FileWorkspace.qml`
- `qml/components/FilePanel.qml`
- `qml/components/FileDelegate.qml`
- `qml/components/FileBriefDelegate.qml`
- `qml/components/FileTableDelegate.qml`

The first implementation pass should add the setting and make the default-off
path inert before adding any new cursor affordance. The second pass should add
the guarded ready-to-drag cursor behavior. The third pass should run QA.

Implementation plan:

1. Extend `AppSettingsController` with persisted
   `Q_PROPERTY(bool useLimitedDragNDrop ...)`, default `false`, stored in the
   `appearance` group and included in settings export/import.
2. Add an experimental toggle in `SettingsDialog.qml`, preferably in the app or
   workspace section, with copy that makes clear this enables panel-to-panel
   drag/drop testing.
3. In `FileWorkspace.qml`, replace the eagerly-created
   `FilePanelDragCoordinator` with a `Loader` active only when
   `appSettings.useLimitedDragNDrop` is true. Expose:

   ```qml
   readonly property bool limitedDragNDropEnabled: typeof appSettings !== "undefined"
                                                   && appSettings
                                                   && appSettings.useLimitedDragNDrop
   readonly property var panelDragCoordinator: dragCoordinatorLoader.item
   ```

   Pass `dragCoordinator: limitedDragNDropEnabled ? panelDragCoordinator : null`
   and `limitedDragNDropEnabled` to both panels.
4. Also create `FilePanelDragPreview` and the workspace cursor overlay through
   loaders guarded by `limitedDragNDropEnabled`, not by `visible: false`.
5. In `FilePanel.qml`, add `property bool limitedDragNDropEnabled: false` and
   a derived `internalDragEnabled` that requires both the setting and a
   non-null coordinator. Gate `dropTargetActive`, `dropTargetAllowed`,
   `dropTargetDeniedReason`, `externalDropSuppressed`,
   `updateSelectionDragCandidate(...)`, `updateSelectionDragPosition(...)`,
   `finishSelectionDrag(...)`, and `selectionDragCursorShape()` through this
   property.
6. Create `OppositePanelDropMenu` and `FilePanelOppositeDropOverlay` through
   guarded loaders in `FilePanel.qml` so they do not exist while the setting is
   off.
7. In `FileDelegate.qml`, `FileBriefDelegate.qml`,
   `FileTableDelegate.qml`, and the embedded grid delegate in `FilePanel.qml`,
   make `dragCandidate` require `panel.internalDragEnabled`. Cursor feedback and
   drag-specific `preventStealing` must also require `panel.internalDragEnabled`.
8. In `beginRubberBandPress(...)`, run the item drag ownership branch only when
   `internalDragEnabled` is true. When it is false, the code must follow the
   baseline rubber-band/click path.
9. Keep the C++ opposite-panel execution API unless a later cleanup explicitly
   removes it. The guard is primarily QML/session UX protection; existing
   keyboard/menu copy-to-opposite and move-to-opposite behavior should not be
   accidentally broken.

Suggested QML shape for the `FileWorkspace.qml` loader guard:

```qml
readonly property bool limitedDragNDropEnabled: typeof appSettings !== "undefined"
                                                && appSettings
                                                && appSettings.useLimitedDragNDrop
readonly property var panelDragCoordinator: dragCoordinatorLoader.item

Loader {
    id: dragCoordinatorLoader
    active: root.limitedDragNDropEnabled
    sourceComponent: FilePanelDragCoordinator {
        workspaceController: root.workspaceController
        renamingActive: root.isRenaming
        onActiveChanged: root.updatePanelDragCursor()
        onPointerXChanged: root.updatePanelDragCursor()
        onPointerYChanged: root.updatePanelDragCursor()
        onCanCopyChanged: root.updatePanelDragCursor()
        onCanMoveChanged: root.updatePanelDragCursor()
        onDestinationPanelSideChanged: root.updatePanelDragCursor()
    }
}
```

All existing references to `panelDragCoordinator.active` in `FileWorkspace.qml`
must become null-safe through `panelDragCoordinator && panelDragCoordinator.active`
or equivalent helpers. When the loader becomes inactive, the workspace should
clear any override cursor.

Suggested `FilePanel.qml` derived property:

```qml
property bool limitedDragNDropEnabled: false
readonly property bool internalDragEnabled: root.limitedDragNDropEnabled
                                           && root.dragCoordinator
```

This property is the only flag delegate code should consult. Do not let
delegates read `appSettings` directly for this behavior.

Planned experimental cursor affordance, also behind the same guard:

- When `useLimitedDragNDrop` is true and no drag is active, hovering an
  unselected item's drag surface should show a ready-to-drag cursor such as
  `Qt.OpenHandCursor`.
- Hovering a selected item should show the same affordance over the draggable
  selected-item surface, including multi-selection cases.
- While an internal drag is active, keep the existing allowed/disallowed cursor
  feedback: normal arrow over the allowed opposite-panel target, forbidden
  elsewhere.
- When `useLimitedDragNDrop` is false, no ready-to-drag cursor should appear
  anywhere.

### Current Risks / Watch Points

- Do not use `activePanel` in new QML drag execution paths.
- Do not use live selection after drag start; always use the coordinator's
  `paths` snapshot.
- Do not broaden external drag/drop behavior.
- Do not let experimental internal drag mouse handling run in the default
  settings state.
- Do not create disabled internal drag objects with `visible: false`; prefer
  guarded `Loader` creation so the default-off path has no active session
  state, connections, cursor overrides, or event-routing side effects.
- Do not target folders under the pointer; destination is always the opposite
  panel `currentPath`.
- Keep filesystem/provider permission decisions in C++ controller APIs, not in
  large QML JavaScript blocks.
- If QML session state grows complex, keep it to UI/session state only and move
  policy back to C++.

## Goal

Add a deliberately limited internal drag-and-drop workflow for selected files
and folders:

- The user can select one or more items in one file panel.
- The user can drag that selection toward the opposite panel.
- When the pointer is released over the opposite panel, FM shows a small
  context menu with exactly:
  - `Copy N items`
  - `Move N items`
  - `Cancel operation`
- Copy and move target the opposite panel's current folder.
- Dropping onto an item, folder row, icon, empty area, header, or any other
  point inside the opposite panel must not change the destination. The
  destination is always the opposite panel's `currentPath`.
- The feature is disabled entirely when split view is not active.

This is not general drag and drop. It is a panel-to-panel command shortcut with
a drag gesture and explicit confirmation menu.

## Current Code Context

### Panels and split state

- `qml/components/FileWorkspace.qml`
  owns the two visible `FilePanel` instances:
  - `leftPanel` uses `workspaceController.leftPanel`.
  - `rightPanel` uses `workspaceController.rightPanel`.
  - `rightPanel` is visible only when `workspaceController.splitEnabled` is
    true.
- `WorkspaceController` exposes:
  - `splitEnabled`
  - `activePanel`
  - `leftPanel`
  - `rightPanel`
  - `operationQueue`

### Selection

- `FilePanelController::selectedPaths()` already exposes selected paths to QML.
- File delegates already route click, right-click, double-click, selection
  badges, and rubber-band behavior through `FilePanel`.
- Existing selection can come from:
  - single click;
  - selection badges;
  - keyboard/range selection;
  - rubber-band selection;
  - command/menu actions.
- Drag must use the existing selected set. It must not invent a separate
  selection model.

### Existing copy/move behavior

- `WorkspaceController::copyActiveSelectionToOpposite()` copies the active
  panel selection to the opposite panel.
- `WorkspaceController::moveActiveSelectionToOpposite()` moves the active panel
  selection to the opposite panel.
- `WorkspaceController::copyPathsToPanel(...)` already rejects:
  - empty sources;
  - unwritable destination;
  - all sources already being in the destination folder.
- Move currently performs source/destination capability checks inline and then
  calls `OperationQueue::moveTo(...)`.
- These methods are active-panel oriented. A drag/drop implementation should
  not depend on whichever panel is active after the mouse release.

### Existing capability checks

- `qml/components/filepanel/FilePanelActionPolicy.qml` already centralizes much
  of the QML-side decision logic:
  - `oppositePanel()`
  - `canCopySelectionToOtherPanel()`
  - `canMoveSelectionToOtherPanel()`
  - source provider checks;
  - destination provider checks;
  - operation busy checks;
  - virtual-root checks.
- `FilePanelSelectionActions.qml`, toolbar actions, and command registry use
  these checks for F5/Shift+F5 and selection actions.
- The new feature should reuse this logic or mirror it through a shared helper
  rather than creating separate permissive checks.

### Existing drop overlay

- `qml/components/filepanel/FilePanelDropOverlay.qml` currently has a `DropArea`
  for `text/uri-list`.
- It immediately calls:
  `root.workspaceController.operationQueue.copyTo(paths, root.currentPath)`.
- It treats the panel `currentPath` as destination, which matches the new
  feature's destination rule.
- It is not suitable as-is because:
  - it handles external drops, not internal panel selection drag;
  - it immediately copies without asking;
  - it has no source-panel/opposite-panel restriction;
  - it accepts only `drop.text`;
  - it does not distinguish dropping onto the same panel from the opposite
    panel.

## Non-Goals

- No drag into subfolders.
- No drag reorder.
- No dragging from panel to sidebar/tree/path bar/desktop/other apps.
- No receiving external file-manager drops as part of this feature.
- No cross-window drag support.
- No copy/move menu on the source panel.
- No auto-open folder on hover.
- No archive-internal drag behavior beyond what existing copy/move operations
  already support.
- No custom drop cursor semantics beyond clear allowed/disallowed feedback.
- No new global setting for this first pass.

## Core Behavioral Rules

### Eligibility

A drag session can start only when all are true:

- `workspaceController.splitEnabled` is true.
- The source panel has at least one selected path.
- The press starts on an item that is part of the current selection, or the
  implementation first selects the pressed item using the existing click rules
  and then starts from that resulting selection.
- The source panel is not in inline rename mode.
- The operation queue is not busy.
- The source panel is not a virtual root.
- The source selection passes `canCopySelection`.

Move can be offered only when all copy conditions are true plus:

- source can delete the selected items;
- source and destination are compatible for move;
- remote-provider limitations match existing Shift+F5 behavior;
- source and destination folders are not the same folder.

### Destination

The only valid drop destination is the opposite panel's current folder:

- left panel selection can drop only on the right panel;
- right panel selection can drop only on the left panel;
- if split view closes during a drag, cancel the drag session;
- if the opposite panel becomes a virtual root, archive read-only location,
  managed ISO mount, or otherwise unwritable during a drag, show disallowed
  feedback and do not show the menu.

The destination does not depend on pointer location inside the opposite panel.
Dropping over a folder row still targets the opposite panel `currentPath`, not
that folder.

### Menu

On valid release over the opposite panel:

- show a `ThemedContextMenu`;
- menu item count and labels are based on the drag session snapshot, not on
  current live selection if it changed later;
- labels:
  - one item: `Copy 1 item`, `Move 1 item`, `Cancel operation`;
  - many items: `Copy N items`, `Move N items`, `Cancel operation`;
- disabled states should explain impossible actions through existing status
  messages or disabled menu entries only if needed. Prefer not showing the menu
  if neither copy nor move is possible.

Recommended first-pass menu behavior:

- `Copy N items` is enabled when the snapshot can be copied to the opposite
  panel.
- `Move N items` is enabled when the snapshot can be moved to the opposite
  panel.
- `Cancel operation` is always enabled.
- If copy is valid but move is not, show move disabled instead of hiding it.
  This preserves the promised three-item menu while still making constraints
  visible.
- If copy is not valid, do not show the menu. Set a status message instead.

### Operation Execution

Copy:

- uses the drag snapshot paths;
- uses the destination panel current folder captured at drop/menu-open time;
- calls a WorkspaceController API that validates destination and starts
  `OperationQueue::copyTo(...)`.

Move:

- uses the same drag snapshot paths;
- uses the same destination folder captured at drop/menu-open time;
- validates source deletion and destination write capability;
- calls `OperationQueue::moveTo(...)`.

The operation must not read `workspaceController.activePanel` at trigger time.

## Proposed Architecture

### 1. Add an internal drag session object in QML

Add a lightweight coordinator under `FileWorkspace.qml` or a new component under
`qml/components/filepanel/`, for example:

- `FilePanelDragCoordinator.qml`

Responsibilities:

- Hold one active drag session:
  - `active`
  - `sourcePanelSide` (`"left"` or `"right"`, or `0/1`)
  - `sourceController`
  - `destinationController`
  - `paths`
  - `itemCount`
  - `canCopy`
  - `canMove`
  - `destinationPath`
  - pointer/menu coordinates
- Start only from a `FilePanel`.
- Cancel on:
  - Escape if easy to wire;
  - source path changes;
  - destination path changes before drop;
  - split disabled;
  - operation starts/busy;
  - inline rename starts;
  - drag leaves the application without drop.
- Expose helpers:
  - `canStartDrag(panel, index, path)`
  - `startDrag(panel, index, path, pressPoint)`
  - `updateDrag(globalPoint)`
  - `isOppositePanel(panel)`
  - `canDropOn(panel)`
  - `completeDrop(panel, localPoint)`
  - `cancelDrag(reason)`

Keep this QML-side object intentionally dumb about filesystem semantics. It
should ask existing action policy/controller APIs for capabilities and use C++
for final validation.

### 2. Add explicit WorkspaceController methods

Add Q_INVOKABLE methods that do not depend on active panel:

```cpp
Q_INVOKABLE QVariantMap oppositePanelDropCapabilities(
    int sourcePanel,
    const QStringList &sources,
    int destinationPanel) const;

Q_INVOKABLE bool copyDroppedSelectionToPanel(
    int sourcePanel,
    const QStringList &sources,
    int destinationPanel,
    const QString &destinationPath);

Q_INVOKABLE bool moveDroppedSelectionToPanel(
    int sourcePanel,
    const QStringList &sources,
    int destinationPanel,
    const QString &destinationPath);
```

Exact names can change, but the contract should stay explicit:

- source panel is explicit;
- destination panel is explicit;
- sources are explicit;
- destination path is explicit;
- active panel is irrelevant.

The implementation can use private helpers:

- `FilePanelController *panelBySide(int side)`
- `bool validateOppositePanelDrop(...)`
- `bool copyPathsToPanel(const QStringList &, FilePanelController *)`
- a new `movePathsToPanel(...)` equivalent to avoid duplicating move checks.

Validation should cover:

- split is enabled;
- source and destination panels exist;
- destination is exactly the opposite panel;
- sources are not empty;
- destination path equals destination panel's current path when operation is
  triggered, or reject if it changed after the menu opened;
- source/destination virtual root checks;
- source copy capability;
- destination create capability;
- operation queue is not busy;
- move-only:
  - source delete capability;
  - provider restriction parity with existing move-to-other-panel;
  - same-folder rejection.

Return values:

- Capability method returns a map:
  - `canCopy: bool`
  - `canMove: bool`
  - `reason: string`
  - `copyReason: string`
  - `moveReason: string`
  - `destinationPath: string`
- Execute methods return `true` if they queued an operation.
- On false, set an operation/status message so the UI has feedback.

### 3. Wire drag start in delegates through FilePanel

Avoid implementing drag logic separately in every delegate.

Recommended route:

- Add signals/functions on `FilePanel`:
  - `dragSelectionCandidatePressed(index, path, mouse)`
  - `dragSelectionCandidateMoved(index, path, mouse)`
  - `dragSelectionCandidateReleased(index, path, mouse)`
- Or, more simply, add one `SelectionDragHandler` component used by the common
  delegate root where possible.

Affected delegates:

- `FileDelegate.qml` for list mode.
- `FileTableDelegate.qml` / `FileTableAdaptiveDelegate.qml` for details mode.
- Grid delegate embedded in `FilePanel.qml`.
- `FileBriefDelegate.qml` / `FileBriefAdaptiveDelegate.qml` for brief mode.

Do not start with every view if that makes the first patch too large. A safe
phase split is:

1. Details/list/grid.
2. Brief.
3. Lightweight resize delegates remain non-draggable.

Start threshold:

- Use a small threshold similar to platform drag threshold.
- Do not begin drag on plain click.
- Do not interfere with:
  - double-click open;
  - right-click context menu;
  - selection badge clicks;
  - inline rename;
  - rubber-band selection;
  - scroll gestures.

Selection rule:

- If the pressed item is already selected, drag the whole selection.
- If the pressed item is not selected, first apply the existing click selection
  behavior, then drag that single item if the user crosses the drag threshold.
- Ctrl/Shift press behavior should keep matching existing click semantics; do
  not create a separate drag-only selection rule.

### 4. Add drop target behavior to FilePanel

Each `FilePanel` should know whether it is currently the valid opposite target.

Add to `FilePanel`:

- `property var dragCoordinator`
- derived state:
  - `dropTargetActive`
  - `dropTargetAllowed`
  - `dropTargetDeniedReason`

Use a full-panel pointer/drop receiver that does not care which child item is
under the mouse. The valid target is the visible opposite `FilePanel` surface as
a panel, not a child view item.

Important:

- Do not use folder delegates as drop targets.
- Do not set destination to `modelData.path`.
- Do not let a hovered folder row visually imply folder-as-destination.

Recommended visual:

- Reuse or extend `FilePanelDropOverlay.qml`.
- For allowed opposite drop:
  - accent-tinted outline/fill over the panel content area;
  - no central text label.
- For disallowed:
  - muted/warning outline;
  - no central text label.
- Disable the existing external drop overlay during internal drag if the two
  behaviors conflict.

### 5. Add the drop action menu

Create a small component, for example:

- `qml/components/filepanel/OppositePanelDropMenu.qml`

Use:

- `ThemedContextMenu`
- `ThemedMenuItem`
- `ThemedMenuSeparator` only if truly needed; first pass does not need it.

Menu structure:

```qml
ThemedContextMenu {
    ThemedMenuItem { text: copyLabel; enabled: canCopy; onTriggered: ... }
    ThemedMenuItem { text: moveLabel; enabled: canMove; onTriggered: ... }
    ThemedMenuItem { text: "Cancel operation"; onTriggered: close() }
}
```

Action handlers call explicit WorkspaceController methods with:

- source side;
- destination side;
- snapshot paths;
- destination path captured when menu opened.

Menu lifetime:

- Opening the menu should freeze the drag session snapshot.
- Closing without action cancels the session.
- Running copy/move clears the session after the controller accepts the action.
- If split is disabled while menu is open, close it.
- If destination path changes while menu is open, close it or let operation
  reject with a status message. Prefer closing to avoid stale destination.

### 6. Keep external drops separate

`FilePanelDropOverlay.qml` currently accepts `text/uri-list` and copies into the
panel current path.

For this feature:

- do not broaden it into internal selection drag without careful separation;
- either keep external drop overlay as-is and add a sibling internal overlay;
- or split it into:
  - `ExternalFileDropOverlay`
  - `OppositePanelDropOverlay`

The internal feature should not accidentally allow external drag into the
opposite-panel confirmation menu.

## Implementation Phases

### Phase 1: Controller API and validation

Files:

- `src/controllers/WorkspaceController.h`
- `src/controllers/WorkspaceController.cpp`

Tasks:

1. Add explicit panel-side helper.
2. Add capability query for panel-to-panel dropped sources.
3. Add explicit copy dropped sources method.
4. Add explicit move dropped sources method.
5. Refactor shared validation from current active-panel methods only where it
   reduces duplication without changing existing F5 behavior.

Verification:

- Existing F5/Shift+F5 still work.
- Same-folder copy/move still reports existing status.
- Split disabled rejects dropped operations.
- Destination virtual root/unwritable rejects dropped operations.

### Phase 2: QML drag session and target overlay

Files:

- `qml/components/FileWorkspace.qml`
- `qml/components/FilePanel.qml`
- `qml/components/filepanel/FilePanelDropOverlay.qml` or new overlay component
- new `qml/components/filepanel/FilePanelDragCoordinator.qml` if useful

Tasks:

1. Create internal drag session state.
2. Expose source/destination side to panels.
3. Show allowed overlay only on the opposite panel.
4. Show denied/no overlay on source panel.
5. Disable feature when `workspaceController.splitEnabled` is false.

Verification:

- No visual drag affordance in single-panel mode.
- Drag from left highlights only right.
- Drag from right highlights only left.
- Drag over source panel does not show menu.
- Drag over opposite panel shows target overlay.

### Phase 3: Delegate drag start

Files:

- `qml/components/FileDelegate.qml`
- `qml/components/FileTableDelegate.qml`
- `qml/components/FileBriefDelegate.qml`
- grid delegate block inside `qml/components/FilePanel.qml`
- adaptive delegates only if required to pass drag events through

Tasks:

1. Add press/move/release path to initiate internal drag.
2. Respect drag threshold.
3. Keep existing click/double-click/right-click behavior.
4. Keep selection badge behavior isolated.
5. Cancel on inline rename and rubber-band selection.

Verification:

- Single click still selects.
- Double-click still opens.
- Right-click still opens context menu.
- Rubber-band selection still works.
- Dragging a selected item drags the full selection.
- Dragging an unselected item uses that item after normal selection behavior.

### Phase 4: Drop menu

Files:

- new `qml/components/filepanel/OppositePanelDropMenu.qml`
- `qml/components/FileWorkspace.qml` or `FilePanel.qml`

Tasks:

1. Add menu with exactly three entries.
2. Use snapshot paths and destination path.
3. Wire copy/move to explicit WorkspaceController APIs.
4. Close/cancel correctly.

Verification:

- Drop over opposite panel shows menu.
- `Copy N items` copies to opposite current folder.
- `Move N items` moves to opposite current folder.
- `Cancel operation` does nothing.
- Dropping over a folder in the opposite panel still targets the panel folder,
  not the hovered folder.

### Phase 5: Polish and regression checks

Tasks:

1. Add concise status messages for rejected drops.
2. Audit cursor/overlay language.
3. Ensure no drag state remains after path changes, split toggles, or operation
   starts.
4. Add manual QA checklist entries.

Potential docs:

- Update `docs/qa-regression-suite.md` with panel-to-panel drag cases.
- Update `suggest/09-testing-and-verification.md` only if this becomes a
  general project rule.

## Manual QA Matrix

### Core

- Split disabled: drag gesture does not start or produces no opposite target.
- Split enabled: select one file in left, drag to right, release, menu appears.
- Select multiple files and folders, drag to opposite, menu count is correct.
- Click `Cancel operation`: no operation starts.
- Click copy: items appear in opposite current folder.
- Click move: items are removed from source and appear in opposite current
  folder.

### Destination rule

- Drop over empty area in opposite panel: destination is opposite current path.
- Drop over a file row in opposite panel: destination is still opposite current
  path.
- Drop over a folder row in opposite panel: destination is still opposite
  current path.
- Drop over panel header/pathbar/footer: destination is still opposite current
  path. The whole visible opposite panel is a panel-level target.

### Restrictions

- Drag inside the same/source panel: no menu.
- Drag from left to left: no menu.
- Drag from right to right: no menu.
- Destination read-only/archive/managed ISO: no copy/move operation starts.
- Operation queue busy: drag is disabled or drop is rejected.
- Both panels show the same folder: copy/move rejected as same destination.
- Source provider where move is unsupported: copy may be offered, move disabled.

### Interaction regressions

- Double-click open still works.
- Right-click context menu still works.
- Rubber-band selection still works.
- Selection badges still work.
- Inline rename still starts and edits normally.
- Scrolling does not accidentally start drag.
- Switching view mode clears any pending drag state.
- Toggling split during drag cancels.
- Navigating either panel during drag cancels or invalidates the menu.

## Risks

- QML `Drag`/`DropArea` can accidentally interact with external drag types. Keep
  internal session state separate from `text/uri-list`.
- Existing copy/move APIs rely on active panel. New drop APIs must be explicit.
- Delegate mouse handling is already dense. Implement drag threshold carefully
  to avoid breaking selection/opening.
- Lightweight delegates used during resize/scroll should probably not start
  drag in the first implementation.
- Provider/archive paths need parity with existing F5/Shift+F5 behavior. Do not
  make drag more permissive than keyboard/menu operations.
- If selection changes after drag starts, using live selection would be
  surprising. Always use the drag snapshot.

## Suggested First Implementation Scope

Implement the first pass narrowly:

1. Explicit C++ drop operation APIs.
2. Internal QML drag session.
3. Drag from normal delegates only, not lightweight resize delegates.
4. Drop only on opposite panel content area.
5. Three-item menu.
6. Reuse existing operation queue for copy/move.

Defer:

- custom drag preview image;
- advanced cursor semantics;
- auto-scroll while dragging near panel edges;
- external drag improvements;
- sidebar/tree/pathbar drop targets.
