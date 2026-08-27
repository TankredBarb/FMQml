# Global File Search Improvement Plan

## Goal

Turn the existing recursive file-search dialog into a clear, efficient search
workspace for the active local filesystem scope. Improve result quality,
filters, navigation, and discoverability without introducing a persistent
filesystem index in the first iteration.

The current asynchronous scanner, progressive result publication,
cancellation, generation filtering, skipped-path reporting, and Search Results
return action remain the foundation of the feature.

## Success Criteria

- The user can clearly choose between name search, content search, and combined
  name plus content search.
- The current search scope is visible, readable for long paths, and can be
  changed deliberately.
- Hidden-file behavior is visible and controllable instead of being inherited
  silently from the active panel.
- The best matches appear first after scanning, with additional explicit sort
  modes for name, path, size, and modification time.
- Every result has discoverable Open, Open containing folder, and Copy path
  actions.
- Long paths and content excerpts remain readable within the standard dialog
  sizing used by the application.
- Progressive results, cancellation, keyboard navigation, and return-to-results
  behavior continue to work during every search mode.
- Name-only search remains fast and does not read file contents.

## Current State

- `FileSearchDialog.qml` opens for the active panel path and uses a fixed
  maximum size of 820 by 600 px.
- Search starts after a 280 ms debounce and replaces the previous scan.
- Results are appended progressively in traversal order and sorted once the
  scan completes.
- The dialog exposes Name / Contents / Name + contents, Contains / Exact /
  Wildcard, case, hidden-file, and file/folder controls.
- Results support Relevance, Name, Path, Size, and Modified ordering.
- Kind, extension, and modified-date preset filters are available in a compact
  Filters popup.
- Right-clicking a result opens explicit Open, Open containing folder, and Copy
  path actions.
- `FileSearchController::revealPath()` exists but is not exposed by result UI.
- Content search reads supported text files up to 10 MiB and returns at most
  three matches per file.
- Local folders are supported. Archives, providers, Favorites, and Devices
  virtual paths are intentionally unsupported.

Phases 1, 2, and 3 are implemented.

## Product Decisions

### Search scope

Keep the initial feature recursive and local-filesystem based. "Global search"
does not initially imply a background index of every mounted filesystem.

Expose an explicit `Search in` control with these useful scopes:

- Current folder;
- Left panel folder;
- Right panel folder;
- a manually selected local folder.

The manual folder selector also permits choosing the filesystem root when an
explicit slow root search is needed. Do not silently scan every mounted disk.

Long scope paths use breadcrumbs or middle elision and provide the complete
path in a bounded tooltip.

### Search target

Replace the ambiguous Contents checkbox with an explicit three-way mode:

- `Name`;
- `Contents`;
- `Name + contents`.

Combined mode emits a name result when the filename matches and content
results when readable contents match. The model must prevent accidental
indistinguishable duplicates while preserving separate line matches.

### Filters

Keep the common controls visible:

- search target;
- files and folders / files only;
- match case;
- hidden files.

Move less common filters into one `Filters` surface:

- kind: all, folders, files, images, video, audio, documents, archives;
- extension;
- minimum and maximum size;
- modified date: today, last week, last month, or a custom range;
- match rule: contains, exact, wildcard.

The first implementation does not need every advanced filter at once. The
filter surface should be introduced only when at least kind, extension, and
modified date are functional.

### Result ordering

Provide these sort modes:

- Relevance;
- Name;
- Path;
- Size;
- Modified.

Relevance uses a small deterministic score, not fuzzy-search infrastructure:

1. exact filename match;
2. filename begins with the query;
3. filename contains the query;
4. content match.

Use stable secondary ordering by name and path. While scanning, results may be
published progressively without continuously reordering the list under the
pointer. Apply the selected final ordering when the scan completes, or update
in controlled batches while preserving the current item and scroll position.

### Result interaction

- Single click selects a result.
- Double click and Enter open it through the active panel.
- Right click opens a context menu; it must not perform an invisible immediate
  action.
- Context actions are Open, Open containing folder, and Copy path.
- Name matches are visually highlighted in the filename.
- Full paths are available in bounded tooltips.
- Content results show line number, one readable excerpt, and the highlighted
  match.

`Open containing folder` should navigate the active FM panel and select the
file when possible. The existing external `revealPath()` behavior is not a
substitute for this action and should either be renamed for clarity or left for
a separately labelled system-file-manager action.

### Dialog layout

Keep the standard application dialog sizing and safe margins on small windows.
Do not introduce a one-off resize interaction for this dialog; reconsider
resizable dialogs only as an application-wide pattern.

Suggested structure:

1. query field;
2. `Search in` scope row;
3. compact mode and common-filter row;
4. result count, sort control, progress, and Stop action;
5. result list;
6. a quiet footer for detailed scan coverage and skipped paths.

The skipped-path popup remains available, but zero skipped paths should not
reserve permanent toolbar width.

## Architecture

### Scanner request

Replace the growing positional argument list with one internal C++ request
value once the new search mode and filters are added. Suggested fields:

- root path;
- query;
- target mode;
- name match rule;
- case sensitivity;
- include hidden;
- include folders;
- kind and extension filters;
- size bounds;
- modification-time bounds;
- generation.

This request is an internal implementation detail, not a plugin API.

Filter cheap metadata before reading file contents. Content mode must continue
to reject oversized, unsupported, binary, or inaccessible files cheaply and
report skipped-content counts honestly.

### Result model

Extend `FileSearchResult` only with data needed by implemented UI:

- name-match range for highlighting;
- relevance score;
- stable discovery order if required for progressive sorting.

Add model operations for applying a stable sort without discarding selection.
Do not insert a generic proxy/model framework unless the concrete sort and
filter behavior requires it.

### Controller

The controller continues to own:

- scanner lifetime and cancellation;
- generation rejection;
- progressive result batching;
- result-update holds while the list is being scrolled;
- progress and coverage state.

Navigation into the active panel remains a QML/application interaction. Search
scanner code must not gain knowledge of panel controllers.

## Implementation Phases

### Phase 1: clarify existing behavior

- Add Name / Contents / Name + contents.
- Expose Hidden files.
- Expose Wildcard alongside Contains and Exact.
- Replace right-click copy with a result context menu.
- Wire Open containing folder and Copy path.
- Improve long-path behavior without introducing a one-off resizable dialog.
- Highlight name matches.

Verification:

- scanner tests cover all three target modes, case handling, hidden files, and
  wildcard matching;
- manual checks cover keyboard navigation, context actions, long paths,
  cancellation, and return to results.

### Phase 2: ordering and core filters

- Add Relevance, Name, Path, Size, and Modified sorting.
- Add kind, extension, and modified-date filters.
- Preserve current selection and scroll stability as progressive batches
  arrive.

Verification:

- deterministic model/controller tests cover score precedence, stable ties,
  directories, content matches, and filter boundaries;
- a large-tree manual scan confirms that results do not jump beneath the
  pointer while the user scrolls.

### Phase 3: scope selection and polish

- Added the explicit Search in selector.
- Supported current, left, right, and manually selected local folders.
- Added optional minimum and maximum size bounds.
- Refined unsupported-scope, empty, and failure states while retaining the
  scanner's progress and partial-coverage reporting.
- Kept filesystem-root search available through manual folder selection instead
  of adding a separate whole-computer option.

Verification:

- switching scope cancels the previous generation and cannot publish stale
  results;
- unsupported provider/archive paths show an explicit limitation;
- root searches retain mount-boundary behavior and honest skipped-path details.

## Out of Scope for This Workstream

- A persistent filesystem index or database.
- Provider-wide or cloud-account search without a provider search contract.
- Searching inside archives.
- OCR, PDF parsing, office-document parsing, or binary-format extraction.
- Regular-expression content search.
- Replacing platform search services such as Windows Search or Tracker.

These can be evaluated separately after the scanner-based workflow is polished
and its real performance limits are measured.

## Validation Checklist

- Build with `cmake --build build -j 12`.
- Run `FileSearchScannerTest` plus any new result-model/controller tests.
- Run the full test suite and `git diff --check`.
- Exercise local searches on a small tree, a large tree, `/`, hidden entries,
  inaccessible folders, symlink/mount boundaries, and files near the content
  size limit.
- Verify rapid query, mode, filter, and scope changes do not leak stale results.
- Verify scrolling during progressive publication does not reset selection or
  move the item under the cursor.
- Verify long paths and excerpts in both built-in light and dark themes.
