# QML JavaScript to C++ Boundary Migration Plan

## Goal

Move domain rules, provider semantics, state transitions, and data-model ownership out of QML JavaScript and into testable C++ code without turning the work into a full UI rewrite.

The plan covers five tracks:

1. Centralize path semantics and file-action availability.
2. Move provider presentation and special-entry behavior into C++.
3. Move Folder Compare action transitions into its C++ model.
4. Introduce a proper C++ Audio Tag Editor model and asynchronous session.
5. Introduce a proper C++ Batch Rename session, rule model, and preview model.

The testing plan assumes one developer, not a QA team. The default verification stack is therefore:

- fast, deterministic C++ tests for rules and state transitions;
- one focused integration test where threading, filesystem writes, or model wiring creates real risk;
- a short manual smoke test for QML binding and visual behavior;
- no screenshot tests, live-network tests, or exhaustive provider/account matrices.

## Implementation Status — 2026-07-15

The first delivery group is complete and committed. The working tree was clean
when this status was recorded.

| Track | Status | Commit | Result |
| --- | --- | --- | --- |
| Track 3 — Folder Compare transitions | Complete | `cd43bcb` | Planned-action cycling and validation are owned by `FolderCompareModel`; QML no longer calculates the next state. Scanner failures such as inaccessible paths are retained as per-entry results instead of aborting the whole comparison. |
| Track 1 — Path semantics and action availability | Complete for the planned migration scope | `acd5073` | `PathSemantics` and `FileActionPolicyEvaluator` own the migrated classification and policy rules. QML call sites delegate semantic questions to the controller/policy instead of parsing schemes themselves. |
| Track 2 — Provider presentation and special entries | Complete | `acd5073` | Provider entries carry semantic action, overlay, and recolor roles; C++ owns Load More dispatch and shared presentation resolution for panels, breadcrumbs, previews, and Quick Look. The obsolete `FilePanelIconPolicy.qml` was removed. |
| Track 5 — Batch Rename session | Complete | — | C++ typed rule, preview, and session models own rules, debounced preview generation, filtering, counts, and path-based apply reconciliation. QML no longer owns internal rule/preview arrays, and the obsolete controller preview adapter was removed. Batch apply uses a manually verified queued `FilePanelController` state machine with progress and path-based completion while preserving preflight, order, conflict, and result semantics. |
| Track 4 — Audio Tag Editor session | Complete | — | `AudioTagEditModel` and `AudioTagEditorSession` own editable records, original snapshots, current-row state, derived dirty tracking, cover operations, lookup request identity, busy/status/candidate state, result application, cancellation, staging cleanup, and path-based apply reconciliation. TagLib load/apply run asynchronously and report deferred results through the shared plugin dialog. CAA candidates are verified before display; the editor uses shared themed controls, visible non-overlapping scrollbars, and a music header icon. QML retains only view bindings and thin user-action delegates. |

Validation completed for the first delivery group:

- the full CTest suite passes: 33/33;
- the Folder Compare state-number/`nextPlanAction` cleanup gate has no matches;
- QML contains no `__load_more__` construction or dispatch;
- remaining `isProviderPath`, `pathCanShowProperties`, and
  `pathCanBeFavorited` QML helpers are thin controller delegates or UI
  composition checks, not independent scheme parsers;
- manual smoke testing covered Folder Compare, inaccessible scan entries,
  combined one-sided/different filtering, provider Load More, context-menu
  icons, breadcrumb menus and Telegram avatars, branded-icon recoloring,
  middle elision, and native/non-native provider folder badges.

Implementation notes:

- Some proposed test executable names were not copied literally. Coverage lives
  in the existing `folder_compare_scanner_test` plus the new
  `path_semantics_test`, `file_action_policy_evaluator_test`,
  `file_entry_presentation_resolver_test`, and
  `provider_semantic_plumbing_test` targets.
- Provider root URL constants remain in QML only where they are navigation
  destinations or provider-specific preview content identifiers. They are not
  used to synthesize model entries or infer Load More behavior.

### Next checkpoint

Tracks 4 and 5 are implemented. Track 4 has automated model/session coverage
and has been manually checked for asynchronous open/apply, verified cover
candidates, themed controls, scrollbars, and the music header icon. One final
manual smoke should cover lyrics selection, tag auto-fill, switching tracks
during/after lookup, and Apply/Apply All before committing the track.

## Current Boundary Problems

The QML code is not merely formatting data. In several places it currently:

- classifies paths and providers;
- decides whether an operation is legal;
- constructs provider protocol paths and magic load-more entries;
- owns state-machine transitions;
- duplicates editable records and reconciles copies manually;
- performs bulk filtering, dirty-state calculation, and result merging.

These are the kinds of behaviors that become difficult to keep consistent across context menus, command palette actions, dialogs, and future entry points. They also have much better test economics in C++ than in QML.

## Scope and Non-Goals

### In scope

- Extract concrete domain logic from QML JavaScript.
- Preserve current product behavior unless a documented inconsistency is being fixed.
- Add small C++ APIs that QML can bind to directly.
- Add tests in the repository's existing CMake/CTest style.
- Remove obsolete QML fallbacks after each migration is proven.

### Not in scope

- Rewriting all QML helper functions in C++.
- Replacing harmless formatting or view-only animation logic.
- Introducing a general-purpose Redux-style application store.
- Adding a GUI automation framework solely for this work.
- Testing real provider accounts in CI.
- Reworking unrelated backend behavior while moving ownership.

## Recommended Delivery Order

Keep the numbered sections below as the five product tracks, but implement them in this order:

1. **Track 3 — Folder Compare transitions.** It is small, self-contained, and proves the C++-model/QML-view migration pattern.
2. **Track 1 — Path semantics and action availability.** It removes a real consistency problem and creates shared primitives needed by provider presentation.
3. **Track 2 — Provider presentation and special entries.** It can then consume the path semantics from Track 1 instead of inventing another classifier.
4. **Track 5 — Batch Rename session.** It is a medium-sized model migration with no network or media-library dependency.
5. **Track 4 — Audio Tag Editor session.** It has the highest risk because it combines models, file I/O, staging files, async work, and external lookups.

Do not keep all five tracks on one long-lived branch. For a solo developer, each track should be a mergeable sequence of small commits:

1. C++ domain type/API plus tests.
2. QML migration with temporary compatibility fallback if needed.
3. Removal of the fallback and dead JavaScript.
4. Manual smoke and documentation update.

---

## Track 1 — Path Semantics and File-Action Availability

### Problem

Path classification and action availability are currently duplicated across `App.qml`, `CommandRegistry.qml`, `WorkspaceOverlays.qml`, and `FilePanelActionPolicy.qml`.

The duplication is already capable of producing different answers. For example, the Properties action can be admitted for provider paths by one entry point and rejected by another. The QML also repeats decisions that the controller already knows, such as wallpaper eligibility.

The desired rule is:

> Every entry point asks the same C++ policy for the same action and receives the same answer and, when useful, the same disabled reason.

### Target architecture

Add two layers rather than one oversized controller.

#### 1. `PathSemantics`

A pure or nearly pure C++ component that classifies and converts paths. It should use existing provider/archive helpers and `QUrl`, not QML regular expressions or string fragments.

Suggested public concepts:

```cpp
enum class PathKind {
    Empty,
    Local,
    FileUrl,
    Archive,
    DevicesRoot,
    FavoritesRoot,
    Provider,
    Unknown
};

struct PathDescriptor {
    PathKind kind;
    QString providerId;
    bool isVirtual;
    bool isWritable;
};
```

The exact type names can follow existing conventions, but the API should provide at least:

- classification of a path;
- provider identity when applicable;
- local path to/from file URL conversion;
- answers for archive, virtual root, and provider membership;
- host-independent handling for Windows drive and UNC forms when the application supports them.

Do not let `PathSemantics` know about selection state or active operations. It is a reusable vocabulary, not the complete action policy.

#### 2. `FileActionPolicyEvaluator`

A pure evaluator that accepts a snapshot and returns a snapshot:

```cpp
struct FileActionContext {
    PathDescriptor currentPath;
    PathDescriptor oppositePath;
    SelectionSummary selection;
    ClipboardSummary clipboard;
    bool operationBusy;
    bool administratorMode;
    // Other existing capability inputs.
};

struct ActionAvailability {
    bool enabled;
    QString disabledReason;
};

struct FileActionAvailabilitySnapshot {
    ActionAvailability copy;
    ActionAvailability move;
    ActionAvailability rename;
    ActionAvailability remove;
    ActionAvailability properties;
    ActionAvailability addToFavorites;
    ActionAvailability setWallpaper;
    // Remaining current actions.
};
```

The live QML-facing adapter can be named `FileActionPolicy` or `FileActionAvailability`. It gathers state from `FilePanelController`, the opposite panel, workspace/controller state, and clipboard state, calls the pure evaluator, then emits one coherent change notification.

The pure evaluator is important: it keeps most tests free from QObject construction and prevents the QML-facing adapter from becoming the only place where rules can be verified.

### Implementation phases

#### Phase 1.1 — Inventory and lock current behavior

- List every boolean/helper currently exposed by `FilePanelActionPolicy.qml`.
- Map every consumer in context menus, `CommandRegistry.qml`, `App.qml`, and `WorkspaceOverlays.qml`.
- Mark intentional differences. Treat all unmarked differences as bugs.
- Decide one canonical result for provider Properties. The existing backend support suggests provider Properties should remain available where provider metadata can be shown.
- Record action prerequisites in a compact table in the implementation PR description.

Deliverable: no product change yet, but the rule set is explicit.

#### Phase 1.2 — Add `PathSemantics`

- Reuse `FileProviderFactory`, archive helpers, and existing controller path code.
- Move generic path classification out of `FilePanelController::pathKindFor()` only if doing so does not break its public API; otherwise have the controller delegate to the new component.
- Add a shared file-URL conversion helper and remove manual `file://` string manipulation from migrated call sites.
- Keep current QML helpers temporarily as fallbacks while tests establish parity.

Deliverable: tested C++ path classification with no QML behavior change.

#### Phase 1.3 — Add the pure action evaluator

- Define a compact `SelectionSummary` instead of passing model rows into the evaluator.
- Encode action rules once.
- Include disabled reasons only where the UI currently displays them or where they will help future command-palette feedback. Avoid a localization project inside this refactor; reason codes may be preferable to final user-visible text.
- Delegate existing `FilePanelController` capability properties to the evaluator where practical, or clearly define which layer owns which capabilities.

Deliverable: table-driven policy tests and a single rule source.

#### Phase 1.4 — Add the live QML-facing adapter

- Expose action properties as constant names that map directly to the existing QML usage.
- Recalculate once when an input snapshot changes, then emit only the necessary notification signal or one `availabilityChanged` signal.
- Avoid binding loops: QML reads availability; it must not write policy state.
- Keep `FilePanelActionPolicy.qml` as a thin delegating shim for one commit if this reduces the size of the QML patch.

Deliverable: existing context menu works through C++ policy.

#### Phase 1.5 — Migrate all consumers

- Migrate the context menu first.
- Migrate `CommandRegistry.qml` second and verify provider Properties consistency.
- Migrate `App.qml` and `WorkspaceOverlays.qml` routing decisions.
- Replace QML wallpaper suffix checks with `FilePanelController::canSetWallpaperPath()` or the new policy result.
- Delete obsolete helpers such as duplicate provider/path classifiers.

Deliverable: all entry points use one policy.

### Automated tests

#### `PathSemanticsTest.cpp`

Use table-driven cases. Each row should contain input, expected kind, expected provider id, and expected normalized/local form where applicable.

Minimum cases:

- empty string;
- normal Linux absolute local path;
- local path with spaces, `#`, and `%`;
- `file:///` URL round trip;
- Windows drive path and file URL;
- UNC path and file URL;
- archive root and path inside an archive;
- devices root;
- favorites root;
- one path for each registered provider scheme;
- provider scheme with upper-case input if schemes are expected to be case-insensitive;
- unknown scheme;
- malformed scheme-like string that must not become a provider accidentally.

The Windows and UNC tests should operate on string/QUrl semantics and must not require the tests to run on Windows.

#### `FileActionPolicyEvaluatorTest.cpp`

Do not test every Cartesian product. Test one representative for each rule and the combinations most likely to regress:

- no selection;
- one local file selected;
- one local directory selected;
- multiple local files selected;
- provider item selected;
- archive item selected;
- operation already busy;
- opposite panel is local and writable;
- opposite panel is provider or virtual;
- clipboard empty and non-empty;
- administrator-only operation with mode off/on;
- supported and unsupported wallpaper file;
- provider path Properties action;
- favorites action on local versus provider/virtual path;
- checksum action for one file, two files, and a directory;
- enabled flag and disabled reason agree.

Include a named regression test for the previous provider Properties disagreement. A named test is easier to understand six months later than a generic row number.

#### Optional adapter wiring test

Only add a QObject-level adapter test if the adapter contains meaningful state aggregation. Verify that changing one input controller updates the expected snapshot and emits one coherent change. Do not test every property again; the pure evaluator already owns that coverage.

### Manual smoke test — 10 minutes

Run this once before merging the track:

1. Open a normal local folder and compare context-menu and command-palette availability for a file and a directory.
2. Open one available provider and verify Properties from both entry points.
3. Enter an archive and verify actions that should be blocked remain blocked.
4. Exercise copy/move availability with a valid and invalid opposite panel destination.
5. Start an operation and confirm conflicting actions disable while it is busy.
6. Check Add to Favorites and Set as Wallpaper on one eligible and one ineligible item.

### Definition of done

- No user-visible entry point implements its own capability rule.
- Provider Properties has one answer everywhere.
- QML no longer parses schemes to decide whether an action is allowed.
- Path and policy tests pass through CTest.
- Manual smoke finds no context-menu/command-palette discrepancy.

Suggested cleanup gate:

```bash
rg 'explicit(Scheme|PathScheme)|pathCanShowProperties|pathCanBeFavorited|isProviderPath' \
  qml/App.qml qml/components/app qml/components/filepanel
```

Every remaining match should be justified as presentation-only or removed.

---

## Track 2 — Provider Presentation, Icons, and Load-More Semantics

### Problem

Providers already return useful semantic icon names, and C++ already recognizes provider path forms, including load-more entries. QML still contains provider-specific branches, scheme checks, overlay decisions, and magic path construction such as `__load_more__`.

This creates three risks:

- a new provider or entry type requires edits across several QML files;
- panel, preview, breadcrumbs, and picker can render the same object differently;
- QML can construct a protocol path that no longer matches provider normalization rules.

The desired rule is:

> Providers and C++ presentation resolvers describe what an entry means; QML only chooses how to draw the supplied semantics.

### Target architecture

#### Extend `FileEntry`

Add stable semantic fields, for example:

```cpp
enum class FileEntrySpecialAction {
    None,
    LoadMore
};

struct FileEntry {
    // Existing fields.
    QString iconName;
    QString overlayIconName;
    FileEntrySpecialAction specialAction = FileEntrySpecialAction::None;
    bool iconRecolorAllowed = true;
};
```

Use semantic icon names, not QRC URLs, in providers. Asset-path resolution belongs in one presentation resolver.

#### Add DirectoryModel roles

Expose at least:

- `iconName` — already present;
- `overlayIconName`;
- `specialAction`;
- `iconRecolorAllowed` if the current theme behavior requires it.

Keep roles small and stable. Do not expose entire provider-specific metadata maps solely to avoid adding a role.

#### Centralize resolution

Extend `FileTypeIconResolver` or add a narrowly named `FileEntryPresentationResolver` that converts semantic fields into the icon source/overlay/tone used by the UI.

During migration it may provide a fallback from existing provider icon names to the new fields. The fallback must be removed after providers populate the fields directly.

#### Make load-more an action

Add a controller/model API such as:

```cpp
Q_PROPERTY(bool canLoadMore READ canLoadMore NOTIFY canLoadMoreChanged)
Q_INVOKABLE void loadMore();
```

Alternatively, make `openItem(row)` dispatch `FileEntrySpecialAction::LoadMore`. The important requirement is that QML never builds or normalizes a provider load-more path.

#### Enrich breadcrumbs

Extend breadcrumb entry maps or replace them with a typed value exposed to QML. Each breadcrumb should include presentation semantics such as:

- name;
- path;
- kind;
- icon name/overlay;
- recolor allowance or tone category.

`PathBar.qml` should not need to know provider schemes.

### Implementation phases

#### Phase 2.1 — Define semantic fields and compatibility roles

- Add the enum and fields to `FileEntry`.
- Register the enum for QML in the same way as existing model enums.
- Add DirectoryModel roles.
- Preserve current QML fallbacks so provider visuals do not change in the same commit.

#### Phase 2.2 — Populate provider entries

- Update each provider to set overlay/special-action/recolor semantics when creating entries.
- Start with Telegram and Instagram because their load-more behavior exercises the new action field.
- Update Google Drive shortcut/root semantics and Mega roots.
- Keep icon names semantic and theme-independent.

#### Phase 2.3 — Move load-more dispatch to C++

- Reuse existing Telegram/Instagram path helpers and controller recognition.
- Locate or dispatch the special entry by model data, not by a QML magic filename.
- Preserve the existing scroll-position behavior.
- Treat repeated `loadMore()` while a request is already active as a no-op.
- Expose an error/status through the same provider/controller mechanism already used for directory loads.

#### Phase 2.4 — Centralize icon and breadcrumb presentation

- Make grid, brief, details, preview, quick look, and breadcrumbs use the same resolver.
- Retain layout-specific sizing in QML; move only semantic selection to C++.
- Route file URL/local path conversion through `PathSemantics` from Track 1.

#### Phase 2.5 — Remove protocol knowledge from QML

- Delete provider scheme branches that only select icons or overlay badges.
- Delete load-more path synthesis.
- Delete duplicate manual `file://` conversion.
- Leave only intentional provider branding/layout branches, and document why each remains.

### Automated tests

#### `ProviderEntryPresentationTest.cpp`

Use table-driven semantic input/output. Cover:

- Google Drive root and shortcut;
- Mega root;
- Telegram saved messages, chat, channel, downloads, and load more;
- Instagram stories and load more;
- unknown provider entry with a normal file icon;
- provider-supplied overlay taking priority over compatibility fallback;
- recolor disabled for brand artwork where required.

Test semantic names and enum values, not QRC file paths. Asset existence can be a separate small resource test if the project does not already have one.

#### Extend provider path tests

Extend `TelegramPathTest.cpp` and `InstagramPathTest.cpp` with:

- load-more recognition;
- normalization with trailing slash;
- parent relation;
- case behavior;
- rejection outside the expected container;
- no collision with a real item whose display name resembles Load More.

#### DirectoryModel role test

Create a synthetic `FileEntry`, insert it through the smallest supported model seam, and verify `overlayIconName`, `specialAction`, and recolor role values survive model storage.

One row is enough. This is a role-plumbing test, not another provider matrix.

#### Controller load-more test

Use a fake provider/model. Verify:

- `canLoadMore` is false without a special entry;
- it becomes true with a load-more entry;
- `loadMore()` makes exactly one provider request;
- a second call while busy is ignored;
- successful completion retains the requested scroll anchor/state;
- provider failure clears busy state and reports the error.

Do not use real Telegram or Instagram accounts in this test.

#### File URL conversion test

If Track 1 did not already cover the shared helper, add cases for spaces, `#`, `%`, Windows drive forms, and UNC forms. There should be one conversion test target, not duplicate copies under both tracks.

### Manual smoke test — 10 to 15 minutes

Only test providers available to the developer locally; fake-provider tests cover the unavailable ones.

1. Compare one provider entry in grid, brief, and details views.
2. Open Preview/Quick Look and verify the icon/overlay agrees with the panel.
3. Check provider breadcrumbs and brand recoloring.
4. Trigger Load More and confirm no jump to a magic path and no lost scroll position.
5. Rapidly trigger Load More twice and confirm only one request/result is applied.
6. Open the folder picker from a file URL containing a space and verify its starting directory.

### Definition of done

- QML does not construct `__load_more__` paths.
- Provider entry meaning is carried by `FileEntry`/DirectoryModel roles.
- Panel, preview, and breadcrumbs share semantic resolution.
- Provider path tests and fake load-more test pass.
- No real account or network is required by CTest.

Suggested cleanup gate:

```bash
rg '__load_more__|telegram://|instagram://|gdrive://|mega://' qml
```

Remaining provider URLs should be intentional root/navigation constants, not parsing or entry-construction logic.

---

## Track 3 — Folder Compare Planned-Action State Machine

### Problem

`FolderCompareDialog.qml` currently implements the next-action transition and knows numeric state values for conflicts and inaccessible entries. `FolderCompareModel::setPlannedAction()` separately validates some of the same safety rules.

That gives the view two responsibilities it should not have:

- knowing the model's enum encoding;
- deciding which write operation is safe next.

The desired rule is:

> The model owns all planned-action transitions and validation; QML forwards the user's click.

### Target API

Add:

```cpp
Q_INVOKABLE void cyclePlannedAction(int row);
```

Internally, centralize:

```cpp
bool isActionBlocked(const FolderCompareEntry &entry) const;
FolderComparePlanAction nextPlannedAction(const FolderCompareEntry &entry) const;
bool isPlannedActionAllowed(const FolderCompareEntry &entry,
                            FolderComparePlanAction action) const;
```

Both `cyclePlannedAction()` and public `setPlannedAction()` must use the same validator. There must not be a safe path through one API and an unsafe path through the other.

### Transition contract

Preserve the current behavior unless a test demonstrates it is accidental:

- blocked/conflict entry: `Unresolved -> None -> Unresolved`;
- both sides exist: `None -> CopyLeftToRight -> CopyRightToLeft -> None`;
- left only: `None -> CopyLeftToRight -> None`;
- right only: `None -> CopyRightToLeft -> None`;
- neither side actionable: remain `None`;
- link/type/inaccessible/changed-after-compare safety is decided by C++ enum values, never numeric QML literals.

Confirm the exact treatment of equal symlinks versus link conflicts before coding; encode the decision in a named test.

### Implementation phases

#### Phase 3.1 — Add focused model tests around current behavior

- Construct entries directly or through the smallest existing test seam.
- Capture the transition table above.
- Include blocked states and symlink metadata.

#### Phase 3.2 — Implement the model transition

- Add private helpers.
- Make `setPlannedAction()` delegate to the same safety validator.
- Ensure the correct roles, aggregate counts, and execution-plan data update.
- Treat invalid row indices as a no-op.

#### Phase 3.3 — Simplify QML

- Replace `nextPlanAction(...)` and `setPlannedAction(...)` composition with one `cyclePlannedAction(index)` call.
- Remove state numbers and symlink conflict rules from QML.
- Keep icon/text formatting in QML.

### Automated tests

Prefer a focused `FolderCompareModelActionTest.cpp`. It is easier for one developer to run and diagnose than adding more unrelated cases to an already broad scanner test.

Minimum cases:

- both sides exist and complete the three-step cycle;
- left-only cycle;
- right-only cycle;
- blocked type conflict toggles Unresolved/None only;
- inaccessible-left, inaccessible-right, and changed-after-compare remain blocked;
- link conflict is blocked;
- equal compatible symlinks are not blocked unnecessarily;
- invalid row is a no-op;
- direct `setPlannedAction()` cannot bypass validation;
- filtered/sorted visible row maps to the intended underlying entry;
- a user override survives the model rebuild behavior that currently promises to preserve it;
- aggregate planned-action counts change correctly.

The test can inspect model data and counters directly. Do not introduce `QSignalSpy` unless signal cardinality is itself an API contract.

### Manual smoke test — 5 minutes

1. Compare folders containing left-only, right-only, and changed files.
2. Click each planned-action control through a complete cycle.
3. Filter or sort, change an action, and confirm the intended row changes.
4. Confirm conflict/inaccessible entries never offer an unsafe copy action.
5. Start execution and verify the resulting plan matches the visible icons.

### Definition of done

- QML does not reference Folder Compare state numbers.
- QML does not calculate next planned actions.
- Direct and cyclic setters share the same safety validator.
- Focused model tests cover every transition class.

Suggested cleanup gate:

```bash
rg 'nextPlanAction|state === (8|9|10|11|12)' qml/components/FolderCompareDialog.qml
```

The command should return no matches.

---

## Track 4 — Audio Tag Editor C++ Edit Model and Async Session

### Problem

The Audio Tag Editor currently keeps the same logical records in multiple QML structures: an array, an editor record object, and a `ListModel`. It copies maps with `slice()`/`Object.assign()`, recomputes dirty state, mutates model rows, and merges apply results in JavaScript.

Tag loading and apply operations are also synchronous at the QML boundary. On a slow file, network mount, embedded cover, or a large selection, this can block the GUI thread.

The current `applyCurrentCoverToAll()` pattern also repeatedly recomputes dirty state and can become quadratic as the selection grows.

The desired rule is:

> A C++ session owns typed editable records, dirty tracking, async file work, and result reconciliation. QML binds controls to the current record and renders progress/errors.

### Target architecture

Use two cooperating types.

#### `AudioTagEditModel : QAbstractListModel`

Own one typed row per selected file. Suggested value type:

```cpp
struct AudioTagEditItem {
    QString path;
    QString title;
    QString artist;
    QString album;
    QString albumArtist;
    QString genre;
    QString comment;
    int year;
    int track;
    QByteArray coverData;
    QString coverMimeType;
    DirtyFields dirtyFields;
    QString error;
    bool writable;
};
```

Retain an original snapshot or per-field original values in C++ so dirty state is derived reliably. Do not round-trip the original state through QVariant maps in QML.

The model should provide bulk operations as single methods, including `applyCoverToAll()`, so it can update rows in one linear pass and emit bounded model notifications.

#### `AudioTagEditorSession : QObject`

Own:

- the edit model;
- current index and current-record properties;
- loading/applying/lookup status;
- dirty count and apply eligibility;
- staging-file lifetime;
- asynchronous generation ids/cancellation state;
- the backend/worker abstraction.

Expose QML-friendly setters or properties for the current row instead of requiring QML to fetch and replace maps.

Suggested methods:

- `setSourcePaths(QStringList)` / `load()`;
- `setCurrentIndex(int)`;
- current-field setters;
- `clearCurrentTags()`;
- `setCurrentCover(...)`, `removeCurrentCover()`, `applyCurrentCoverToAll()`;
- `applyLookupFields(...)`;
- `applyCurrent()` and `applyAll()`;
- `cancelPendingLoad()` or `close()` with explicit semantics.

#### Worker boundary

The worker/service should consume and return value snapshots. Never move QObjects, model indexes, TagLib file handles, or QML-owned values across threads.

Use the project's preferred async mechanism (`QtConcurrent`/`QFutureWatcher` is sufficient if no common worker pool exists). Every completion must carry a generation/request id so stale results cannot overwrite a newer selection.

Be precise about cancellation:

- a queued or superseded load may be ignored/cancelled;
- an apply that has begun writing a file is not transactional;
- a cancellation request may stop before the next file, but must not claim to roll back files already written.

### Implementation phases

#### Phase 4.1 — Introduce typed records and the edit model

- Define typed load/apply result structures.
- Add model roles for all displayed fields, dirty state, write eligibility, and error.
- Implement current-row field mutation and original-value comparison.
- Implement clear/cover/bulk-cover operations.
- Keep the existing synchronous backend temporarily.

Deliverable: model tests pass while UI behavior is unchanged.

#### Phase 4.2 — Move QML state ownership to the model/session

- Replace the QML `records` array and duplicated `ListModel`.
- Bind the file list directly to `AudioTagEditModel`.
- Bind form fields to session current properties.
- Move dirty-count and apply eligibility calculation to C++.
- Remove `slice()`, `Object.assign()`, per-row `model.set()`, and manual dirty recomputation.

Deliverable: single authoritative edit state, still allowed to load/apply synchronously for this phase only.

#### Phase 4.3 — Make loading asynchronous

- Snapshot input paths.
- Load tags on a worker thread.
- Return value records and apply them to the model on the GUI thread.
- Ignore stale generations when selection changes or the component closes.
- Expose deterministic loading and error state.
- Avoid progressive row insertion unless it materially improves UX; one result batch is simpler and easier to reason about.

#### Phase 4.4 — Make apply asynchronous

- Snapshot only the records/fields to be written.
- Apply on a worker thread, one file at a time.
- Return per-path success/failure results.
- Reconcile by stable path, never row index.
- Update original snapshots only for fields successfully applied.
- Preserve dirty state for failures.
- Expose progress as completed/total if useful, without exposing worker internals.

#### Phase 4.5 — Consolidate lookups and staging lifetime

- Route lyrics/cover/metadata lookup results through the session with request ids.
- Move staging-file ownership into the session/backend.
- Release superseded and unused staging files deterministically.
- Keep lookup UI/layout in the plugin QML and preserve the plugin host contract.

#### Phase 4.6 — Remove compatibility code

- Remove map conversion helpers no longer used by QML.
- Remove synchronous QML calls.
- Remove manual staging cleanup loops.
- Retain a small compatibility API only if another non-QML caller actually uses it.

### Automated tests

#### `AudioTagEditModelTest.cpp`

No real media files or network are needed. Cover:

- loading typed rows resets current selection and dirty count correctly;
- selecting a row exposes the expected current values;
- changing one field marks only that row/field dirty;
- restoring the original value clears dirty state;
- clearing current tags updates supported fields and dirty count;
- setting and removing a cover;
- applying current cover to all supported rows in one operation;
- read-only/unsupported rows are skipped or reported according to the chosen contract;
- applying non-empty lookup fields does not erase populated fields unintentionally;
- per-path apply success updates the original snapshot;
- per-path failure preserves dirty state and stores the error;
- changing current index after a bulk update remains valid.

Avoid a brittle timing assertion for the linear bulk-cover operation. Instead, assert bounded notification/update behavior or add an optional benchmark target for local use.

#### `AudioTagEditorSessionTest.cpp`

Inject a fake worker/backend. Test:

- `loading` becomes true and false in the right order;
- a newer load generation wins and the stale result is ignored;
- destruction/close before completion is safe;
- apply-current snapshots one path only;
- apply-all snapshots only dirty/eligible records;
- results arriving in a different order reconcile by path;
- partial failure produces correct dirty count and error state;
- a cancellation request does not report rollback of already completed files;
- fake delayed work does not block the GUI event loop;
- stale lookup result does not replace a newer query's result;
- staging resources are released when superseded or when the session closes.

The fake worker should allow the test to complete requests explicitly. This is more deterministic than sleeping for arbitrary milliseconds.

#### One media integration test

Commit one tiny, license-safe silent MP3 fixture under `tests/data`. Do not require `ffmpeg` during the test.

The test should:

1. Copy the fixture into `QTemporaryDir`.
2. Load it through the real backend asynchronously.
3. Change the title.
4. Apply.
5. Reload and confirm the new title.

An embedded-cover round trip can be added if cover writing has historically been fragile, but do not build a codec conformance suite. MP3 plus the fake model/session tests is enough for this refactor. A developer can sample FLAC manually before release.

#### Do not automate

- live MusicBrainz, lyrics, or cover-service calls;
- every supported codec/container;
- network-share latency;
- visual layout and focus behavior.

### Manual smoke test — 10 to 15 minutes

1. Open one MP3, edit a field, apply current, close/reopen, and verify persistence.
2. Open about ten files, edit shared fields, and apply all.
3. Choose, remove, and apply a cover to all.
4. Use one metadata/lyrics/cover lookup and apply selected fields.
5. Include one read-only or intentionally invalid file and verify partial failure is understandable.
6. Change selection or close the editor while a load/lookup is pending and confirm no stale data appears.
7. Confirm panel metadata/thumbnail refresh after apply.
8. Before release, repeat the basic write/read smoke with one FLAC file.

### Definition of done

- QML has one C++-owned source of editable truth.
- Tag loading and applying do not block the GUI thread.
- Stale async results cannot overwrite a newer session.
- Apply reconciliation is path-based and preserves failed dirty rows.
- Live network services are absent from automated tests.
- The plugin host API remains compatible.

Suggested cleanup gate:

```bash
rg 'records\.slice|Object\.assign|recomputeDirtyCount|fileModel\.set' \
  src/plugins/audio_tags/AudioTagEditor.qml
```

The command should return no state-ownership matches.

---

## Track 5 — Batch Rename Session, Models, Filtering, and Debounce

### Problem

`BatchRenameDialog.qml` currently owns the rule list, converts it into QVariant structures, regenerates preview immediately on edits, filters preview rows in JavaScript, and merges apply results back by row/index.

This makes QML responsible for domain state and creates avoidable performance and correctness risks:

- multiple text edits can trigger multiple full previews;
- filtered and full preview state can disagree;
- index-based result merging breaks if ordering changes;
- conflict counts can accidentally reflect the filtered view rather than the full operation.

The desired rule is:

> A C++ session owns rules, preview scheduling, complete/filtered preview state, conflict accounting, and path-based apply reconciliation.

### Target architecture

#### Typed rules

Introduce typed enums and a value structure around the existing engine:

```cpp
enum class BatchRenameRuleType {
    Replace,
    RegexReplace,
    Prefix,
    Suffix,
    CaseConversion,
    Counter,
    // Existing rule types.
};

struct BatchRenameRule {
    BatchRenameRuleType type;
    bool enabled;
    QString pattern;
    QString replacement;
    // Type-specific options.
};
```

The first phase may convert typed rules to the engine's current QVariant format at one C++ boundary. Do not require an engine rewrite before the UI state migration can start.

#### `BatchRenameRuleModel`

A `QAbstractListModel` owning rule order, enabled state, selection, and editable fields. Expose methods for add/remove/move and current-rule properties for the editor panel.

#### `BatchRenamePreviewModel`

Own the complete preview result and a filtered row mapping, or provide a dedicated proxy model if it fits existing Qt usage. Roles should include:

- source path/name;
- proposed name/path;
- changed flag;
- conflict/error type;
- apply status/error.

Global conflict and changed counts must derive from the complete preview, not the filtered rows.

#### `BatchRenameSession`

Own:

- source paths;
- rule and preview models;
- selected rule;
- filter text;
- conflict/changed counts;
- debounce timer;
- preview generation;
- apply and result reconciliation.

Start with synchronous preview generation after a short debounce, for example 120 ms. Measure with a large synthetic list. Add background generation plus generation ids only if the measured UI pause is unacceptable. Avoid async complexity by reflex; the debounce alone may solve the actual problem.

Apply results must match by stable source path, not row number.

### Implementation phases

#### Phase 5.1 — Add typed rules and the rule model

- Define typed rule data without changing naming semantics.
- Add conversion to the existing engine request type at one boundary.
- Implement add/remove/move/select/edit.
- Preserve current default rule and UI order.
- Add model tests.

#### Phase 5.2 — Add the session and preview model

- Move source-path ownership into the session.
- Generate a complete preview through the existing engine.
- Store preview rows in C++.
- Expose changed/conflict/invalid counts.
- Keep preview generation immediate initially so parity is easy to verify.

#### Phase 5.3 — Migrate QML rule and preview bindings

- Replace the QML rule `ListModel` with `BatchRenameRuleModel`.
- Bind rule editors to current-rule properties.
- Replace JavaScript `getRules()` conversion.
- Replace QML preview clearing/appending with the C++ preview model.
- Keep delegate formatting and focus behavior in QML.

#### Phase 5.4 — Move filtering and result reconciliation

- Move case-insensitive source/proposed-name filtering into the preview model/session.
- Ensure global counts remain based on all rows.
- Reconcile apply results by source path.
- Preserve each failed row's preview and error.
- Define what happens when the filesystem changes after preview; at minimum surface backend failure clearly rather than silently trusting stale row positions.

#### Phase 5.5 — Add preview debounce and measure

- Restart one single-shot timer on rule edits.
- Generate one preview after the edit burst.
- Generate immediately for structural actions where feedback should be instant, such as adding/removing/moving a rule.
- Add a counting seam in tests so coalescing can be asserted without timing races.
- Run a local 5,000-path synthetic measurement.
- Only if the measured GUI pause remains unacceptable, move engine calculation to a worker using value snapshots and generation ids.

#### Phase 5.6 — Cleanup

- Remove `getRules()`, JavaScript preview filtering, internal preview copies, and index-based merge code.
- Remove compatibility QVariant conversion only if no other caller needs it.
- Keep the proven `BatchRenameEngine` naming/conflict algorithms intact.

### Automated tests

#### Extend `BatchRenameEngineTest.cpp`

Keep engine tests focused on naming semantics. Ensure there is at least one case for every existing rule type and combinations that historically cause ambiguity:

- literal replace;
- regex replace and invalid regex;
- prefix/suffix;
- case conversion;
- numbering/counter formatting;
- extension preservation/change behavior;
- stacked rule order;
- duplicate destination conflict;
- existing filesystem destination conflict if the engine currently checks it;
- unchanged result.

Do not duplicate these cases in the session test.

#### `BatchRenameRuleModelTest.cpp`

Cover:

- default rule;
- add/remove/select;
- moving a rule changes order;
- enabled state;
- editing each shared rule field;
- invalid selected index behavior;
- selected index remains sensible after deleting the current rule.

If this target becomes tiny, it may be combined with `BatchRenameSessionTest.cpp`; solo maintainability matters more than theoretical separation.

#### `BatchRenameSessionTest.cpp`

Use a real engine for normal preview rows and a fake/counting engine seam for scheduling/reordered apply results.

Minimum cases:

- setting sources creates the expected preview;
- a burst of field changes coalesces into one preview generation;
- structural rule changes generate a fresh preview;
- case-insensitive filter matches old and proposed names;
- filtering does not change global conflict count;
- clearing filter restores all rows;
- invalid regex is represented as an understandable error and blocks apply;
- a destination collision blocks apply;
- partial apply results arriving out of order match rows by source path;
- successful rows and failed rows receive the correct final status;
- changing source paths resets stale preview/apply state;
- zero sources and zero enabled rules are handled explicitly;
- if async preview is added, stale generation results are ignored.

For debounce, do not sleep and hope. Prefer an injectable scheduler, an explicit test hook that fires the pending preview, or a fake clock if the project already has one.

#### Large synthetic correctness smoke

Generate 5,000 in-memory source paths and verify row count, deterministic names, and conflict count. Do not impose a strict CI time threshold, which can be noisy. Record local timing in the PR and optionally add a non-default benchmark target.

#### Filesystem apply integration test

Use `QTemporaryDir` with a small set of files:

- successful multi-file rename;
- one deliberate conflict or missing source;
- confirm successful files exist under new names;
- confirm failed source is not reported as successful;
- verify result reconciliation by path when backend results are reordered.

Keep the fixture set small. Engine and session unit tests already carry the combinatorial coverage.

### Manual smoke test — 10 minutes

1. Try every rule type once.
2. Stack two rules and change their order.
3. Enter an invalid regex and verify the error/apply state.
4. Create a duplicate destination and verify the conflict count.
5. Filter by source and proposed name; confirm the global count does not change.
6. Cancel and confirm no files changed.
7. Perform one successful batch.
8. Cause one partial failure and verify the correct row reports it.
9. Once before merge, open 500–1,000 generated names and type rapidly in a rule field to judge responsiveness.

### Definition of done

- QML does not own the authoritative rule or preview arrays.
- Preview bursts are coalesced.
- Filtering cannot alter global validation/conflict counts.
- Apply results reconcile by path.
- Core naming behavior remains covered by the existing engine test suite.
- Async preview is added only if measurement shows it is necessary.

Suggested cleanup gate:

```bash
rg 'getRules|filterPreviewModel|internalPreviewModel' \
  qml/components/BatchRenameDialog.qml
```

The command should return no state-ownership matches.

---

## Cross-Track Test Strategy for One Developer

### Test pyramid

For each track, use the smallest test layer capable of finding the bug:

| Risk | Primary test | Why |
|---|---|---|
| Pure classification or transition rule | C++ unit-style executable | Fast, deterministic, easy to debug |
| QAbstractItemModel role/state wiring | Focused C++ model test | Avoids brittle UI automation |
| Threading and stale results | Session test with controllable fake worker | Deterministic ordering without sleeps |
| Real media/filesystem write | One small temp-directory integration test | Covers the external library boundary |
| QML bindings, layout, focus, icons | Short manual smoke | Cheaper and more reliable than a new GUI harness |
| Real provider/network behavior | Existing provider tests plus occasional manual check | CI credentials/network would be fragile |

### What not to build

For these five tracks, one developer should not spend time on:

- screenshot/golden image infrastructure;
- a complete Qt Quick Test harness;
- live service credentials in CI;
- tests for every permutation of selection/path/action state;
- hard performance thresholds on shared CI machines;
- duplicating the same rule cases at engine, model, and UI levels.

### Per-commit rule

Each behavior-moving commit should follow this sequence:

1. Add or update a C++ test that captures the behavior.
2. Move the behavior to C++ and make the test pass.
3. Point QML at the new API.
4. Run the focused CTest regex.
5. Run the track's short manual smoke before merge, not after every small commit.

### Suggested CTest groups

Register focused names so a solo developer can run only what changed:

```bash
cmake --build <build-dir> --parallel

ctest --test-dir <build-dir> --output-on-failure \
  -R 'PathSemantics|FileActionPolicy'

ctest --test-dir <build-dir> --output-on-failure \
  -R 'ProviderEntryPresentation|TelegramPath|InstagramPath|DirectoryModel'

ctest --test-dir <build-dir> --output-on-failure \
  -R 'FolderCompareModelAction'

ctest --test-dir <build-dir> --output-on-failure \
  -R 'AudioTagEditModel|AudioTagEditorSession|AudioTagIntegration'

ctest --test-dir <build-dir> --output-on-failure \
  -R 'BatchRenameEngine|BatchRenameRuleModel|BatchRenameSession'
```

Keep each focused group under roughly a minute on a normal developer machine. Longer media/provider integration checks can carry a separate label and run in full CI or before merge.

### Failure diagnosis policy

- A pure-rule failure should print the input case and expected/actual result.
- A model failure should print row, role, and current source path.
- An async-session failure should print request generation and completion order.
- A filesystem integration failure should preserve or report the temporary path when practical.
- Avoid one test executable with hundreds of silent integer return codes; small helper assertions with descriptive messages are worth adding even without adopting a full test framework.

## Cross-Track Implementation Rules

### Keep C++ APIs semantic

Good API:

```cpp
entry.specialAction() == FileEntrySpecialAction::LoadMore
policy.properties().enabled
folderCompareModel.cyclePlannedAction(row)
```

Avoid moving JavaScript string conventions verbatim into C++:

```cpp
entry.fileName() == "__load_more__"
path.startsWith("telegram://") // scattered through controllers
state == 10
```

The magic may still exist inside one protocol parser for compatibility, but it should not be the cross-layer contract.

### Use stable identity for async and apply results

- File operations: source path or an explicit immutable entry id.
- Audio records: normalized source path plus request generation.
- Provider requests: provider request id/generation.
- Never merge results solely by current row index.

### Keep presentation in QML

The migration is complete when QML receives semantic state, not when QML contains no JavaScript. These belong in QML:

- choosing dimensions and margins;
- formatting labels and tooltips;
- view-specific animation;
- focus/navigation wiring;
- delegate visibility based on supplied roles;
- purely visual color interpolation.

### Avoid notification storms

For bulk model changes:

- prefer one model reset for a complete replacement;
- prefer one contiguous `dataChanged` range for a bulk field update;
- update aggregate counts once after the batch;
- do not emit every property notification from every row unless each value truly changed.

### Preserve compatibility deliberately

Temporary fallback code is acceptable for one migration phase, but each fallback must have:

- a removal phase in the same track;
- a grep/check gate;
- no new callers added after the C++ replacement exists.

## Final Regression Pass

After all five tracks are merged:

1. Run the full CTest suite.
2. Run the five cleanup grep commands and review every remaining match.
3. Run one combined 30-minute manual session:
   - local and provider navigation;
   - context menu and command palette consistency;
   - provider icons/breadcrumb/load more;
   - folder compare action cycle and execution;
   - batch rename preview/filter/apply;
   - audio tag load/edit/apply/close.
4. Check the application log for QML binding errors, invalid enum conversions, and QObject/thread warnings.
5. Re-run an AddressSanitizer or equivalent debug build if the project already supports it, with special attention to closing Audio Tag Editor during pending work.

## Overall Completion Criteria

The effort is complete when:

- domain decisions in these five areas have one C++ owner;
- QML delegates user intent and renders semantic state;
- each extracted rule has a focused deterministic C++ test;
- async completion uses stable identity/generation guards;
- no automated test depends on a real provider account or live metadata service;
- each track has a short, repeatable manual smoke checklist a single developer can finish;
- the full existing regression suite remains green.
