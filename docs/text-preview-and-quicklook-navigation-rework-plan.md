# Text Preview and Quick Look Navigation Rework Plan

## Status

Research and implementation plan, 2026-08-14. This document describes the
target behavior and staged implementation. Neither feature is marked as
implemented by this plan.

## Goals

This work has two related goals:

1. Replace the current text preview implementation with one predictable,
   bounded pipeline for plain text, source code, scripts, logs, configuration,
   markup, and other text-like files in both Preview Pane and Quick Look.
2. Add explicit Previous and Next buttons to Quick Look. They must follow the
   originating panel's visible order and remain correct under rapid navigation,
   asynchronous preview work, model changes, and provider latency.

The result should make text preview a first-class feature rather than a small
special case inside the general preview controller.

## Scope

### Text preview

In scope:

- local, administrator-readable, archive-contained, and provider-backed text;
- plain text, source code, scripts, structured text, configuration, logs, and
  extensionless files that are confidently detected as text;
- one shared presentation contract for Preview Pane and Quick Look;
- deterministic wrap, line numbering, scrolling, font controls, encoding,
  truncation, large-file paging, loading, errors, and cancellation;
- bounded memory and rendering work for very large files and pathological long
  lines;
- focused automated tests plus a repeatable UI acceptance corpus.

### Quick Look navigation

In scope:

- visible Previous and Next buttons in Quick Look;
- navigation through the originating panel's current filtered and sorted rows;
- correct panel current-item/selection/reveal behavior;
- exclusion of virtual special-action rows such as `Load more...`;
- generation-safe rapid navigation across local, archive, and provider items;
- deterministic boundaries and model-mutation behavior.

## Non-goals

- Do not turn the preview into a text editor.
- Do not add syntax highlighting in the first implementation. The architecture
  may expose a language id, but correctness, scrolling, and large-file behavior
  come first.
- Do not add search, minimap, folding, editing, save, or arbitrary encoding
  selection in the first implementation.
- Do not render Markdown or HTML as rich content. They remain readable source
  text.
- Do not download an unbounded provider file merely to preview it.
- Do not cycle from the last Quick Look item to the first or vice versa.
- Do not treat `Load more...` as a navigable preview target.
- Do not make Preview Pane navigation buttons part of this feature.
- Do not add global Left/Right keyboard shortcuts in the first slice. Text and
  media previews already use directional input; explicit buttons avoid an
  input-ownership conflict. Keyboard navigation can be specified separately.

## Research Summary

### Current text data flow

The current flow is split across several owners:

- `PreviewClassifier` and `isTextSuffix()` classify text in C++.
- `TextPreviewLoader::loadLocalPreviewData()` reads the first 8 KiB for normal
  local previews.
- `QuickLookController::loadFullText()` uses a separate 1 MiB limit.
- `QuickLookController::loadTextChunk()` uses independent 16 KiB byte chunks.
- `QuickLookController::previewPath()` has another archive-text branch.
- `ProviderPreviewMaterializer` first materializes provider content and then
  re-enters the local loader.
- `PreviewRenderer.qml` separately maps extensions and filenames to source-code
  labels.
- `TextPreview.qml` owns wrap heuristics, line-number delegates, scroll
  geometry, font state, and chunk controls.

Preview Pane and Quick Look bind to the same `QuickLookController` state, but
they apply different QML defaults. Only Quick Look exposes the current `Load`
and chunk controls.

### Current limits and why they are not a coherent policy

| Constant/current behavior | Value | Current effect |
| --- | ---: | --- |
| `kTextPreviewLimit` | 8 KiB | Initial local preview prefix |
| `kTextChunkSize` | 16 KiB | Quick Look byte-chunk page |
| `kTextFullLoadLimit` | 1 MiB | Explicit full-load ceiling |
| `kArchivePreviewExtractLimit` | 1 MiB | Archive text eligibility |
| remote materialization cap | 40 MiB | Provider staging ceiling |
| `maximumUnwrappedTextLength` | 8192 characters | QML silently forces wrap |
| Preview Pane line-number cap | 100 | Stops numbering while text continues |
| Quick Look line-number cap | 2000 | Stops numbering while text continues |

These values solve different local concerns but do not form one user-visible
state machine. In particular:

- a 15 KiB file first appears truncated, then changes mode after `Load`;
- a file just over 16 KiB enters byte-chunk navigation rather than simply
  displaying as a small text document;
- byte boundaries can split a UTF-8 sequence or CRLF pair;
- chunk line numbers restart at one and do not describe the file;
- wrap is derived from the currently rendered string length, not from an
  explicit user preference or document kind;
- source-code recognition exists in QML while text eligibility exists in C++;
- initial, full, archive, privileged, and provider paths duplicate decoding and
  truncation logic;
- `QuickLookController` publishes several correlated primitive properties, so
  QML can briefly observe a mixed old/new state;
- the current `Load` label does not explain whether it will load the complete
  file, enter pages, or stop at another limit.

### Current rendering problems

`TextPreview.qml` combines toolbar, text layout, line-number layout, and loading
overlay in one component. Its `ScrollView` relies on the implicit geometry of
the text control while also deriving width from `availableWidth`. Framework
changes to `FmTextArea` have already exposed binding loops and broken scroll
ranges.

The key requirement for the rework is not a particular QML control. It is a
tested geometry contract:

- the viewport has a stable width and height;
- the document surface owns a measurable content width and height;
- wrap changes only the horizontal layout policy;
- vertical extent always follows the rendered window;
- the horizontal and vertical scrollbars are anchored to the viewport and do
  not determine their own source geometry;
- changing text, wrap, font size, or page cannot create a binding loop.

`FmTextArea` should remain behavior-compatible with Qt `TextArea`, but the new
preview must have a focused runtime geometry test instead of relying on visual
inspection alone.

### Current asynchronous protection

`QuickLookController` already has useful foundations:

- a private two-worker `QThreadPool`;
- a preview generation increment for new targets;
- queued-work clearing when the generation changes;
- `QPointer` checks before publishing;
- generation checks on the GUI-thread publish step;
- cleanup-managed provider materialization.

These protections must be retained. The rework should stop duplicating the
same generation checks across text branches and give text page requests their
own revision within the active document generation.

### Current Quick Look opening path

`App.quickLookActiveTarget()` resolves the active panel's current preview target,
asks `QuickLookController` to load it, and opens a lazily created modal
`QuickLook`. The popup stores only `previewPath`; it does not retain:

- the originating panel;
- the originating directory identity;
- the visible model row;
- the latest requested row while a previous preview is still loading;
- Previous/Next eligibility.

The popup therefore cannot implement race-safe adjacency by looking only at
`quickLookController.path`. That path describes the most recently published
preview, which may lag behind rapid button clicks.

## Product Decisions

### One text model, two presentation profiles

The text data model is shared. Preview Pane and Quick Look may use different
visual density and initial viewport size, but they must consume the same
document/window state and use the same decoding, wrap semantics, line offsets,
and error states.

### No ambiguous `Load` mode

Remove the existing `Load`, `1 / N` byte-chunk UI, and public
`loadFullText()/loadTextChunk()` contract.

The replacement selects a mode automatically:

- a bounded small document is loaded completely in the background;
- a larger document opens a bounded, line-aligned window immediately and shows
  explicit Previous Page and Next Page controls;
- the UI says what is visible, for example `Lines 1-842` or `Beginning of file`,
  rather than exposing implementation chunks;
- paging never implies that the whole file has been loaded into memory.

Initial numeric thresholds are implementation candidates, not assumptions:

- full-document candidate: 256 KiB to 512 KiB;
- rendered-window candidate: 64 KiB to 128 KiB;
- maximum decoded/rendered characters per window: a separate hard cap.

Choose the final values only after the benchmark corpus described below. The
contract is bounded behavior, not a specific number.

### Explicit wrap semantics

Wrap has two user-visible states: On and Off. There is no hidden
`forcedWrapText` based on loaded string length.

Defaults:

- source code, scripts, logs, and structured text: Off;
- prose-like plain text and Markdown: On;
- the user can toggle either default;
- changing page or promoting from prefix to full document preserves the chosen
  state;
- opening another file resets to the default for that document kind in the
  first implementation.

Persistence across application restarts is deferred. If added later, it should
be a single text-preview preference, not a property inferred from text size.

A pathological single line is handled by the bounded render window and
horizontal scrolling, not by silently changing wrap.

### Line numbers

- Source code, scripts, structured text, and logs show line numbers by default.
- Prose-like text may hide them by default.
- Line numbers are global within the file, never restarted at one for a later
  page.
- The gutter is virtualized or painted; it must not create one QML `Label` per
  line for a large window.
- The first implementation does not promise an immediate total line count for
  a huge file. It may show the visible range while an incremental line index is
  available.

### Encoding policy

Decoding must be centralized and deterministic:

1. Honor UTF-8, UTF-16 LE, and UTF-16 BE BOMs.
2. Accept valid BOM-less UTF-8.
3. Detect NUL-heavy/binary input and return `NotText` rather than replacement
   character soup.
4. For invalid BOM-less UTF-8, use one documented platform-neutral fallback
   only if confidence is high; otherwise show an encoding error state.
5. Preserve decoder state across range boundaries.
6. Align rendered page boundaries so no multibyte sequence, CRLF pair, or
   surrogate is split.

The fallback encoding decision should be fixture-driven. Do not silently use
the process locale because the same file would preview differently by machine.

### Quick Look adjacency

- Previous/Next follows `DirectoryModel`'s current filtered and sorted order.
- Directories, ordinary files, archives, and provider items are eligible because
  Quick Look already previews them.
- Rows with `specialAction != None` are skipped.
- Boundaries disable the corresponding button; navigation does not wrap.
- A multi-selection summary (`selection://`), virtual overview, transient
  externally opened preview, or target not belonging to the captured model has
  no Previous/Next session.
- Navigation updates the panel's current item and selects only the navigated
  item, matching ordinary unmodified keyboard movement. It reveals the row with
  `Contain` behavior without closing Quick Look.

## Target Text Architecture

### 1. `TextPreviewController`

Add a focused QObject owned by `QuickLookController`, rather than adding more
text-only fields and workers directly to the general controller.

Suggested location:

```text
src/preview/text/TextPreviewController.h
src/preview/text/TextPreviewController.cpp
src/preview/text/TextPreviewTypes.h
src/preview/text/TextPreviewReader.h
src/preview/text/TextPreviewReader.cpp
```

Responsibilities:

- own the active text-document generation and page-request revision;
- expose one coherent immutable snapshot to QML;
- request initial/full/page reads on the existing private preview pool;
- reject stale document and page completions;
- retain a small bounded cache of adjacent decoded windows;
- expose `requestPreviousPage()` and `requestNextPage()`;
- expose no file-operation or panel-navigation behavior.

`QuickLookController` remains the owner of general preview target selection,
provider materialization, cleanup, and non-text preview state. It forwards a
text snapshot or exposes the child controller to QML.

### 2. Coherent snapshot

Replace correlated QML primitives with one revisioned snapshot. A concrete C++
struct can contain:

```text
documentId / generation
requestRevision
state: Empty | Loading | Ready | Paging | Error | NotText
mode: Complete | Windowed
textKind: Plain | Code | Script | Structured | Log
languageId / languageLabel
encodingLabel
content
firstVisibleLine
visibleLineCount
hasPreviousPage / hasNextPage
byteOffset / byteLength / totalBytes
errorCode / errorText
```

Publish the snapshot in one GUI-thread assignment and one notify signal. QML
must never combine the content from one page with page metadata from another.

### 3. `TextPreviewReader`

This is the non-QObject, unit-testable read/decode layer. It receives a source,
requested direction/window, and cancellation predicate, then returns a complete
snapshot payload.

It must:

- determine complete versus windowed mode from known total size and configured
  limits;
- read enough overlap to align start/end to code-point and line boundaries;
- return global line base information accumulated by sequential paging;
- distinguish EOF, read error, encoding error, binary input, cancellation, and
  success;
- never append presentation strings such as `...` to document content;
- keep status metadata separate from selectable/copyable text.

### 4. Source adapters

Use one small read-range contract instead of duplicating local/admin/archive
branches in the controller:

```text
totalSize()
readRange(offset, length, cancellation)
stableIdentity()
```

Adapters are needed for:

- ordinary local `QFile`;
- privileged `LinuxAdminBroker::ReadFile`;
- already materialized provider files;
- archive entries when the archive backend can provide a bounded device/range.

Provider materialization remains capped and cleanup-managed. If a provider
cannot supply a bounded preview source without downloading beyond policy, the
snapshot must say that preview is unavailable; it must not fall back to an
unbounded transfer.

Do not redesign the general provider ABI in the first slice. Start with existing
materialized local paths and add a provider range API only if an actual provider
requires it.

### 5. Centralized classification

Move language and text-kind mapping out of `PreviewRenderer.qml` into C++ beside
`PreviewClassifier` or the new text types.

Classification inputs:

- normalized filename and compound suffix;
- MIME inheritance;
- shebang for extensionless scripts;
- a bounded content sniff for text/binary confidence.

Classification output supplies text eligibility, `TextKind`, language id,
language label, default wrap, and default line-number visibility. QML renders
the result but does not maintain a second extension table.

Keep SVG classified as SVG/image where the SVG preview is supported; do not let
the generic text suffix list steal it merely because XML is textual.

### 6. New `TextPreview.qml`

Rewrite the component rather than incrementally preserving its current state
machine.

Recommended internal structure:

```text
TextPreview
├── TextPreviewToolbar
│   ├── font decrement / increment / reset
│   ├── kind/language label
│   ├── visible line/page status
│   └── explicit wrap toggle
├── TextViewport
│   ├── virtual/painted line-number gutter
│   ├── one read-only FmTextArea document surface
│   └── independently anchored scrollbars
├── Previous Page / Next Page controls (windowed mode only)
└── loading/error overlay that does not destroy the previous ready frame
```

Geometry rules:

- `FmTextArea` must preserve Qt `TextArea` content-size semantics.
- Do not bind `implicitHeight` to `ScrollView.availableHeight`.
- Do not make `ScrollView.contentWidth` depend on a child width which itself
  depends on `ScrollView.contentWidth`.
- Keep viewport dimensions and document content dimensions separate.
- Wrap On: document width follows a stable viewport width; vertical extent
  follows the laid-out document.
- Wrap Off: content width follows the text document; horizontal scrolling is
  enabled.
- The gutter follows the text viewport's vertical offset and uses global line
  numbers.
- Loading another page keeps controls responsive and may dim the previous page,
  but must not replace it with an unexplained blank surface.

The toolbar must retain compact content-sized `A-` and `A+` controls. Language
labels such as `JavaScript` or `PowerShell` must not force unrelated controls
off-screen; the label elides before controls overlap.

### 7. Preview Pane and Quick Look integration

`PreviewRenderer` passes a text snapshot and surface profile instead of the
current set of text flags.

- Preview Pane: compact toolbar and smaller font default, but complete access to
  wrap and paging.
- Quick Look: roomier typography and paging controls.
- Both surfaces use the same content, page metadata, error semantics, and
  classification.
- Per-surface visual preferences (font size, wrap toggle, scroll offset) remain
  in the QML instance and are reset only when the document identity changes.
- A snapshot revision change for the same document/page must not reset wrap or
  font size.

## Target Quick Look Navigation Architecture

### Navigation session

Quick Look needs a session separate from preview publication. The session is
created when Quick Look opens from a panel and stores:

```text
originating panel/controller identity
originating directory identity
requested target path
requested row anchor
navigation revision
session validity
```

The requested target is updated immediately on each click, before asynchronous
preview work begins. Never derive the next click from
`QuickLookController.path`, because that is a published-result path and may lag.

Transient Quick Look opened for an external path deliberately creates no panel
navigation session.

### Adjacency policy

Add a small unit-testable C++ policy, not a broad new panel abstraction. It
accepts the current visible model rows, requested path/row anchor, and direction,
and returns:

```text
target path
target row
canGoPrevious
canGoNext
valid / invalid reason
```

Rules:

1. Prefer resolving the current requested path in the live model.
2. If it disappeared during a model mutation, use the stored row anchor and
   clamp it to the surviving model.
3. Walk in the requested direction until an ordinary row is found.
4. Skip every special-action row.
5. Invalidate the session if the panel navigated to another directory.
6. Recompute boundary eligibility after model reset, insert, remove, sort, or
   filter changes.

The implementation can live in `src/core/QuickLookNavigationPolicy.*` with a
thin session owner in QML/App coordination, or in a focused
`QuickLookNavigationController` if signal ownership makes a C++ QObject simpler.
Do not put adjacency rules in `QuickLookController`; it does not own panel order.

### UI and panel synchronization

- Add overlay edge buttons to `QuickLook.qml`, vertically centered over the
  content area, using `FmIconButton` and existing left/right assets.
- Keep them outside `PreviewRenderer` so they navigate files, not pages or book
  content.
- Hide or disable them when no panel session exists; disable the unavailable
  direction at boundaries.
- Tooltips are `Previous item` and `Next item`.
- On click, synchronously commit the session's requested path/revision, then:
  1. set panel current item by path;
  2. select only that ordinary row;
  3. reveal it with `Contain` without closing Quick Look;
  4. update `QuickLook.previewPath` immediately;
  5. call `QuickLookController.preview(targetPath)`.

Preview Pane synchronization must not issue a competing preview request when
the selection change originated from an open Quick Look session. Add an explicit
preview owner/reason (`PaneSync`, `QuickLookOpen`, `QuickLookNavigate`,
`Restore`) or a narrowly scoped coordinator suppression token. Do not solve this
with an arbitrary timer.

### Race invariants

1. The last accepted button press owns the displayed target.
2. A stale local decode, metadata load, archive extraction, provider download,
   image inspection, or text page load cannot publish into a newer target.
3. Rapid Next, Next, Previous resolves from the session's requested target,
   even if none of the three previews has finished.
4. Button eligibility follows the requested target, not the last completed
   target.
5. Closing Quick Look invalidates the navigation session and all popup-owned
   callbacks.
6. Switching panels or navigating the originating panel to another directory
   invalidates the session rather than silently attaching to a new list.
7. Model mutation may choose a deterministic nearest survivor, but it cannot
   resurrect a removed path.
8. A provider failure affects only that target. The user may immediately move
   to the next target.
9. Preview generation and navigation revision are both checked before UI
   publication; neither substitutes for the other.

## Implementation Phases

### Phase 0: Fixtures, tracing, and regression harness

Before replacing behavior:

- add a deterministic text corpus under `tests/fixtures/text-preview/`;
- add `FM_TEXT_PREVIEW_TRACE` with generation, request revision, source kind,
  offsets, bytes read, decode result, publish/discard reason, and elapsed time;
- add `FM_QUICKLOOK_NAV_TRACE` with session id, navigation revision, origin
  panel/directory, requested/published path, row, and invalidation reason;
- never log document content, provider secrets, account data, or signed URLs;
- add a focused controller/policy test target so later phases do not depend only
  on manual Quick Look interaction.

### Phase 1: Pure text reader and classification

- introduce snapshot/types and the read/decode layer;
- centralize filename/MIME/shebang classification;
- test encodings, binary rejection, CRLF, multibyte boundaries, very long
  lines, no-final-newline, empty files, and line-aligned windows;
- keep the existing UI temporarily while comparing old/new loader results on
  the fixture corpus.

### Phase 2: Text controller and async state machine

- add `TextPreviewController` and integrate it with the existing private
  Quick Look worker pool;
- publish one revisioned snapshot;
- implement automatic complete/windowed selection and adjacent-page cache;
- route local and privileged files through the new reader;
- verify cancellation, stale-page rejection, and destruction behavior.

### Phase 3: New text UI

- replace `TextPreview.qml` internals with the new toolbar/viewport/pager;
- remove `Load`, byte-chunk arrows, `forcedWrapText`, per-line QML repeater, and
  QML language table;
- keep Preview Pane and Quick Look profiles in the shared component;
- add a UI Lab page with deterministic small, wrapped, unwrapped, long-line,
  windowed, loading, and error snapshots;
- add runtime checks that fail on binding-loop warnings.

### Phase 4: Archive and provider sources

- route archive text through the same snapshot and decoder;
- route existing bounded provider materialization through the same reader;
- preserve cleanup leases and remote caps;
- test cancellation during materialization and page changes;
- explicitly report unsupported bounded-range cases.

### Phase 5: Quick Look navigation policy and session

- implement and unit-test adjacency over visible rows;
- capture origin panel/directory/row when Quick Look opens;
- add preview owner/reason coordination;
- update current item, selection, and reveal through an explicit FilePanel entry
  point rather than reaching into view internals from the popup;
- add the edge buttons and live boundary state.

### Phase 6: Stress, polish, and removal

- remove obsolete text fields, methods, constants, and duplicated branches only
  after all sources use the new pipeline;
- exercise rapid Quick Look navigation over mixed local and provider folders;
- benchmark render and paging thresholds;
- update Help and `docs/qa-regression-suite.md`;
- retain trace flags for future diagnosis, disabled by default.

Each phase must be a logically complete, buildable slice. Do not combine the
text rewrite and Quick Look navigation in one unreviewable patch.

## Test Matrix

### Reader and decoder tests

- empty file;
- one line with and without final newline;
- LF, CRLF, and mixed newline fixtures;
- ASCII, UTF-8 Cyrillic, UTF-8 emoji crossing a read boundary;
- UTF-8 BOM, UTF-16 LE BOM, UTF-16 BE BOM;
- invalid UTF-8 and embedded NUL/binary data;
- extensionless shebang script;
- known code/config/log/prose filenames;
- a single line larger than the rendered window;
- page start/end alignment and global first-line calculation;
- first, middle, previous, next, and final pages;
- read error, privileged success/failure, cancellation, and stale revision.

### Text controller tests

- small file publishes one Complete snapshot;
- large file publishes Windowed without an explicit `Load` step;
- rapid Next Page, Next Page, Previous Page publishes only the last request;
- a new document invalidates all pages from the old document;
- wrap/font QML preferences are not reset by a page snapshot update;
- provider/archived source errors preserve a coherent error snapshot;
- cache remains within its byte/window bound.

### QML/runtime tests

- Preview Pane and Quick Look render the same snapshot;
- wrap On: no horizontal scrollbar, full vertical range;
- wrap Off: horizontal scrollbar appears only when needed and vertical scrolling
  still reaches the end;
- toggling wrap at top, middle, and bottom does not blank content;
- changing font size preserves valid scroll ranges;
- page replacement never shows an unexplained empty surface;
- `A-`/`A+` stay compact beside long language labels;
- no `Binding loop detected` warnings in the state matrix;
- gutter and text remain aligned while scrolling.

### Quick Look navigation policy tests

- first/middle/last ordinary row;
- ascending and descending sort;
- folders-first and mixed ordering;
- filtered model;
- hidden files shown/hidden;
- special action at the end or between synthetic test rows;
- removal of current, previous, and next paths;
- insert/reset/reorder while popup is open;
- directory identity change invalidates the session;
- selection summary and transient external preview have no session.

### Quick Look race tests

- three rapid clicks with completions returned in reverse order;
- slow provider item followed by fast local item;
- failed item followed immediately by Next;
- close while work is active;
- active-panel switch while work is active;
- origin folder navigation while work is active;
- Preview Pane enabled while Quick Look navigation updates selection;
- deletion/rename release invalidates a target safely.

## Benchmark and Acceptance Corpus

Use deterministic files rather than only synthetic repeated characters:

- 1 KiB plain prose;
- 15 KiB `warpfrogDownloader.py`-shaped Python source with many short lines;
- 250 KiB JSON and minified JSON;
- 500 KiB mixed-length source/log text;
- 5 MiB log;
- 100 MiB sparse/streamed log for bounded-memory checks;
- a 1 MiB single-line file;
- UTF-16 Cyrillic and multibyte-boundary fixtures;
- binary file with a misleading `.txt` suffix.

Record:

- time to first readable frame;
- time to complete small-document frame;
- wrap toggle and font-change latency;
- next/previous page latency;
- maximum resident bytes owned by the text session;
- GUI-thread stalls over 16 ms and 50 ms;
- stale request/publish counts;
- QML warnings and binding loops.

Threshold selection is accepted only when the 5 MiB and 100 MiB cases remain
bounded and the ordinary 15–500 KiB cases feel immediate.

## Manual Acceptance Scenarios

### Text preview

1. Open each corpus file in Preview Pane, then Quick Look.
2. Toggle wrap both before and after paging/loading.
3. Scroll to the bottom with wrap On and Off.
4. Change font size at top, middle, and bottom.
5. Page forward/backward through a large UTF-8 file and verify continuous line
   numbers and intact characters.
6. Repeat for an archive entry, administrator-readable file, and at least one
   provider-backed file.
7. Confirm no blank content, clipped final lines, false full-load state, or QML
   binding-loop warning.

### Quick Look navigation

1. Open Quick Look on the first, middle, and last row of a mixed folder.
2. Navigate rapidly across images, text, folders, archives, and a slow provider
   item.
3. Confirm the title, content, panel current item, selection, and button states
   always agree on the last requested item.
4. Change sort/filter while Quick Look is open and verify deterministic
   adjacency.
5. Remove the current item externally and verify nearest-survivor behavior.
6. Navigate the originating panel to another folder and verify the buttons
   disable rather than targeting the new list.
7. Repeat with Preview Pane enabled and confirm it does not overwrite the popup
   request.

## Acceptance Criteria

### Text preview

- Every supported text-like file uses one read/decode/state pipeline.
- Small text opens completely without an ambiguous `Load` action.
- Large text is bounded and pageable without byte-split corruption.
- Wrap is explicit, predictable, and independent of file/page length.
- Vertical scrolling reaches the final rendered line with wrap On and Off.
- Horizontal scrolling works in wrap Off without altering vertical extent.
- Global line numbers remain correct across pages.
- Preview Pane and Quick Look agree on content and state.
- Rapid target/page changes cannot publish stale content.
- No binding loops occur across the UI Lab text matrix.

### Quick Look navigation

- Previous/Next follows the originating visible model order and skips special
  actions.
- Boundaries are disabled and never wrap.
- The last rapid click wins even when async completions arrive out of order.
- Panel current item, selection, reveal, popup path, title, and content converge
  on one target.
- Folder navigation, popup close, panel switch, and destructive preview release
  invalidate stale sessions safely.
- A failed preview does not block navigation to other items.

## Expected File Impact

Likely additions:

- `src/preview/text/TextPreviewTypes.h`
- `src/preview/text/TextPreviewReader.{h,cpp}`
- `src/preview/text/TextPreviewController.{h,cpp}`
- `src/core/QuickLookNavigationPolicy.{h,cpp}`
- focused tests and text fixtures;
- a deterministic UI Lab text-preview page.

Likely modifications:

- `QuickLookController.{h,cpp}`;
- `PreviewClassifier.{h,cpp}` and `PreviewData.h`;
- `ProviderPreviewMaterializer.cpp` and archive preview integration;
- `TextPreview.qml`, `PreviewRenderer.qml`, `QuickLook.qml`;
- `App.qml`, `PreviewCoordinator.qml`, and one explicit FilePanel navigation
  entry point;
- `FmTextArea.qml` only where required to restore the base TextArea geometry
  contract;
- `CMakeLists.txt`, Help, and the QA regression suite.

Likely removals after migration:

- `kTextPreviewLimit`, `kTextChunkSize`, and `kTextFullLoadLimit` as unrelated
  public behaviors (replacement bounds live in the new policy);
- `loadFullText()`, `loadTextChunk()`, and primitive chunk properties;
- QML `forcedWrapText` and the extension-to-language table;
- duplicated local/archive text decoding branches;
- per-line `Repeater` gutter.

## Risks and Mitigations

| Risk | Mitigation |
| --- | --- |
| QML text layout blocks the GUI thread | Bound decoded/rendered window size and benchmark pathological lines |
| Scroll geometry regresses again | UI Lab matrix, warning capture, explicit viewport/content invariants |
| Encoding boundaries corrupt text | Stateful decoder and boundary fixtures |
| Line indexing scans giant files eagerly | Incremental sequential index; no immediate total-line promise |
| Provider preview downloads too much | Preserve materialization cap; require bounded source or clear unsupported state |
| Shared Preview Pane controller races popup navigation | Explicit preview owner/reason, no timer-based ownership |
| Rapid clicks use stale completed path | Navigation session owns requested path/revision independently |
| Model changes invalidate row numbers | Resolve by path first, retain row only as nearest-survivor anchor |
| Framework control fix changes editor layouts | Preserve minimum size and validate UI Lab plus Audio Tag Editor |

## Verification Gate Per Phase

Every implementation phase ends with:

1. focused unit/controller tests for that phase;
2. `cmake --build build -j 12`;
3. `ctest --test-dir build --output-on-failure`;
4. `git diff --check`;
5. a smoke run with QML warnings captured;
6. the relevant subset of the manual corpus.

The final phase additionally requires the complete manual scenarios and the
recorded benchmark table. Build success alone is not evidence that wrap,
scrolling, or navigation races are correct.
