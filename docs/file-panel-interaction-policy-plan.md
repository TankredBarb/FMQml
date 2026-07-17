# File Panel Focus, Selection, Reveal, and Preview Policy Plan

## Status

Draft implementation plan, 2026-07-15. This document defines intended behavior
and sequencing. It does not mark any phase as implemented.

## Goal

Make file-panel behavior predictable after navigation and mutations by giving
focus, current item, selection, reveal, and preview explicit and independent
policies.

The implementation must work for synchronous local model updates and for
asynchronous operations, provider refreshes, conflict resolution, partial
success, delayed filesystem watchers, and panels that the user has navigated or
interacted with while an operation was running.

## Non-goals

- Do not redesign file delegates, selection visuals, or the preview UI.
- Do not make every operation steal keyboard focus or activate its destination
  panel.
- Do not infer final copy names from source names after an operation has
  completed.
- Do not restore stale selection merely because a folder was revisited.
- Do not replace the existing navigation/history or operation queue in one
  change.
- Do not weaken preview release behavior to make transitions look smoother.
  Releasing file handles before rename, overwrite, move, delete, and unmount is
  a correctness requirement, especially on Windows.

## Terminology

These concepts must remain separate in code and tests:

- **Panel focus**: the panel/view receiving keyboard input.
- **Current item**: the single keyboard cursor/anchor in a panel. A current item
  does not imply selection.
- **Selection**: zero or more paths targeted by file operations.
- **Reveal**: scroll enough to make a path visible. Reveal does not imply panel
  activation, current item, or selection.
- **Preview target**: the item, selection summary, virtual root, or folder shown
  in the preview pane.
- **Attention request**: a semantic request to update current item, selection,
  reveal, and preview after navigation or an operation.
- **Interaction revision**: a per-panel counter incremented by newer user intent
  so stale asynchronous completions cannot move the cursor or selection.
- **Model convergence**: the point at which the model contains the final result
  paths, or has removed successfully deleted paths, after an operation.

## Core invariants

1. A panel may have a current item with an empty selection.
2. Changing current item programmatically must not accidentally select it.
3. Reveal must not activate a background panel.
4. A preview pane should not remain blank when the active panel has a valid
   current item or a meaningful folder/root fallback.
5. A preview may be intentionally released during a mutation. It must be
   restored only after the operation outcome and model state are known.
6. User interaction after an asynchronous operation starts wins over the
   operation's original post-action attention request.
7. Final destination paths come from the execution layer. They must not be
   reconstructed from source names because Keep Both, providers, archives, and
   case rules can change them.
8. Partial success is not success for every requested path. Focus and selection
   must reflect per-item outcomes.
9. History navigation restores a location anchor; it does not resurrect an old
   destructive-operation selection.
10. Timers may be watchdogs or model-convergence retries, but must not be the
    semantic source of operation completion or preview ownership.

## Current behavior and known gaps

The current implementation already has useful pieces:

- `FilePanel.qml` can set a view current index without selection.
- direct navigation initializes a current item, normally the first entry;
- history/up navigation can preserve scroll and target a child path;
- `PreviewCoordinator.previewTargetFor()` prefers multiple-selection summary,
  then `currentItemPath`, then a single selected item, then roots/current path;
- create/rename paths have a delayed reveal mechanism;
- preview is force-released for rename/delete-related flows;
- `operationFinishedDetailed` reports counts and failed source paths.

The missing contracts are the reason behavior is inconsistent:

- delete releases the preview but does not schedule an explicit survivor;
- preview suppression uses overlapping booleans plus a timeout, which is fragile
  when rename, delete, move, or another operation overlaps;
- copy/move completion does not expose exact per-source final paths;
- `WorkspaceController` often reconstructs destination paths from source names;
- background and active destination panels are not distinguished by a shared
  attention policy;
- model refresh and provider enumeration can finish after `operationFinished`;
- a late completion can currently race newer navigation or selection;
- selection, current item, and preview sometimes update through independent
  signal chains instead of one semantic result.

## Normative behavior matrix

### Navigation and view state

| Action | Current item | Selection | Reveal/scroll | Preview |
| --- | --- | --- | --- | --- |
| Open a new folder normally | First visible item | Clear | Top/first item | First item |
| Open an empty folder | None | Clear | Top | Folder summary/empty-folder state |
| Back/Forward | Stored path anchor if it still exists, otherwise nearest valid item | Clear | Restore stored scroll, then contain anchor | Restored current item |
| Go Up | Direct child folder that was left | Clear | Contain child | Child folder |
| Refresh | Preserve current path if it exists | Preserve surviving selected paths | Preserve scroll | Existing current target |
| Change sort order | Preserve by path | Preserve by path | Do not move unless current would be lost/offscreen by policy | Existing current target |
| Change view mode | Preserve by path | Preserve | Preserve equivalent anchor/offset | Unchanged |
| Apply filter | Preserve current if visible; otherwise first visible item | Remove hidden/non-visible paths from selection | Contain replacement current | Replacement current |
| Clear filter | Preserve current | Preserve | Do not jump without need | Existing current target |
| Activate the other panel | Restore that panel's current item | Preserve | No forced scroll | New active panel target |
| Load more provider items | Preserve current | Preserve | Preserve scroll | Existing current target |
| Navigation failure | Restore pre-navigation current/scroll | Preserve pre-navigation selection | Restore | Restore previous target |

Normal navigation and history navigation must carry an explicit reason. A
single `preserveScroll` boolean is not expressive enough to distinguish Direct,
OpenChild, Up, Back, Forward, Refresh, Restore, and Recovery behavior.

### Pointer and keyboard selection

| Action | Current item | Selection |
| --- | --- | --- |
| Plain click | Clicked item | Clicked item only |
| Plain keyboard move | New keyboard item | New item only |
| Ctrl+click | Clicked item | Toggle clicked item |
| Ctrl+keyboard move | New keyboard item | Preserve selection |
| Shift+click / Shift+keyboard move | Range endpoint | Range from selection anchor |
| Click empty space | Preserve current unless explicitly clearing the panel cursor | Clear |
| Rubber-band selection | First selected item is current anchor | Exact intersected set |

The selection anchor used for Shift ranges is separate from the current item
and must be reset only by actions that semantically start a new selection.

### Delete

Before the confirmation dialog or operation starts, capture a delete snapshot:

- panel and directory identity;
- ordered source paths and their model rows;
- current path;
- selection paths;
- first deleted row;
- next surviving candidate path;
- previous surviving candidate path;
- interaction revision.

Do not release preview merely to display the confirmation dialog. If the user
cancels, current item, selection, preview, and scroll must remain unchanged.

After confirmation and before mutation, release previews/materialized handles
for the paths that will actually be deleted. After completion and model
convergence:

- all requested paths deleted successfully: current becomes the first survivor
  at the old first-deleted row; if none exists, use the previous survivor;
- no survivor: current is empty and preview falls back to the folder state;
- successful delete does not automatically select the survivor;
- partial failure: keep failed surviving paths selected and make the first
  failed path current; successful paths disappear from selection;
- abort/cancel after partial work follows the same per-item outcome rule;
- complete failure restores the original current and surviving selection;
- reveal only when the chosen survivor is outside the viewport;
- preview restores to the chosen survivor or folder fallback, never to a
  deleted path.

If the user navigates, changes current item, or changes selection after delete
starts, do not apply the old survivor attention request. Model removal still
occurs, but newer user intent owns focus and preview.

### Copy, paste, duplicate, extract, and drop

For one successfully created top-level result in a visible destination panel:

- make the exact final result current;
- select only that result;
- reveal it with `Contain` behavior;
- preview it if the destination panel is active.

For multiple successfully created top-level results:

- use the first successful result in request order as current;
- select the complete successful result group;
- reveal the first result;
- show selection summary when the destination panel is active.

Skipped and failed items are not included in the result selection. Replaced
destinations count as successful results only when execution confirms that the
replacement committed.

Additional rules:

- Keep Both must use the actual generated final path.
- A provider-assigned or normalized name must use the provider's returned path.
- Extraction selects/reveals top-level extracted results, not every descendant.
- Copying a directory selects the directory root, not its entire tree.
- A background destination panel may update current/selection/reveal state, but
  must not become active and must not steal keyboard focus or preview.
- If the destination panel has navigated away, retain the outcome for status or
  a future optional notification; do not navigate it back automatically.
- If the user interacted with the destination panel after operation start,
  suppress automatic current/selection changes. A non-invasive reveal may only
  happen if policy explicitly allows it and the directory is still the same.

### Move

Move has two panel outcomes:

- destination follows the successful-result copy policy;
- source follows delete-survivor policy for successfully moved paths;
- failed source paths remain selected in the source panel;
- if source and destination are represented by the same panel/directory, apply
  one combined policy rather than two competing requests;
- only the active panel controls preview;
- cross-provider move must wait for both destination commit and source removal
  before reporting a path as moved.

### Create and rename

Create file/folder:

- new path becomes current and the only selection;
- reveal it;
- start inline rename only after the model exposes the path;
- keep the semantic target through sorting changes;
- after rename commit, remap current/selection/preview to the new path;
- after rename cancellation, keep the created item current if it still exists;
- if cancellation removes a temporary placeholder, apply survivor policy.

Rename existing item with F2:

1. Capture current/selection and the old path.
2. Release preview and any preview materialization for the old path before the
   filesystem/provider rename is allowed to mutate it.
3. Show inline editor without changing selection.
4. On cancel, restore preview of the old path after confirming it still exists.
5. On success, wait until the model exposes the new path, remap current and
   selection, then restore preview using the new path.
6. On failure, keep the old path current/selected and restore its preview.

F2 must never be optimized by leaving the preview loaded. A temporarily blank
or explicit `Preview paused while renaming` state is preferable to holding the
file open. The implementation must verify whether `QuickLookController` release
is synchronous; if not, mutation must wait for a release acknowledgement.

### External filesystem/provider changes

- If a non-current item disappears, remove it from selection and keep current.
- If current disappears externally, choose the nearest survivor using the last
  known row; do not select it automatically.
- If a selected item is renamed externally and identity is known, remap it;
  otherwise remove the stale selection.
- If a provider refresh temporarily returns an incomplete page, do not apply
  survivor policy until the load generation is final.
- Device removal overrides normal survivor behavior: release preview first,
  leave the removed root, then focus the recovery destination.

## Preview policy

### Target precedence

For the active panel:

1. More than one selected path: selection summary.
2. Valid current item: current item preview.
3. One selected path with no valid current item: selected item preview.
4. Device/favorites/selection virtual root: corresponding root preview.
5. Valid current folder: folder summary or empty-folder state.
6. Empty preview only when no meaningful target exists or the pane is closed.

### Token-based suppression

Replace the semantic use of `renameSuppressed`, `operationSuppressed`, and the
delete timeout with release tokens/leases. A token contains:

- unique token/operation ID;
- reason: Rename, Delete, MoveSource, ReplaceDestination, Unmount, or other;
- affected paths or volume root;
- requested restore policy;
- panel/directory identity and interaction revision;
- lifecycle state: Requested, Released, MutationRunning, ModelPending,
  Restored, Cancelled.

Multiple tokens may overlap. Preview restores only when no active token blocks
the selected target. A timer remains only as a watchdog that reports a leaked
token; it must not silently decide that an operation finished.

### Mutation release barrier

The following operations can require a preview release before mutation:

- rename/F2;
- delete;
- move source removal;
- overwrite/Replace conflict destination;
- archive replacement;
- administrator mutation;
- provider materialization cleanup;
- device unmount/eject.

For local operations, define whether release is synchronously complete after
`preview("")`. If QuickLook workers, mapped files, media decoders, PDF handles,
or temporary materialization can outlive that call, add an asynchronous
`releaseCompleted(token)` acknowledgement. The operation must not mutate the
affected file before acknowledgement or a visible, reported failure.

Copying to a new name does not require suppressing the source preview. Replacing
an existing destination does.

## Asynchronous operation contract

### Stable operation identity

Every queued operation needs a monotonically unique `operationId`. Start,
conflict, progress, detailed completion, preview release, model convergence,
and attention requests must carry this ID. Queue serialization alone is not
enough because model updates and QML timers can outlive queue completion.

### Per-item outcome

Extend the detailed operation result with top-level item outcomes:

```text
OperationItemOutcome
  sourcePath
  requestedDestinationPath
  finalPath
  disposition: Created | Replaced | Moved | Deleted | Skipped | Failed | Aborted
  error
```

The operation-level result also carries:

```text
operationId
type
requestedDestinationDirectory
itemOutcomes[]
aborted
error
```

Requirements:

- record `finalPath` at the point conflict/provider resolution commits it;
- preserve source request order;
- do not emit a successful result for a `.part` or staging path;
- report top-level directory results once, not every descendant;
- partial results remain available when later items fail or operation aborts;
- administrator and provider batches must use the same outcome format;
- history recording consumes confirmed outcomes where exact paths matter.

### Attention request

After interpreting an operation result, `WorkspaceController` routes semantic
attention requests to affected panels:

```text
PanelAttentionRequest
  requestId / operationId
  reason
  directoryPath
  primaryPath
  groupPaths[]
  selectionMode: Preserve | Clear | ReplaceWithGroup | KeepFailed
  revealMode: None | Contain | RestoreAnchor
  previewMode: SyncIfActive | FolderFallback | Preserve
  activatePanel: false by default
  expectedInteractionRevision
```

Ownership should be explicit:

- `OperationQueue`: exact execution outcomes only;
- `WorkspaceController`: maps outcomes to source/destination panels;
- `FilePanelController`: validates directory/path identity and survivor choice;
- `FilePanel.qml`: applies current index, selection, and view reveal after model
  convergence;
- `PreviewCoordinator`: owns release tokens and final preview synchronization.

QML must not listen to generic `operationFinished` and guess destination names.

### Interaction revision and stale completion

Increment a per-panel interaction revision for:

- pointer or keyboard current-item changes;
- selection changes initiated by the user;
- navigation;
- filter/sort/view changes that establish a new anchor;
- explicit scroll/reveal actions where automatic repositioning would be
  disruptive.

An async attention request applies only when:

- the panel still shows the target directory;
- its navigation generation matches;
- the expected interaction revision is still valid;
- no newer attention request superseded it;
- the model contains the primary/result paths or has conclusively removed the
  deleted paths.

Programmatic changes made while applying the request must not increment the
user interaction revision.

### Model convergence

Operation completion and model visibility are separate events. Use a bounded,
generation-aware convergence step:

1. Apply direct model mutation when the provider/local model supports it.
2. Otherwise request refresh with a known load generation.
3. Wait for the exact final paths or confirmed removals.
4. Apply attention once per request.
5. If convergence fails, leave user state untouched, restore a safe preview
   fallback, and log/report the missing result paths.

The current delayed reveal timer can be reused only after it accepts an
attention request ID, directory identity, exact paths, and staleness checks.

## Implementation phases

### Phase 0: characterization and regression harness

Tasks:

1. Add opt-in traces for current path, current item, selection, reveal request,
   preview target, preview release token, operation ID, and model generation.
2. Record current F2, delete, paste, move, Keep Both, provider copy, and panel
   navigation sequences.
3. Add pure policy tests for neighbor/survivor selection.
4. Add a small fake asynchronous model test fixture capable of delayed insert,
   delayed remove, reorder, partial result, and stale generation.

Exit criteria:

- traces can explain every state transition without relying on visual guesses;
- existing preview release before F2/delete is covered by a regression test;
- no behavior change yet.

### Phase 1: central current-item and preview fallback policy

Tasks:

1. Introduce explicit navigation reasons.
2. Centralize programmatic current-item changes without selection.
3. Make preview target fallback deterministic for current item/folder/root.
4. Preserve navigation/history/view-mode behavior with path-based anchors.

Exit criteria:

- entering a new non-empty folder focuses but does not select the first item;
- preview shows that first item;
- empty folder shows folder fallback;
- Back/Forward/Up behavior matches the matrix;
- no F2 or operation behavior changes in this phase.

### Phase 2: tokenized preview release

Tasks:

1. Replace overlapping suppression booleans with release tokens.
2. Preserve the existing F2 release sequence before changing rename focus code.
3. Add release acknowledgement if controller teardown is asynchronous.
4. Convert delete and volume removal to tokens.
5. Keep the existing timer only as a diagnostic watchdog.

Exit criteria:

- F2 never keeps the previewed file open;
- cancel/failure/success restore the correct old/new path;
- overlapping rename/delete/unmount cannot restore preview early;
- watchdog expiry produces a diagnostic instead of silently restoring.

### Phase 3: delete survivor policy

Tasks:

1. Capture delete snapshots after confirmation and before mutation.
2. Extend detailed delete outcomes per path.
3. Wait for model convergence.
4. Apply full-success, partial-failure, abort, and complete-failure policy.
5. Restore preview to survivor/folder only after release token completion.

Exit criteria:

- deleting first/middle/last/all items behaves deterministically;
- multi-delete and partial provider delete retain failed paths appropriately;
- newer user navigation/current changes are never overwritten;
- preview does not remain blank after completion.

### Phase 4: exact operation result paths

Tasks:

1. Add `operationId` and per-item outcomes to `OperationQueue`.
2. Record final paths through local, admin, archive, and provider execution.
3. Cover Replace, Skip, Keep Both, failure, abort, and batch paths.
4. Stop reconstructing successful destination paths in
   `WorkspaceController` where exact outcomes are available.

Exit criteria:

- every successful top-level copy/move/duplicate/extract result has an exact
  final path;
- partial results are preserved;
- no result references staging/`.part` paths;
- existing history and refresh behavior remains correct.

### Phase 5: copy/move/paste/drop attention

Tasks:

1. Route result groups to visible destination panels.
2. Select/reveal one or many results according to the matrix.
3. Apply source survivor policy for move.
4. Add interaction-revision staleness checks.
5. Keep background panels inactive and preview-neutral.
6. Reuse the same path for clipboard, panel-to-panel, external drop, providers,
   extraction, duplicate, and administrator copy.

Exit criteria:

- single and multi result operations reveal exact results;
- Keep Both reveals generated names;
- background destination never steals focus;
- user activity during a slow operation wins;
- active preview shows item or selection summary as specified.

### Phase 6: create/rename/external-change unification

Tasks:

1. Convert existing created-entry reveal to attention requests.
2. Remap rename old/new path identity through one policy.
3. Apply survivor behavior to external removals.
4. Handle provider refresh/load-more final generations.
5. Remove obsolete timers and duplicate QML branches only after all callers
   migrate.

Exit criteria:

- create, rename, refresh, external delete, and provider changes share the same
  current/selection/reveal primitives;
- inline rename focus remains stable in list, grid, and brief modes;
- no legacy reveal path competes with the coordinator.

## Verification matrix

Every relevant phase must cover:

### Views and panel state

- list, grid, and brief view;
- active and inactive destination panel;
- split view enabled/disabled;
- preview pane open/closed;
- current item selected and current item not selected;
- top, middle, bottom, and offscreen anchors;
- ascending/descending sort and active filter.

### Storage/path types

- local filesystem;
- slow local/removable filesystem;
- provider URI with delayed refresh;
- archive paths and extraction;
- administrator operations;
- case-only names where platform rules differ;
- device removal while preview is active.

### Operation outcomes

- one and many sources;
- success, complete failure, partial failure, and abort;
- Replace, Skip, Keep Both, and apply-to-all;
- same-name results and provider-normalized names;
- user navigates away during operation;
- user changes selection/current during operation;
- model notification arrives before completion, after completion, or twice.

### Required F2/preview regression checks

1. Preview a local image, video, PDF, audio file, and text file, then press F2.
2. Confirm the preview controller/materializer releases the old path before the
   rename mutation.
3. Cancel rename and verify preview returns to the old path.
4. Commit rename and verify preview returns only after the new path exists.
5. Force rename failure and verify old current/selection/preview are restored.
6. Repeat on Windows with a decoder/file type that previously held an open
   handle.
7. Start rename while another preview suppression token exists and verify no
   early restore.

### Required delete regression checks

- delete first, middle, last, only, and multiple adjacent/non-adjacent items;
- cancel confirmation without any state change;
- delete currently previewed and non-previewed items;
- partial provider deletion;
- delete while sorted/filtered;
- navigate or select another item while delete runs;
- verify preview release before mutation and survivor restore afterward.

### Required copy/move regression checks

- paste one/many into active panel;
- copy/move one/many into inactive opposite panel;
- Keep Both and Replace;
- conflict dialog delay followed by newer user interaction;
- provider upload/download and cross-provider transfer;
- external drop and archive extraction;
- destination refresh delayed beyond operation completion;
- exact result group selection and first-result reveal.

## Instrumentation

Add one opt-in trace category, for example `FM_PANEL_INTERACTION_TRACE=1`, with
compact events:

```text
intent navigation reason=OpenChild panel=left navGen=12
preview-release token=44 reason=Rename path=...
operation-finish id=81 type=Copy success=2 failed=1
model-converged id=81 panel=right paths=2 interactionRev=19
attention-suppressed id=81 reason=newer-user-interaction
attention-applied id=82 current=... selected=2 reveal=Contain
preview-restored token=44 target=...
```

Paths should use existing safe logging conventions for providers/secrets.

## Rollout and review discipline

- Land phases separately; do not combine the operation-result contract with a
  broad `FilePanel.qml` refactor.
- Keep compatibility signals until all consumers migrate.
- In every phase, compare current behavior before removing existing timers or
  suppression calls.
- Treat any regression where F2, delete, overwrite, move, or unmount holds a
  previewed file as a release blocker.
- Treat focus stealing in an inactive panel and stale async selection as release
  blockers.
- Do not declare a phase complete from build/tests alone; run the corresponding
  manual matrix on Linux and the file-handle-sensitive checks on Windows.

## Definition of done

This plan is complete only when:

- navigation reasons produce the documented current/selection behavior;
- delete always resolves to a deterministic survivor or folder fallback;
- copy/move/paste/drop reveal exact successful result paths;
- background panels never steal focus;
- async completion cannot override newer user intent;
- preview release is tokenized and verified before destructive mutation;
- F2 cancel/success/failure restore preview safely;
- partial operation results drive selection correctly;
- list/grid/brief and local/provider/manual regression matrices pass;
- obsolete reveal/suppression paths are removed after migration.
