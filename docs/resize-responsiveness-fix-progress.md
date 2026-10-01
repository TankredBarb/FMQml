# Panel resize progress as of 2026-10-01

Continuation of the [plan](resize-responsiveness-fix-plan.md). The user makes commits.

## Implemented behavior

Public QTest input moves the real SplitView in FM. The benchmark generates its
scenario and PNG files; Python launches each repetition in an independent process
with temporary XDG_CONFIG_HOME. No private Qt APIs are used. Data and OS caches
are retained. Correctness tests also exercise repeated grabs within one process;
timing comparisons use independent processes.

Detailed snapshots the effective column layout during lightweight mode. Brief
snapshots single-column row width. Current geometry returns after release.
PanelResizeController applies the latest movement in afterAnimating and delivers
the final coordinate before release. Native SplitView retains grab and width limits.

Earlier comparison of these changes gave frameSwapped p95 around 13 ms for
Detailed/Grid and 8.5–9.6 ms for Brief. These are Qt measurements, not evidence of
physical 144 FPS. [Raw results](resize-responsiveness-fix-results.json).
The user confirmed a substantial Brief improvement with a real mouse; Grid still
felt sloppy and Detailed fell between them. Attention therefore shifted to
transition pauses.

## Transition pauses: demonstrated cache cost

Entry simultaneously cleared cacheBuffer and the recovery-cache size. ContentItem
child counts per panel fell from 64 to 22 in Detailed, 164 to 57 in Grid, and 173
to 30 in Brief. These include hidden objects, not just visible files. A control
without clearing reduced press duration but regressed Detailed movement to
20–21 ms p95 from 13–14 ms, so it was not shipped unchanged.

The current fix retains Brief cache for the return to two columns. Grid/Detailed
trim it to zero over a 120 ms animation. Release stops that animation, then uses
the previous delayed 320 ms recovery. A new grab can interrupt recovery.
Thumbnail suppression and the resize reuse prohibition remain in effect.

The table compares three independent baseline repetitions per view with two fix
repetitions. One baseline repetition per view additionally records recovery.
QTest call duration includes processEvents and multiple stages; it is not pure
mouse-handler time.

| View | Press before → after, ms | Release before → after, ms | Maximum interval in the first 200 ms: one baseline → two fix repetitions, ms |
|---|---|---|---|
| Detailed | 63.5–69.3 → 24.6–24.8 | 23.6–24.9 → 23.4–23.8 | 34.4 → 21.3–21.5 |
| Grid | 69.4–73.9 → 34.7–35.2 | 20.6–21.3 → 21.1–22.0 | 42.4 → 15.5–21.3 |
| Brief | 102.9–104.8 → 59.6–60.3 | 142.7–146.8 → 41.6–42.1 | 66.4 → 37.3–39.2 |

First-frame delay did not improve consistently: Brief increased from 49–56 to
70–71 ms and Grid from about 30 to 37 ms; another baseline Grid repetition gave
3.9 ms. The signal may describe an already-scheduled frame and does not establish
that the new mode reached the display. Shorter press calls therefore do not imply
an equal reduction in visible latency.

Pauses became shorter but were not eliminated. Whole-drag p95 after this fix:
Detailed 13.4–13.8, Grid 14.5–14.7, Brief 8.7–9.1 ms. Baseline Grid in that series
was 10.6–13.3 ms, so improvement in steady movement was not demonstrated.
Recovery is observed for another 600 ms with GUI heartbeat. Recovery frameSwapped
intervals include idle waiting for scene changes; a large interval alone does not
prove blocking.

[Repetitions and controls](resize-pauses-results.json). The first animation
control did not actually trim cache because restart preceded target-binding
updates. cacheBuffer=1600 exposed the error, so that control was excluded from
gradual-trimming comparisons. An earlier Grid candidate that retained column
count while changing cellWidth regressed frames and was removed; its data is in
the preceding archive.

## Grid/Detailed movement: hidden pages

The user accepted transition feel; additional pause changes are deferred.
QML profiling identified width recalculations in device/quick-access cards and
favorite rows hidden in ordinary folders. Their anchors.fill now activates only
while visible. A second profile confirmed removal of that work. Opening virtual
pages restores correct geometry, and they continue to follow subsequent resize.

Two paired repetitions reduced p95 after the first 500 ms: Grid about 13 → 8 ms,
Detailed 13–15 → 8–9 ms, Brief 8 → 7 ms. These are Qt intervals, not proof of
physical 144 FPS. [Research and validation](resize-hidden-views-research.md).
The user confirmed good progress but still found Brief smoothest and requested
a small additional Grid/Detailed investigation.

## Small follow-up investigation

Two independent controls targeted inactive backdrop mapping and invisible hover
anchors. Neither gave a consistent p95 or long-frame improvement in Grid/Detailed;
both were reverted. All 12 follow-up runs passed correctness checks. Accepted
production behavior is unchanged. [Results and remaining leads](resize-follow-up-research.md).
All documentation and artifact annotations for this task are now in English.

## Frame lifecycle follow-up

Optional frame traces associated 20 of 25 long steady Grid intervals with new
content children; none of 23 destruction-only frames were long. Detailed and
Brief had no steady child population changes. Recorded synchronization/rendering
stages did not explain Detailed's largest intervals, so its remaining cause is
still open.

Deferring new hidden full Grid content until release reduced movement tails but
tripled the release call; that control was rejected. The retained Loader creates
the hidden normal subtree asynchronously during lightweight mode, keeps existing
content alive, and completes pending construction on return to normal mode.
Thumbnail suppression remains in place.

Three independent Grid runs per cohort without optional tracing reduced steady
p99 from 19.31–20.15 to 15.66–15.80 ms. Press/release stayed around 33–34/10–11 ms.
p95 ranges overlapped and rare maximum intervals were not improved. Maximum GUI
heartbeat intervals during recovery were somewhat higher (24.74–26.91 versus
13.61–23.84 ms); this limitation is recorded rather than treating the whole task
as complete. [Research, reproduction, and raw reports](resize-frame-lifecycle-research.md).

All 16 measurement runs passed correctness gates. A separate Wayland capture
run passed and its resize scene was inspected; it is excluded from timing data.
Final offscreen geometry checks covered rename and rapid repeated grabs in all
three modes, plus cache and thumbnail recovery. Real-mouse confirmation of the
new Grid behavior is still required.
The Grid geometry test additionally verifies the loaded normal layout's margins
and dimensions and icon-center drag hit testing after recovery; it passed.

## Validation and remaining work

Build and all eight selected CTest cases passed; the latest full selected suite
took 79.57 seconds. The additional Grid content geometry/hit-test assertion also passed.
Coverage includes the SplitView controller, all three views, geometry freeze and
recovery, selection, post-resize rename, rapid repeat grabs before recovery ends,
and cache/thumbnail recovery. Existing wheel scenarios and GUI smoke also passed.

The final Wayland transition check completed correctly in all three views with
cacheBuffer restored to 1600. Grid and Brief had ready thumbnails again. Detailed
does not expose those counters through its adaptive delegate; its pause flag was
checked. These three runs remain a separate correctness cohort, not extra entries
in the two-repetition transition comparison.

Before pause optimization, all primary dividers, both middle-preview handles,
and mixed view modes were checked. XWayland smoke passed; native X11 remains
untested. Grid relayout, remaining first-frame delay, Detailed header dragging,
and physical presentation remain separate validation items.

Reproduce from the repository root:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j12
python3 scripts/panel-resize-benchmark.py --runs 3 --output-dir /tmp/fm-resize-check
```

Other dividers: `--dividers sidebar preview middle-left middle-right`.
Mixed modes: `--right-mode`. `--platform xcb` runs XWayland inside a Wayland session.
`--capture` adds a scene image outside the main timing phase; exclude capture runs
from timing comparisons.


## Pre-submission review fixes

The [pending-change review](pending-changes-review.md) found three reproducible
P2 issues, now fixed: scrollbar arrow actions cancel pending wheel animation;
narrow Brief panels retain one lightweight column while the inner content stays
frozen; Grid recovery tests inspect expected viewport delegates with bounded
readiness waiting rather than unfinished cache children. The Grid assertion
also rejects deliberately missing visible full content before restoring it.

All four new regression cases failed on the original production behavior.
The final build and full CTest suite passed (79/79, 44.07 seconds), and the Grid
geometry test passed four consecutive repetitions (69.76 seconds). Two separate
Wayland Brief smoke runs passed, with steady Qt frame interval p95 of 7.27 and
7.37 ms. This small post-fix cohort does not establish a paired performance gain
or physical 144 FPS. [Archived results](review-fixes-results.json).
