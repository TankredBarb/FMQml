# Map deletion safety

A user reported losing a parent folder after intending to delete two nested
files selected with Ctrl. The incident's exact confirmation contents are unknown.
A matching dangerous selection sequence was reproduced without touching user data:
select the folder, Ctrl-click the first child, then Ctrl-click the second child.
Previously selectedPaths contained three objects, including the parent folder.
The real-controller regression failed before the fix on that exact count.

The selection policy now excludes ancestor/descendant overlap:

- Selecting a nested item drops selected ancestors, retaining unrelated items.
- Explicitly selecting a folder drops selected descendants; it does not select
  the folder merely because files inside it were selected.
- Shift ranges and bulk/restored Map selections also discard selected ancestors
  when descendants are present. This favors the narrower explicit targets.
- Left-clicking a nested folder's body outside its child tiles explicitly selects
  that folder, just like its header. The model still drops selected ancestors when
  a child is chosen, and selecting a folder drops its selected descendants.
  Right-clicking its body opens the empty-space menu without replacing the file
  selection. Dragging pans without selecting; the current Map root is not selected.

A second check rejects overlapping folder/descendant targets in the shared
workspace deletion policy. Both requestDelete and confirmDelete use that policy;
this also protects a stale or programmatically constructed confirmation request.
It blocks the operation rather than silently expanding or rewriting its targets.
The existing permanent-delete behavior is unchanged: Trash/Undo are separate work.

Validation uses temporary test data exclusively:

- The real model/controller/queue test checks folder → Ctrl-child → Ctrl-child,
  the exact request paths, rejection of a corrupted overlapping request, actual
  deletion of only the two files, survival of the parent and an unselected third
  file, and explicit folder selection remaining available.
- Map hit tests distinguish folder headers, child tiles and gaps.
- Pure selection-policy tests cover nested ancestors, similarly prefixed sibling
  names and SFTP URL paths. The helper uses sorted path keys without filesystem
  I/O, avoiding a quadratic ancestor scan.
- The focused native Wayland/GPU scenario passed all five checks:
  mapOnlySelectedFiles, mapGapPreservesSelection, mapOverlappingDeleteBlocked,
  mapParentAndUnselectedFileSurvive and mapExplicitFolderSelection.
  Its aggregate mapDeleteSafety result is true. It does not claim that unrelated
  placeholder/clipboard checks from the general GPU benchmark passed.
  The original mapGapPreservesSelection check is superseded by the
  button-specific body checks in the left-click regression follow-up below.
- Build succeeded; the initial three focused CTest targets passed in 48.64 s.
  Logs: /tmp/fmqml-map-delete-hit-before.log, /tmp/fmqml-map-delete-before.log,
  /tmp/fmqml-map-delete-focused.log and /tmp/fmqml-map-delete-wayland.log.

The design conclusion is that confirmation cannot compensate for an incorrect
selection. Destructive actions must consume only explicit, unambiguous targets;
a parent folder must not remain selected as a side effect of selecting its files.

The complete final CTest suite passed: 93/93. The final build succeeded and
`git diff --check` is clean. Added code lines contain no Cyrillic.
Full-suite log: /tmp/fmqml-map-delete-full.log.


## Empty-folder context menu follow-up

A loaded empty folder must use the same content-area hit policy as a nonempty
folder. Right-clicking its body opens the empty-space menu without selecting the
container; left-clicking explicitly selects it. Its header opens its item menu. This also applies
when the category filter hides every file. Small/unloaded folder tiles retain
ordinary whole-tile selection until their content area is displayed.

The empty-body hit regression failed before the fix. A dedicated native GPU
scenario (--map --map-empty-menu) checks the actual nested empty-space popup,
unchanged current path/selection, and the folder popup from the header.

Follow-up validation passed: 4/4 focused CTest targets in 22.88 seconds, plus
native Wayland/GPU nestedEmptyBodyMenu=true and nestedEmptyHeaderMenu=true.
Logs: /tmp/fmqml-map-empty-menu-before.log, /tmp/fmqml-map-empty-menu-tests.log
and /tmp/fmqml-map-empty-menu-wayland.log. Build and git diff --check passed.

## Nested context actions and creation follow-up

The popup-type check above did not exercise its actions. Creating a folder from
a nested empty body still used the panel root, and creation in Map entered the
row-based inline rename flow. That flow cannot resolve nested Map entries.

The empty-space menu now carries its clicked folder path through deferred popup
creation. Folder/file creation and paste use that destination without navigating
the panel. Terminal, favorites, properties, disk usage, custom actions, select-all
and refresh use the same context. Nested creation refreshes the affected Map
branch without inserting its entry into the root directory model. Elevated
creation/paste actions remain available only for the panel's current folder.

After revealing a created Map entry, the create session finishes and opens the
existing Map rename dialog. Canceling or confirming its default name must leave
the menu usable.

The native GPU scenario `--map --map-context-create` injects F7 through QtTest
and gives the test application focus explicitly when Wayland denies activation.
It checks creation, rename cancellation, nested menu creation, default-name
confirmation, absence of an extra sibling, unchanged panel directory, and menu
reopening. The controller/queue regression additionally checks nested file
creation and clipboard paste using temporary local files.

Build passed. All 93 CTest targets passed across the full run (92 passing) and
the corrected creation-UI rerun (1 passing). The latter now reopens its menu
after navigation instead of triggering actions from a closed, stale context.
Logs: `/tmp/fmqml-map-context-full.log`,
`/tmp/fmqml-map-context-create-regression.log`,
`/tmp/fmqml-map-context-controller.log`,
and `/tmp/fmqml-map-context-create-after.log`.

## Folder body left-click regression

The initial safety fix also suppressed intentional left-click selection of a
nested folder's body. That restriction was too broad. Only right-click uses the
body hit region as empty-space menu context; left-click now follows the existing
selection policy. Root selection, drag/pan guards and double-click navigation
policy are unchanged.

The native deletion-safety scenario failed before this correction on
mapFolderBodySelection and mapBodyReplacesChildSelection. It now checks body
selection and Ctrl toggling, selecting two children without retaining their
parent, right-click context without selection changes, replacing children with an
explicit body selection, drag safety, root protection, and actual deletion of
only the two selected temporary files. The empty-menu scenario additionally
checks left-click selection of an empty folder before testing both menu types.

Logs: `/tmp/fmqml-map-body-before.log`, `/tmp/fmqml-map-body-after.log`,
`/tmp/fmqml-map-body-empty.log`, `/tmp/fmqml-map-body-tests.log`.
Final build and git diff --check passed. The three focused CTest targets passed
in 48.82 s. Both native GPU scenarios passed: body selection/deletion safety and
F7 creation/empty-folder selection/context menus/default-name confirmation.
