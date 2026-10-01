# Panel resize responsiveness fix plan

Plan date: 2026-09-30. Updated: 2026-10-01.
Status: the benchmark, Detailed/Brief geometry snapshots, central-divider input
coalescing, initial transition-pause optimization, and suppression of hidden
StorageView/FavoritesView geometry updates are implemented. Grid and final
hardware validation remain in progress.

Actual results and limitations: [progress](resize-responsiveness-fix-progress.md).
The [small follow-up](resize-follow-up-research.md) tested and reverted two
unproven hover/backdrop controls. Subsequent [frame lifecycle research](resize-frame-lifecycle-research.md)
identified Grid population-related tails and retained asynchronous creation of
hidden full content. Detailed's remaining outliers are not yet attributed.
Evidence: [research](resize-responsiveness-research.md),
[measurements](resize-responsiveness-data.json),
[comparison](resize-responsiveness-comparison.svg).

Documentation and artifact annotations for this task must be written in English.

## Goal and constraints

Maximize resize smoothness between the two file panels, between the sidebar and
workspace, and between the workspace and preview bar. The central divider has
priority. Preserve thumbnail suppression during resize and scrolling.

Optimize demonstrated geometry recalculations and resize-mode transitions.
Keep wheel speed, the visual framework, and compositor settings unchanged.
A Wayland-specific regression has not been established. The 144 Hz frame budget
is about 6.94 ms, but reaching it requires presentation and live-input validation,
not just a successful build.

## 1. Establish a reproducible measurement

- [x] Add a minimal repeatable resize scenario to the existing GUI benchmark,
  without dependencies on temporary research files. Use public test-input APIs;
  do not ship the private Qt layout probe.
- [x] Record a baseline with matching window size, files, trajectory, warmup,
  and view mode for before/after comparisons.
- [x] Record frame interval p95/p99/max, QTest move/press/release call durations
  including processEvents, divider following error in pixels, and resize state.
  These are not pure handler timings or physical latency measurements.
- [ ] Exclude mouse-grab loss, an already-active resize flag, and minimum-width
  clamping. Report these separately rather than treating them as lag.

Validation: the scenario actually moves the divider, ends with normal state in
both panels, preserves selection, and produces complete results. Timing is
informational; automated gates primarily check correctness, completion, and
absence of stuck state.

## 2. Remove unnecessary Detailed and Brief recalculations

- [x] Detailed: snapshot the effective column layout on resize entry and use it
  throughout lightweight mode. Hidden full delegates should not receive column
  width changes on every divider movement.
- [x] Release the snapshot and apply current layout on exit. Do not freeze
  ordinary column resizing through the header.
- [x] Brief: freeze lightweight single-column row width for the resize duration.
  Preserve this mode rather than switching to the regular two-column view.
- [ ] Check entry/exit, cancellation, repeated resize, delegate creation/reuse
  during resize, rename, selection, and minimum sizes.

Validation: geometry freezes only in the intended mode and current widths return
on completion, without clipping or blank regions after resize. Measure each
change separately. Earlier controls reduced Detailed move cost from 9–11 to
6–7 ms and Brief frame intervals from 15–17 to 11–13 ms; these are research
reference points, not guaranteed results. Additional freezing of the Detailed
lightweight row surface was not supported by measurements.

## 3. Limit central-divider recalculation to frame cadence

- [x] Choose a minimal public-Qt solution: apply the latest coordinate before
  layout/synchronization rather than recalculating both panels for every
  intermediate mouse movement.
- [ ] Preserve mouse grab, cursor, minimum widths, final release coordinate,
  cancellation, and persisted panel ratio.
- [ ] Avoid an additional frame of delay and dependence on a fixed timer or
  a particular monitor refresh rate.
- [x] Check sidebar, trailing preview, and preview between panels.

Validation: at most one necessary recalculation per frame, exact final geometry,
and measured following error. The synthetic coalescing control brought Grid to
9–10 ms p95, but did not itself validate production input. Do not ship a diagnostic
input controller without live-input checks and correct release/cancellation.

## 4. Reduce Grid relayout and mode-transition pauses

- [x] Use profiling to eliminate geometry updates in device/quick-access cards
  and favorite rows hidden in ordinary folders. Check geometry on opening those
  pages and during subsequent resize. [Results](resize-hidden-views-research.md).
- [x] Measure entry, movement, and exit separately: steady-state p95 must not
  conceal a long initial frame.
- [x] Correlate column changes and content-child lifecycle with frame intervals.
  Asynchronous creation of hidden full Grid content reduced measured p99 without
  the release penalty of deferring all creation until release. Typical cadence,
  recovery outliers, and live-input validation remain open.
- [ ] Investigate stable lightweight Grid geometry: a frozen cell size does not
  prevent relayout as viewport width changes.
- [ ] Choose a solution that reduces relayout while filling an expanding panel
  correctly. Freezing the entire viewport was diagnostic only; shipping it
  directly could leave blank space.
- [ ] Reduce mass delegate population changes on entry and exit while retaining
  thumbnail suppression and gradual cache recovery.
- [ ] Consider reuse only with lifecycle/transition validation: unconditional
  reuse reduced some outliers but increased press and release cost.

Validation: reduce interval tails and transition cost without moving the work to
the first frame, release, or thumbnail recovery. Check stale roles/icons,
selection loss, navigation correctness, and reuse correctness.

Transition update, 2026-10-01: Grid/Detailed trim cacheBuffer gradually; Brief
retains its cache for the return to two columns. Thumbnails remain suppressed.
Keeping the entire cache in every view was rejected because Detailed movement
regressed. Brief first-frame delay remains unresolved, so the complete task is
not closed. The user found transitions comfortable; further pause work is
currently deferred in favor of Grid/Detailed movement.

## 5. Final validation

- [ ] Compare Detailed, Grid, and Brief before/after on all three main dividers;
  also cover mixed panel modes and middle preview.
- [ ] Check fast movement, direction changes, repeated grabs, cancellation,
  size boundaries, selection, rename, and thumbnail recovery.
- [ ] Check real mouse input on Wayland and separately on a native X11 session.
  Running xcb inside Wayland means XWayland and does not replace native X11.
- [ ] Distinguish Qt submission timing, physical presentation, and subjective
  smoothness. Investigate remaining outliers with narrow traces when needed;
  do not change code based on assumptions.
- [ ] For each implementation slice, run suitable correctness checks, build,
  GUI smoke, and git diff --check, then validate appearance live.

Work in small independent slices: benchmark → Detailed → Brief → central divider
→ Grid/transitions → overall validation. Record actual effects and remaining
limitations after each slice. The user makes commits.


## Pre-submission review follow-up

- [x] Cancel shared wheel animation before scrollbar arrow clicks and repeats;
  cover both directions in all three file views.
- [x] Keep narrow-to-wide Brief resize in one lightweight column independently
  of the frozen inner content width; verify actual delegate placement.
- [x] Check all expected Grid viewport delegates after recovery with bounded
  readiness waiting, and verify rejection of missing visible full content.
- [x] Build, run the complete suite (79/79), repeat Grid recovery four times,
  and run two Wayland Brief smoke checks.

[Review evidence and fixes](pending-changes-review.md). These checks close the
three review findings; the live-input, native X11, and physical presentation
items above retain their separate validation requirements.
