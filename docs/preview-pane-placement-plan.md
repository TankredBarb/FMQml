# Preview Pane Placement Plan

## Goal

Allow the preview pane to occupy one of three horizontal positions without
changing what it previews or weakening file-panel interaction:

- immediately after the sidebar;
- between the left and right file panels;
- at the far right, which is the current layout.

The implementation must preserve file-panel proportions while the preview is
shown, hidden, moved, or resized. Repeated `Ctrl+P` presses must remain smooth.
When the preview is between two file panels, internal opposite-panel drag/drop
must clearly communicate that the preview is a transit area rather than a drop
target.

## Decision Summary

- Persist one preferred placement using the stable values
  `after-sidebar`, `between-panels`, and `right`.
- Keep `right` as the default so existing installations do not change layout.
- When `between-panels` is selected but split view is disabled, use `right` as
  the effective placement without discarding the preference. Re-enabling split
  view restores the preview to the middle automatically.
- Keep exactly one live `PreviewPane` instance. Move that instance between
  lightweight host items; do not create three independent preview loaders.
- Keep one persisted preview width across all placements.
- Persist the file-panel split as a ratio. Do not use an opaque absolute
  `SplitView.saveState()` value as the long-term source of truth.
- Treat the middle preview as a non-droppable transfer corridor during the
  experimental internal panel-to-panel drag gesture.
- Continue to allow a drop only over the actual opposite file panel. Releasing
  over the preview cancels the gesture and never starts a file operation.
- Keep incoming external OS drag/drop separate. The preview is not an external
  drop target in any placement.
- Expose directional placement actions beside the `Details` controls inside
  the preview pane. Do not duplicate placement controls in Workspace settings.

## User-Visible Layout Contract

### Single-panel mode

| Preferred placement | Effective order |
| --- | --- |
| `after-sidebar` | Sidebar / Preview / File panel |
| `right` | Sidebar / File panel / Preview |
| `between-panels` | Sidebar / File panel / Preview |

`between-panels` falls back to `right` because there is no second file panel to
define a middle position. The stored preference remains `between-panels`.

### Two-panel mode

| Preferred placement | Effective order |
| --- | --- |
| `after-sidebar` | Sidebar / Preview / Left panel / Right panel |
| `between-panels` | Sidebar / Left panel / Preview / Right panel |
| `right` | Sidebar / Left panel / Right panel / Preview |

Placement does not change preview ownership. The preview continues to follow
the active file panel. A middle preview is not permanently associated with the
left or right panel merely because it is adjacent to both.

## Current State

### Layout

- `qml/App.qml` owns the outer horizontal `mainSplitView` with Sidebar,
  `leadingPreviewHost`, `FileWorkspace`, and `trailingPreviewHost`.
- `qml/components/FileWorkspace.qml` owns the inner horizontal `SplitView` with
  the two `FilePanel` instances and `middlePreviewHost` between them.
- One live `PreviewPane` wrapper is reparented between the three lightweight
  hosts according to the effective placement.
- The pane width is represented by `previewPaneStoredWidth` and
  `previewPanePreferredWidth`.
- `FileWorkspace.qml` tracks `splitRatio`, excludes the active middle preview
  and split handles from the panel-only width, and reapplies the ratio after
  workspace or preview width changes.
- Outer placement visibility changes use immediate geometry. The middle
  placement retains the lightweight resize path used during its transition.

### Persistence

- `WorkspaceStateCoordinator.qml` saves preview visibility, width, preferred
  placement, and the normalized file-panel split ratio.
- `AppSettingsController` validates the three placement values and clamps the
  ratio before saving, restoring, exporting, or importing workspace state.
- The opaque `fileWorkspaceSplitState` remains as a legacy migration fallback
  for settings that do not yet contain `filePanelSplitRatio`.

### Internal panel-to-panel drag/drop

- The internal workflow is guarded by `useLimitedDragNDrop` and remains off by
  default.
- `FilePanelDragCoordinator.qml` stores the source side, destination side,
  selected-path snapshot, capabilities, destination path, and pointer position.
- Panel side remains logical, not visual: `0` is left and `1` is right.
- `FilePanel.finishSelectionDrag()` maps the pointer into the opposite panel and
  opens `OppositePanelDropMenu` only when the release is inside that panel.
- `FileWorkspace.updatePanelDragCursor()` also tests the real destination-panel
  bounds. A middle preview will therefore already be rejected by policy.
- `FilePanelDragPreview.qml` supplies the floating selected-items affordance.
- `FilePanelInternalDropOverlay.qml` marks the actual opposite panel as the
  allowed or denied destination while an internal drag is active.
- `PreviewTransferBridgeOverlay.qml` marks a middle preview as a non-droppable
  transfer corridor and points toward the logical destination panel.

### External drag/drop

- `FilePanelDropOverlay.qml` is a distinct incoming `file://` copy path.
- Its `DropArea` is scoped to an actual file panel.
- It must remain available independently of `useLimitedDragNDrop` and must not
  be extended onto the preview pane as part of this feature.

## Non-Goals

- Do not make the preview a third file panel.
- Do not make preview content depend on physical proximity to a panel.
- Do not add preview tabs, multiple simultaneous previews, or pinned previews.
- Do not change copy/move capability policy or operation-queue execution.
- Do not make the middle preview a proxy drop target.
- Do not add dropping onto a child folder or previewed folder.
- Do not combine internal panel drag with native outgoing OS drag.
- Do not auto-change placement based on window width.
- Do not create separate width settings for each placement in the first pass.
- Do not animate the preview across the entire window. Collapse/reparent/expand
  is sufficient and avoids a fragile cross-parent position animation.

## Architecture

### 1. Preferred and effective placement

Add one preferred string to workspace state:

```text
previewPanePlacement = after-sidebar | between-panels | right
```

Expose two QML concepts in `App.qml`:

- `previewPanePlacement`: persisted user preference;
- `effectivePreviewPanePlacement`: resolved from the preference and current
  split state.

Resolution rule:

```text
preferred == between-panels && !splitEnabled -> right
otherwise                                      preferred
```

Do not rewrite the preference when split view changes. This prevents a
temporary switch to single-panel mode from silently losing the user's middle
layout choice.

All placement comparisons should use helper functions or readonly booleans.
Avoid scattering raw string comparisons through host items and preview controls.

### 2. Three lightweight host slots, one live preview

Create three host slots:

1. `leadingPreviewHost` in the outer split, directly after Sidebar;
2. `middlePreviewHost` in `FileWorkspace`, between left and right panels;
3. `trailingPreviewHost` in the outer split, after `FileWorkspace`.

The outer child order becomes:

```text
Sidebar / leadingPreviewHost / FileWorkspace / trailingPreviewHost
```

The inner child order becomes:

```text
LeftPanel / middlePreviewHost / RightPanel
```

`FileWorkspace` should expose the middle host through a readonly alias or a
narrow function. `App.qml` then selects the active host from the effective
placement.

Keep one wrapper containing one `PreviewPane` loader and reparent that wrapper
to the active host. This preserves:

- loaded preview renderer state;
- audio/video playback objects;
- scroll and zoom state where the renderer owns it;
- metadata-demand registration;
- the existing `previewPaneLoaded` lifetime optimization.

Do not create one `PreviewPane` per host with three mutually exclusive
Loaders. Even if only one Loader is active, changing position would destroy and
recreate media/rendering state and would make future transition bugs harder to
reason about.

### 3. Host sizing contract

Every host is a direct child of its owning `SplitView`. Exactly one host may be
active at a time.

An active host uses:

- preferred width: `previewPanePreferredWidth`;
- minimum width: the same transition-aware `0`/`280` policy as the current
  pane;
- maximum width: the existing practical upper bound, currently represented in
  settings as `1200`;
- fill width: `false`.

An inactive host uses:

- preferred, minimum, and maximum width: `0`;
- `visible: false` after its collapse is complete;
- no enabled resize handle.

Verify Qt's behavior for invisible SplitView children. There must be no ghost
four-pixel gap or draggable handle beside an inactive host. If the standard
handle is still instantiated, make the shared handle delegate derive its
extent and enabled state from the visibility of both adjacent layout items.
Do not accept permanent empty separators as a cosmetic compromise.

### 4. Safe position relocation

Visibility toggling and position relocation are separate transitions:

- `Ctrl+P` only opens or closes the pane in its current effective host.
- Changing placement keeps the visibility preference unchanged.

When the pane is hidden, relocation can reparent immediately.

When the pane is visible:

1. stop any visibility transition timer;
2. mark relocation active so intermediate widths are not persisted;
3. collapse the old host to zero;
4. after collapse, reparent the single preview wrapper;
5. activate the new host and expand it to the stored width;
6. clear relocation state only when the new host reaches its target.

Use a small transition generation counter. Each placement or visibility
request increments it, and delayed callbacks exit when their captured
generation is stale. This prevents rapid setting changes or `Ctrl+P` presses
from completing an obsolete transition.

Do not restore `SplitView.saveState()` at the end of relocation. File panels
must be governed by their ratio and the current available width.

### 5. File-panel ratio and available width

The file-panel ratio is always:

```text
leftPanel.width / (leftPanel.width + rightPanel.width)
```

Preview width and split handles are excluded.

For leading and trailing preview placements, the outer split changes the total
width of `FileWorkspace`; the current width-change capture/apply path can
preserve the file-panel ratio.

For middle placement, `FileWorkspace` itself does not change width. The inner
preview host takes width away from the two panels. Therefore add an explicit
middle-host width hook:

1. synchronously capture the current file-panel ratio before the inner split
   relayout settles;
2. apply that ratio on the next event-loop turn using the new panel-only
   available width.

Generalize the current hard-coded `splitView.width - 4` calculation. The
available width must account for:

- middle preview host width;
- the handle between the two file panels;
- the additional active handle or handles around the middle preview.

Prefer named handle extents and a helper such as `filePanelsAvailableWidth()`.
Do not duplicate arithmetic in show, hide, restore, and resize paths.

Continue to clamp each visible file panel to its existing `280` minimum. If the
window cannot satisfy all minimums, preserve the existing SplitView constraint
behavior; do not silently move or hide the preview.

### 6. Persist the ratio explicitly

Add a normalized workspace value:

```text
filePanelSplitRatio: real, default 0.5, accepted range 0.1...0.9
```

The runtime minimum-width clamp remains authoritative, so a stored extreme
ratio cannot make a panel narrower than `280`.

Migration rule:

- if `filePanelSplitRatio` exists and is valid, restore it directly;
- otherwise restore the legacy `fileWorkspaceSplitState` once, capture the
  resulting ratio, and save the explicit ratio on the next full layout save;
- keep reading the legacy field for compatibility during the migration period;
- stop writing the legacy opaque state after the explicit ratio is proven,
  unless another SplitView property is found to depend on it.

Update settings export/import so the ratio and placement remain ordinary JSON
values. The old byte-array encoder may remain only for importing/exporting
legacy state.

### 7. Preview width ownership

Keep one `previewPaneStoredWidth` for all positions.

Only the active host may update it, and only when:

- workspace restore is not active;
- visibility animation is not active;
- relocation is not active;
- the host width is at least `280`;
- the width change is stable or comes from the active preview resize handle.

Opening, closing, moving, maximizing, restoring, or changing the file-panel
ratio must not overwrite the stored preview width with an intermediate value.

## Internal Drag/Drop With a Middle Preview

### Interaction contract

The backend target remains the logical opposite panel. Placement changes only
the distance and visual route to that target.

During an internal drag:

- the source panel is not a valid target;
- the middle preview is not a valid target;
- only the actual opposite file panel is valid;
- releasing over the preview cancels the drag;
- Copy/Move/Cancel appears only after release over the actual target panel;
- destination remains the target panel's `currentPath` snapshot.

This keeps current controller validation and avoids surprising operations from
an informational pane.

### Visual scheme: transfer corridor

Add a small `PreviewTransferBridgeOverlay.qml`, created only when all of these
are true:

- `useLimitedDragNDrop` is enabled;
- an internal drag snapshot is active;
- preview is visible;
- effective placement is `between-panels`.

The overlay sits above preview content but below the floating drag-item preview
and is input-transparent.

Allowed destination treatment:

- apply a quiet veil over preview content so it temporarily reads as transit,
  not inspection UI;
- show a horizontal rail with two or three chevrons pointing from the source
  side toward the destination side;
- show compact copy such as `Continue to right panel` or
  `Continue to left panel`;
- use the normal accent family, but less strongly than the actual destination
  highlight;
- keep the existing floating icon stack following the pointer.

Denied destination treatment:

- keep the directional rail, because it still explains the intended route;
- use danger/disabled treatment;
- show a concise capability reason only if it fits without covering essential
  preview geometry;
- the actual destination panel must also use denied treatment.

The actual destination panel should receive an explicit full-panel tint and
outline while the drag is active. Since the standalone overlay described by
the older plan is absent from the live tree, add or restore one narrow
component driven only by:

- `isOppositePanel(panelSide)`;
- `canDropOn(panelSide)`;
- the coordinator's denied reason.

The middle overlay says where to continue; the destination overlay says where
release is accepted. They must not use identical intensity.

### Cursor rules

- Over the destination file panel: normal allowed drag cursor.
- Over source panel, preview, sidebar, or outside the workspace: forbidden.
- Do not use an allowed cursor over the transfer corridor merely because it is
  on the route. Release there is invalid, so the cursor must remain truthful.

The corridor label and chevrons compensate for the forbidden cursor while
crossing the preview and prevent the preview from looking like a dead barrier.

### Geometry and overlay ownership

Keep coordinator coordinates in `FileWorkspace` space for the middle layout:
the inner preview host is inside that same workspace, so the floating drag
preview can cross it without a coordinate-system change.

The existing target checks already map the pointer into the real destination
panel and therefore tolerate a preview gap. Retain those geometry-based checks;
do not infer the target from pointer direction or x-coordinate ranges.

On preview placement change, preview visibility change, or split-mode change
while an internal drag is active, cancel the drag session with a concise reason.
Do not move layout items under an active grabbed drag and then try to continue
the stale gesture.

### External drag/drop

No preview bridge is shown for native external drag/drop.

- Only file-panel `DropArea`s accept incoming URLs.
- A middle preview remains a non-target gap.
- Allowed/denied feedback remains owned by the panel under the native drag.
- Do not suppress external panel drops merely because the preview is central.

## Placement Controls

The preview `Details` header owns two directional actions:

- at `after-sidebar`, show only the right action;
- at `between-panels`, show both left and right actions;
- at `right`, show only the left action.

Each action moves by one available position. Navigation does not wrap from the
right edge to the left edge or vice versa. In single-panel mode, the middle
position is unavailable, so the actions move directly between `after-sidebar`
and `right`.

The actions use left/right arrow icons, expose matching tooltips and accessible
names, and call the same placement setter used by persistence restore.

Do not add a new global shortcut. `Ctrl+P` remains show/hide only.

## Implementation Phases

### Phase 0: Baseline and pending geometry fixes

1. Keep the current maximize/restore ratio fix and rapid `Ctrl+P` animation fix
   as the baseline.
2. Manually confirm those two routes before adding host relocation.
3. Record current widths and ratios for single-panel and split-panel layouts
   with preview hidden and right-positioned.

Verification:

- maximize/restore preserves file-panel ratio;
- rapid `Ctrl+P` does not skew panels or store a partial preview width;
- `cmake --build build -j 12` passes.

### Phase 1: State contract and persistence

1. Add placement and explicit ratio defaults/sanitization to
   `AppSettingsController` workspace state.
2. Include them in save, reset, export, and import paths.
3. Add focused cases to `AppSettingsControllerTest.cpp` for defaults, valid
   round-trip, invalid placement fallback, and ratio clamping.
4. Update `WorkspaceStateCoordinator.qml` to read/write the new fields without
   changing visual topology yet.

Verification:

- old settings with no new fields open on the right at a sane ratio;
- new settings round-trip through JSON;
- malformed values cannot break layout;
- the existing settings controller test passes.

### Phase 2: Host-slot scaffold with current-right behavior

1. Add leading, middle, and trailing host slots.
2. Add the single reparentable preview wrapper.
3. Keep effective placement forced to `right` during this phase.
4. Verify inactive hosts create no gaps or handles.
5. Preserve the current loader lifetime and preview synchronization.

Verification:

- pixel-level layout remains equivalent to the current right-side layout;
- preview content does not reload merely because the host scaffold exists;
- audio/video state behaves as before;
- no extra separator is visible or draggable.

### Phase 3: Leading and trailing placement

1. Enable `after-sidebar` and `right` selection.
2. Implement hidden-pane immediate relocation and visible-pane
   collapse/reparent/expand.
3. Keep one width across both positions.
4. Make rapid placement changes generation-safe.

Verification:

- both single and split layouts match the layout table;
- repeated placement changes do not create multiple preview instances;
- `Ctrl+P` during or immediately after relocation reaches the latest requested
  state without a delayed rebound;
- file-panel ratio remains stable.

### Phase 4: Middle placement and ratio integration

1. Enable `middlePreviewHost` when split view is active.
2. Generalize file-panel available-width calculations.
3. Capture/apply ratio around every middle-host width change.
4. Implement the single-panel fallback without changing preference.
5. Verify operations drawer and other workspace overlays remain correctly
   anchored.

Verification:

- left/right panels retain their ratio while preview opens, closes, resizes, or
  moves into/out of the middle;
- both file panels respect `280` minimum width;
- switching split off falls back to right; switching it on returns to middle;
- maximize/restore remains correct in all placements.

### Phase 5: Middle drag/drop visuals

1. Add the input-transparent transfer corridor overlay.
2. Add explicit destination-panel allowed/denied feedback if the live UI still
   lacks it.
3. Drive direction from `sourcePanelSide` and `destinationPanelSide`.
4. Preserve real-panel hit testing and menu execution.
5. Cancel active internal drag on layout relocation.

Verification:

- left-to-right and right-to-left gestures point in the correct direction;
- preview never accepts release;
- target panel remains the only place that opens Copy/Move/Cancel;
- forbidden cursor remains over preview and allowed cursor appears only over
  the valid panel;
- default-off mode creates no new drag overlay objects;
- external incoming drops remain panel-scoped and functional.

### Phase 6: In-pane placement actions

1. Add left/right placement actions beside the `Details` controls.
2. Show only directions that lead to another available position.
3. Wire both actions through the runtime placement setter.
4. Add accessible names and tooltips.

Verification:

- navigation never wraps across the workspace;
- the selected placement persists across restart and export/import;
- `Ctrl+P` semantics remain unchanged;
- keyboard focus order in the `Details` header remains sensible.

### Phase 7: Cleanup and documentation

1. Remove legacy opaque split-state writes after migration coverage is proven.
2. Remove transitional helpers made unused by the new host controller.
3. Update workspace and drag/drop QA documentation.
4. Keep unrelated preview renderers and provider code untouched.

Verification:

- `git diff --check` passes;
- `cmake --build build -j 12` passes;
- focused settings test passes;
- relevant existing core tests pass;
- manual matrix below is complete.

## Manual QA Matrix

Run with both light and dark themes where visual treatment differs.

### Layout and persistence

- single panel: after-sidebar, right, middle-preference fallback;
- split panels: all three placements;
- preview initially hidden and initially visible;
- restart in every placement;
- export settings, reset, import, and restart;
- toggle split while preferred placement is middle;
- resize preview in every effective placement and confirm one stored width;
- resize the main window slowly and rapidly;
- maximize and restore in every placement;
- move the file-panel divider before and after preview operations.

### Transition stress

- press `Ctrl+P` once in every placement;
- press `Ctrl+P` repeatedly faster than the 120 ms width animation;
- change placement during opening and during closing;
- change placement repeatedly before the prior relocation completes;
- toggle split during preview relocation;
- close the app during a transition and verify next startup state is valid.

### Preview content lifetime

- image zoom/pan;
- long text preview scroll;
- PDF preview;
- audio playback;
- video playback;
- folder and provider preview;
- selection preview;
- active-panel switch with preview in the middle.

The placement change should not create duplicate playback, duplicate metadata
requests, or a stale preview tied to the formerly adjacent panel.

### Internal drag/drop, experimental setting on

For grid, brief, details, and list-like normal views:

- left to right with preview in middle;
- right to left with preview in middle;
- release over source panel;
- release over preview corridor;
- release over valid destination panel;
- release outside the workspace;
- allowed copy only, allowed move only, both allowed, and denied capability;
- multi-selection floating preview;
- cancel menu;
- active panel/path changes while menu is open;
- toggle preview or placement during active drag;
- toggle split during active drag.

Repeat a smaller left-to-right/right-to-left set for after-sidebar and right
placements to confirm the corridor appears only in the middle.

### Drag/drop, experimental setting off

- no ready-to-drag cursor or internal coordinator activity;
- rubber-band selection remains unchanged;
- normal clicks and multi-selection remain unchanged;
- incoming external local-file drop works on both panels;
- dropping external data over preview is rejected without affecting a panel.

## Risks and Mitigations

### SplitView attached-property mutation during reparenting

Risk: `SplitView` may retain or overwrite attached preferred/minimum values when
a host changes activity.

Mitigation: reparent only the content wrapper, never a direct SplitView child.
Hosts remain owned by their original SplitViews and have explicit active and
inactive sizing contracts.

### Duplicate or reset preview renderer state

Risk: multiple Loaders or Loader recreation can reset media and metadata state.

Mitigation: one live wrapper and one Loader, reparented between stable hosts.

### Ratio feedback loop

Risk: preview animation changes panel widths, panel changes update ratio, and a
new ratio is captured from already-distorted geometry.

Mitigation: synchronously capture before layout settles, apply on the next
turn, and suppress ratio capture while explicitly applying/restoring it. Keep
one helper for panel-only available width.

### Rapid transition completion out of order

Risk: an old timer reparents or expands a host after a newer request.

Mitigation: generation-check every delayed transition step and make the latest
request authoritative.

### Middle preview looks like a valid drop target

Risk: the user releases on the prominent center pane expecting a transfer.

Mitigation: directional `Continue to ... panel` treatment, truthful forbidden
cursor, stronger destination-panel highlight, and no preview DropArea.

### Experimental drag handling leaks into default mode

Risk: new overlays or connections affect baseline selection while the feature
is disabled.

Mitigation: create corridor and internal target visuals through Loaders guarded
by `useLimitedDragNDrop`, matching the existing default-off requirement.

## Completion Criteria

The feature is complete only when:

- all three placements work in split mode and the two meaningful placements
  work in single-panel mode;
- placement, visibility, width, and file-panel ratio survive restart;
- preview content remains a single live instance across relocation;
- rapid `Ctrl+P` and rapid placement changes do not rebound or skew geometry;
- maximize/restore preserves the file-panel ratio in every placement;
- the middle preview has clear directional drag feedback but never accepts a
  drop;
- internal drag execution still targets only the opposite panel snapshot;
- external panel drops and default-off selection behavior are unchanged;
- focused tests, the full build with `-j 12`, and the manual QA matrix pass.
