# Debug Information and UI Lab Plan

Status: planning complete as of 2026-08-09. No application code is changed by this document.

This plan defines the next project after the native visual framework migration. It separates runtime diagnostics from visual component testing, turns the existing Debug Information dialog into a useful support report, and introduces a dedicated UI Lab overlay for manual and later automated visual regression checks.

The agreed project order is:

1. Fix newly discovered bugs independently in small bug-fix changes.
2. Rework Debug Information and build UI Lab according to this plan.
3. Start Find Duplicates only after the Debug/UI Lab work is accepted.

Bug fixes are not phases of this project and must not be accumulated into its implementation changes.

## 1. Current state

`qml/components/DebugInformationDialog.qml` currently combines two unrelated responsibilities:

- live runtime information about the application, clipboard, panels, history, plugins, and providers;
- an interactive showcase of framework components.

The runtime information is assembled directly in QML from global controllers. Most values are live bindings, there is no coherent snapshot timestamp, and the information cannot be copied as one safe report. The framework showcase is a long section inside a 640 by 520 dialog, making component comparison, narrow/wide layout checks, popup inspection, and theme comparison awkward.

`WorkspaceOverlays.qml` already creates Debug Information lazily. The new UI Lab should follow the same ownership and lazy-creation model, while presenting as a full application overlay rather than another compact dialog.

The native framework currently includes the Fm button, icon button, menu, combo box, text input, selection, tab, progress, slider, spin box, switch, scrollbar, toggle-row, and related native `QPainter` visuals. UI Lab must test those controls rather than create alternative visual implementations.

## 2. Product boundaries

### 2.1 Debug Information

Debug Information answers:

- What build and runtime environment is this process using?
- What is the current application state relevant to a bug report?
- Which plugins/providers are loaded?
- Which background or queued work is active?
- Can the user copy a useful, sanitized diagnostic report?

It is not a component gallery, theme editor, profiler, log viewer, or general settings screen.

### 2.2 UI Lab

UI Lab answers:

- Does a framework component render correctly in every important state?
- Does it remain usable under different themes, widths, font sizes, and backgrounds?
- Do keyboard focus, popups, menus, scrolling, clipping, elision, and animation behave correctly?
- Can a visual regression be reproduced in a deterministic scene?

It is not a replacement for application screens and must not become a second implementation of production components.

### 2.3 Explicit non-goals

- Do not create a universal popup, dialog, row, or card abstraction solely for UI Lab.
- Do not expose credentials, cookies, authorization headers, tokens, provider secrets, or private plugin state.
- Do not add continuous high-frequency polling throughout the application.
- Do not make UI Lab depend on real cloud accounts, mounted devices, network access, or mutable user files.
- Do not add pixel-baseline infrastructure before deterministic manual scenes work reliably.
- Do not redesign production controls as part of moving their examples.
- Do not mix Find Duplicates implementation into this project.

## 3. Debug Information redesign

### 3.1 Presentation

Keep Debug Information as an application dialog because it is a compact support tool. Increase its useful width only if the final report layout requires it; do not turn it into a full-screen dashboard.

The header remains `Debug Information`, with a subtitle that states the snapshot time. The footer should contain:

- `Refresh`;
- `Copy Report`;
- `Close`.

Optional path inclusion belongs beside `Copy Report` as a clearly labelled `Include file paths` checkbox. It defaults to off. The visible report may show elided paths for local inspection, but copied output follows the checkbox.

### 3.2 Snapshot model

Introduce one narrow C++ QObject, tentatively `DebugInformationController`, owned at application/controller level and exposed to QML. It should:

- collect one immutable snapshot on request;
- expose the snapshot as structured read-only data suitable for QML;
- format the text report from the same snapshot;
- sanitize report values before returning them;
- emit one `snapshotChanged` notification after refresh.

Do not build the copied report by concatenating labels in QML. One snapshot must drive both the visible UI and the copied report so they cannot silently disagree.

The controller may call existing controller/provider APIs, but it must not own application state or start work. Missing data is represented explicitly as `Unavailable`, not hidden or guessed.

Suggested API shape:

```cpp
Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
Q_PROPERTY(QString generatedAtText READ generatedAtText NOTIFY snapshotChanged)

Q_INVOKABLE void refresh();
Q_INVOKABLE QString reportText(bool includePaths) const;
Q_INVOKABLE void copyReport(bool includePaths);
```

Exact types may be tightened during implementation if a small typed value object is clearly simpler. Avoid a hierarchy of diagnostic models unless the live data proves a `QVariantMap` inadequate.

### 3.3 Snapshot sections

#### Build and runtime

- application version;
- build type when available;
- git revision when supplied by the build;
- build timestamp only if it is deterministic and already available;
- Qt runtime version;
- operating system name and version;
- process architecture;
- Qt platform plugin;
- active graphics API/renderer when obtainable through supported Qt APIs;
- process memory RSS;
- snapshot timestamp.

If build revision/type are not currently compiled into the application, add minimal CMake definitions rather than parsing the repository or invoking Git at runtime. Packaged builds must work outside a checkout.

#### Display and appearance

- window size;
- device pixel ratio;
- active theme identifier/display name;
- dark/light state;
- configured font family;
- effective base font size or typography scale;
- relevant UI scale when available.

#### Storage paths

- executable/application directory;
- configuration directory;
- cache directory;
- temporary/staging root;
- cleanup subsystem root/status if already exposed.

Paths are visible locally but excluded or reduced to non-identifying forms in copied reports unless `Include file paths` is enabled.

#### Panels and navigation

For left and right panels:

- active/inactive state;
- normalized scheme/provider;
- current path only under the path policy above;
- view mode;
- visible item count;
- selection count;
- back/forward history counts;
- hidden-file state;
- filter/search state when cheaply available.

Do not enumerate selected filenames into the default report.

#### Clipboard and operation state

- clipboard operation kind and item count;
- no clipboard paths by default;
- Operation Queue active/pending/failed counts;
- current operation kind and progress when present;
- administrator-mode state and backend, without secrets.

#### Plugins and providers

For every loaded plugin:

- plugin id;
- display name;
- version/API version if exposed;
- supported schemes;
- declared capabilities;
- load state;
- binary path only when paths are included.

Plugin diagnostics must not call authentication APIs or serialize provider sessions.

#### Background subsystems

Include only state already available without forcing expensive work:

- Quick Look busy/idle and generation where meaningful;
- File Search busy/idle;
- Disk Usage busy/idle;
- active cleanup work;
- thumbnail/cache counters only if reliable counters already exist or can be added cheaply.

Do not add broad instrumentation to every subsystem in the first implementation. Missing counters can be listed as later diagnostic extensions.

#### Recent errors

The first implementation should report structured errors already retained by controllers/providers. It must not invent an in-memory global log collector merely for this screen.

If a later error journal is added, it requires its own bounded-storage and redaction design.

### 3.4 Redaction policy

Sanitization is mandatory and centralized in C++.

Always redact or omit:

- passwords and passphrases;
- OAuth/access/refresh tokens;
- cookies and authorization headers;
- session identifiers;
- URL query/fragment data that may contain secrets;
- provider-private request payloads;
- environment variables except an explicit safe allowlist.

Default report behavior:

- local paths: omitted or replaced with a stable label such as `<local-path>`;
- remote paths: retain scheme and high-level provider only;
- filenames and selected item names: omitted;
- plugin binary paths: omitted;
- counts, versions, states, and capability names: retained.

`Include file paths` may restore ordinary filesystem/plugin paths, but it never disables secret redaction.

Add focused tests for sanitization. The tests should include URLs with credentials, token-like query parameters, Windows and Linux paths, provider URLs, and benign version strings that must remain intact.

### 3.5 Refresh behavior

- Capture a snapshot when the dialog opens.
- `Refresh` captures another complete snapshot.
- Do not bind the entire dialog continuously to global controllers.
- A low-frequency memory update may be added later, but consistency is more important than animation.
- Copying uses the most recent displayed snapshot and states its timestamp.

## 4. UI Lab experience

### 4.1 Overlay and lifecycle

Create `qml/components/UiLabOverlay.qml` as a dedicated full-size application overlay. Register it lazily in `WorkspaceOverlays.qml` with the same close/Escape and overlay-state integration as other workspace overlays.

Tentative application API:

```qml
openUiLab()
openUiLabPage(pageId, scenarioId)
```

Do not keep UI Lab instantiated when it has never been opened. Closing may retain the object like existing overlays, but any running indeterminate animations and timers must stop while hidden.

The normal entry point may live beside Debug Information in the hidden/developer command path. UI Lab should not become a prominent ordinary-user feature unless a later product decision says so.

### 4.2 Layout

Use the established application-overlay visual language:

- header with title, current theme/environment summary, and close action;
- left navigation for page categories;
- central scrollable scene canvas;
- compact top toolbar for environment controls;
- optional inspector panel only where a page has meaningful interactive parameters.

The central scene must use available space rather than a fixed dialog width. It should remain usable at the minimum supported application window size.

### 4.3 Environment toolbar

Required controls:

- theme selector using the real theme model;
- viewport preset: Narrow, Medium, Wide;
- font-size/typography preset using supported settings;
- scene background: Panel, Surface, Strong Surface, Checkerboard;
- `State Matrix` toggle where supported;
- `Reset Scene`;
- current page/scenario identifier suitable for a bug report.

Changing Lab environment must not permanently overwrite user settings without an explicit action. Prefer temporary overrides scoped to Lab. If the current theme system cannot safely support scoped overrides, document that limitation and restore the original values on close.

Do not create a parallel fake theme implementation.

### 4.4 Navigation categories

Initial pages:

1. Overview
2. Buttons and Actions
3. Text Inputs
4. Selection Controls
5. Combo, Menu, and Popup
6. Tabs and Navigation
7. Progress and Activity
8. Scrollbars and Scrolling
9. Lists, Rows, and Delegates
10. Dialog Surfaces
11. Typography
12. Colors and Surfaces
13. Icons
14. Composite Patterns

Pages should be separate QML files loaded by a small page registry. Do not place the entire Lab in one monolithic QML file.

Suggested structure:

```text
qml/components/ui_lab/
  UiLabPageRegistry.qml
  UiLabSection.qml
  UiLabScenario.qml
  pages/
    UiLabOverviewPage.qml
    UiLabButtonsPage.qml
    UiLabInputsPage.qml
    UiLabSelectionPage.qml
    UiLabMenusPage.qml
    UiLabProgressPage.qml
    UiLabScrollbarsPage.qml
    ...
```

Only extract `UiLabSection` or `UiLabScenario` after two pages prove the same Lab-only layout pattern. These helpers are test-harness components, not production framework controls.

### 4.5 State Matrix

Where component APIs allow deterministic states, show side-by-side examples for:

- normal;
- focused;
- highlighted/checked/selected;
- disabled;
- error/destructive;
- long text;
- minimum useful geometry;
- wide geometry.

Hover and pressed are generally read-only interaction states. Do not add production-only writable `forceHovered` or `forcePressed` APIs simply for the Lab. Provide interactive targets and clear instructions for those states. If deterministic screenshot capture later requires state injection, implement it in a test-only harness boundary rather than the production control API.

Each scenario should display:

- stable scenario id;
- short expected-behavior note;
- relevant geometry/value readout when useful;
- no dependence on live application data.

### 4.6 Required primitive coverage

#### Buttons and actions

- `FmButton`: standard, highlighted, destructive, flat, disabled, long text;
- `FmIconButton`: normal, active, disabled, tooltip, different icon sizes;
- keyboard focus and Space/Enter activation;
- icon-only and icon-plus-text alignment where supported.

#### Inputs

- `FmTextField`: empty, value, placeholder, focused, error, disabled, long value;
- `FmTextArea`: short/long, wrapped, error, disabled;
- `FmSpinBox`: min/max, disabled, keyboard editing;
- editable and non-editable `FmComboBox`.

#### Selection

- `FmCheckBox`: unchecked, checked, partial, disabled;
- `FmSwitch`: off/on/disabled;
- `FmToggleRow`: title/subtitle, warning tone, disabled, long labels;
- any radio control only if it is an actual supported framework component.

#### Menus and popups

- `FmMenu` surface and geometry;
- items with and without icons;
- separators;
- disabled item between enabled items;
- current/selected item;
- submenu where supported;
- keyboard navigation, Escape, click-outside close;
- long labels and constrained width;
- combo popup with enough rows to require a scrollbar.

#### Tabs and navigation

- different tab counts;
- long labels;
- disabled tab if supported;
- keyboard navigation;
- narrow-width behavior.

#### Progress and activity

- `FmProgressBar`: 0, tiny nonzero, 25, 50, 100 percent;
- minimum-fill on/off;
- indeterminate;
- error/danger and disabled;
- different supported track heights;
- `FmProgressRing`: idle, determinate values, busy, disabled.

#### Scrollbars and scrolling

This is a priority regression page.

- full vertical scrollbar with arrows and liquid active state;
- compact `flat` vertical scrollbar;
- horizontal scrollbar;
- AlwaysOn, AsNeeded, and AlwaysOff behavior;
- very short and very long content;
- nested scrolling equivalent to Lyrics;
- full main-page scrolling equivalent to the audio-tags editor;
- list scrolling equivalent to Sidebar and Combo popup;
- thumb size and placement at start/middle/end;
- wheel, drag, arrow repeat, and keyboard behavior;
- explicit checks for full-height anchoring and right-edge placement;
- no binding-loop warnings while content size crosses the visibility threshold.

#### Typography, colors, and icons

- every Theme typography token with sample and resolved pixel size;
- multiline, elide, and wrap examples;
- theme text colors over relevant surfaces;
- accent, warning, danger, success, focus, and disabled contrast;
- classic and current icon sources at supported sizes;
- tinting and missing-icon fallback.

### 4.7 Composite patterns

Composite scenes validate real arrangements without turning them into framework abstractions:

- settings toggle row;
- form section with labels, inputs, validation, and actions;
- error banner with standard and destructive actions;
- file/list row with icon, two text lines, selection, hover, and disabled state;
- dialog header/content/footer;
- menu opened from a toolbar action;
- scrollable editor containing a nested scrollable text area;
- progress summary card.

Use deterministic fixtures and existing production components where ownership permits. If copying a small production composition is clearer than adding a shared abstraction, keep it Lab-local.

### 4.8 Keyboard and focus inspection

Every interactive page must support a predictable Tab/Shift+Tab path. Provide a visible focus indicator and a small readout of the currently focused scenario/control.

Acceptance checks include:

- no focus trap except an intentionally modal popup;
- Escape closes the current popup before closing UI Lab;
- Enter/Space behavior matches the corresponding production control;
- disabled controls are skipped where Qt semantics require it;
- opening and closing menus returns focus sensibly.

## 5. Determinism and visual capture readiness

### 5.1 Deterministic scenes

UI Lab fixtures must use:

- fixed strings stored with the page;
- fixed list lengths and values;
- bundled icons;
- no current time in the scene canvas;
- no network/provider requests;
- no randomized animation phase for capture scenarios;
- stable viewport presets.

Indeterminate animation pages may have an explicit `Pause animations` capture mode that sets a stable phase through already-supported component properties. Do not freeze the entire application event loop.

### 5.2 Stable routing

Each page and scenario receives a stable id, for example:

```text
scrollbars/full-vertical
scrollbars/nested-flat
menus/disabled-middle-item
inputs/text-field-errors
```

The overlay must be able to open directly to an id. This is useful for bug reports immediately and enables a screenshot harness later.

### 5.3 Screenshot automation follow-up

Automated capture is a later phase after manual acceptance. The first automation slice should:

- launch a dedicated Lab route;
- apply a fixed theme and viewport preset;
- wait for scene readiness, not an arbitrary long sleep;
- capture only the deterministic scene region;
- save an image and metadata containing build, theme, page, viewport, DPR, and Qt version;
- return nonzero on missing page, QML error, or capture failure.

Initial baseline matrix:

- System light and dark where available;
- Porcelain;
- Graphite;
- Ember;
- one narrow and one wide viewport;
- representative menu, scrollbar, input form, progress, and dialog scenes.

Do not generate the Cartesian product of every component, state, theme, viewport, DPR, and platform. Baselines must remain reviewable. Other themes remain part of manual Lab inspection until a concrete regression justifies adding them.

Image comparison requires tolerances for platform font rasterization and graphics backends. Begin by archiving captures for human comparison; introduce pixel/perceptual failure thresholds only after collecting stable evidence on supported CI runners.

## 6. Ownership and integration

### 6.1 Expected files

Likely additions:

- `src/controllers/DebugInformationController.h/.cpp`;
- `qml/components/UiLabOverlay.qml`;
- `qml/components/ui_lab/UiLabPageRegistry.qml`;
- page files under `qml/components/ui_lab/pages/`;
- focused controller/redaction tests;
- later, a UI Lab capture launcher/script.

Likely modifications:

- `CMakeLists.txt` for controller, QML resources, and tests;
- application context registration/ownership;
- `WorkspaceOverlays.qml` for lazy UI Lab creation and overlay state;
- `App.qml` or the existing developer command/shortcut entry points;
- `DebugInformationDialog.qml` to consume snapshots and remove framework examples.

### 6.2 State ownership

- `DebugInformationController` owns only its last snapshot and formatting/redaction logic.
- Application controllers remain authoritative for runtime state.
- `UiLabOverlay` owns temporary Lab navigation and environment state.
- Theme/user settings remain authoritative outside Lab.
- UI Lab page fixtures own their deterministic sample models.

### 6.3 Theme override safety

Before implementing the Lab theme selector, inspect how `Theme`, `themeController`, and settings persistence interact.

Preferred behavior:

1. Save the active theme/environment when Lab opens.
2. Apply a Lab-scoped preview without persistence.
3. Restore the exact previous state on close or application shutdown.

If a scoped preview is not supported, phase one may show the active theme and provide a link/action to use the existing theme selector. Do not silently persist theme changes merely for inspection.

## 7. Implementation phases

### Phase 0: inventory and contracts

1. Inventory every current Debug field and its owner.
2. Inventory framework controls and current showcase examples.
3. Confirm safe APIs for build metadata, renderer information, theme preview, queue state, and plugin descriptors.
4. Record fields that are unavailable without disproportionate instrumentation.
5. Finalize the snapshot schema and redaction tests before editing QML.

Verification:

- each planned diagnostic field maps to an existing source or is explicitly deferred;
- no secret-bearing API is included;
- page registry covers every committed framework component.

### Phase 1: diagnostic snapshot and report

1. Add `DebugInformationController` and register/own it consistently with other controllers.
2. Implement snapshot capture and deterministic text formatting.
3. Implement centralized sanitization and path policy.
4. Add unit tests for report stability and redaction.
5. Keep the current dialog presentation temporarily while controller tests stabilize.

Verification:

- refresh produces one internally consistent timestamped snapshot;
- copied report contains no test secrets;
- missing optional plugins/controllers do not break snapshot creation;
- tests cover both path modes.

### Phase 2: Debug Information UI

1. Rebuild the dialog around snapshot sections.
2. Add Refresh, Include file paths, Copy Report, and Close.
3. Remove all framework showcase content from the dialog.
4. Verify long values, unavailable fields, small windows, and scrollbar behavior.

Verification:

- opening the dialog performs one refresh;
- Refresh updates all sections together;
- copied text matches visible snapshot values under the path policy;
- dialog contains no interactive framework showcase;
- no binding-loop or QML warnings.

### Phase 3: UI Lab shell and primitive pages

1. Add lazy overlay lifecycle and developer entry point.
2. Add page registry, navigation, environment toolbar, and deterministic routing.
3. Move and expand primitive framework examples into separate pages.
4. Implement Buttons, Inputs, Selection, Menus, Progress, and Scrollbars first.
5. Add Typography, Colors, Icons, Tabs, and remaining pages.

Verification:

- direct page routing works;
- closing/reopening does not leak popups, focus, or animations;
- theme/environment changes are restored safely;
- all framework controls have at least one deterministic scenario;
- menus and scrollbars are exercised in their known regression states.

### Phase 4: composite and interaction coverage

1. Add the agreed composite scenes.
2. Add keyboard/focus readout and inspection instructions.
3. Add narrow/wide/long-text scenarios.
4. Manually inspect the supported theme set.

Verification:

- Tab/Shift+Tab and Escape behavior pass page checks;
- no clipping or overlap at the minimum supported window size;
- System, Aurora, Porcelain, Ember, Graphite, Calus, and Catppuccin receive a manual pass;
- findings are fixed as separate bug changes when they are production defects.

### Phase 5: capture-ready follow-up

1. Add stable scene-ready signaling.
2. Add direct launch arguments or an equivalent test route.
3. Capture representative deterministic scenes and metadata.
4. Establish a small human-reviewed baseline set.
5. Decide separately whether CI image comparison is stable enough to enforce.

Verification:

- repeated capture on the same environment is stable;
- invalid route/capture failures are reported;
- capture mode leaves normal application startup unchanged;
- baseline set remains intentionally small.

## 8. Acceptance matrix

### Debug Information

- [ ] Framework examples are absent from Debug Information.
- [ ] Snapshot has a timestamp and refreshes atomically.
- [ ] Build/runtime, appearance, panels, operations, plugins, and background-state sections render.
- [ ] Missing optional data displays `Unavailable` without QML errors.
- [ ] Copy Report uses the displayed snapshot.
- [ ] Default copied report excludes local paths and item names.
- [ ] Include file paths affects ordinary paths only, never secrets.
- [ ] Redaction tests cover token/cookie/credential URL cases.
- [ ] Dialog behaves correctly with a full scrollbar and small window.

### UI Lab shell

- [ ] UI Lab is a separate lazy full-size overlay.
- [ ] Developer entry point and Escape/close lifecycle work.
- [ ] Stable page/scenario ids and direct routing work.
- [ ] Environment reset/restore does not corrupt user settings.
- [ ] Hidden Lab does not keep unnecessary animations/timers active.

### Component coverage

- [ ] Every committed Fm framework control appears in the registry.
- [ ] Normal, disabled, selected/highlighted, error/destructive, long-text, and constrained geometry are covered where applicable.
- [ ] Hover/pressed remain genuine interactions, not production API overrides.
- [ ] Menu disabled-row hover, icons, separators, keyboard navigation, and popup scrolling are covered.
- [ ] Full and flat scrollbars, nested scrolling, anchoring, visibility threshold, and liquid behavior are covered.
- [ ] Progress boundary values and indeterminate mode are covered.
- [ ] Keyboard focus path and Escape hierarchy are verified.

### Project boundary

- [ ] Production visual bugs discovered through Lab are fixed separately.
- [ ] No speculative framework abstractions are added for one Lab scene.
- [ ] No real accounts/network/mutable user files are required.
- [ ] Full build succeeds.
- [ ] Focused controller tests and the full existing test suite pass.
- [ ] Startup smoke test has no QML warnings.
- [ ] `git diff --check` is clean.

## 9. Delivery and commit strategy

Keep implementation reviewable:

1. diagnostic controller and tests;
2. Debug Information UI conversion;
3. UI Lab shell and routing;
4. primitive page groups in bounded slices;
5. composite/keyboard coverage;
6. optional capture harness.

Do not combine unrelated bug fixes with these slices. A visual issue found while building Lab should be reproduced in Lab, then fixed in its own bug-fix change unless it blocks the current Lab slice.

## 10. Transition to Find Duplicates

Find Duplicates begins after phases 1 through 4 are accepted. Phase 5 screenshot automation may continue independently if it does not block feature work.

Before starting Find Duplicates:

- Debug Information must provide a safe copied report;
- UI Lab must cover the native framework and known menu/scrollbar regression scenarios;
- theme/environment changes must restore correctly;
- the full application regression suite must pass;
- known framework bugs do not need to be exhausted, because they remain an independent bug-fix stream.

The existing Find Duplicates design remains in `docs/next-feature-work-plan.md` and should be revalidated against current traversal, cancellation, fingerprinting, hard-link, and Operation Queue APIs when that project starts.
