# Resize frame lifecycle investigation

Date: 2026-10-01. This continues the [follow-up](resize-follow-up-research.md)
after the user authorized further investigation. The starting point includes the
accepted hidden-page, input-coalescing, and cache-transition fixes.

## Findings

Grid has a reproducible long-interval tail associated with newly populated
content children. In two traced baseline repetitions, 20 of 23 steady frames
with additions exceeded 13.889 ms (two nominal 144 Hz periods). These accounted
for 20 of the 25 long Grid intervals. None of the 23 destruction-only frames
exceeded that threshold. The 48 frames with a geometric column-count change
also stayed below it; additions generally followed in a later frame.

This separates column-count changes from the more expensive population work.
Counts cover direct contentItem children, including non-file objects; they are
not a private Qt delegate count or a CPU profile. Association alone does not
prove all of the delay comes from constructors. However, the controls below
also reduced the tail by changing creation of hidden full Grid content.

Detailed and Brief had no child additions or destructions after the first
500 ms in these runs. Detailed had 6 and 3 long intervals; Brief had 2 and 3.
Detailed steady p95 was about 7.84 ms in both repetitions. Its three largest
intervals per repetition were 24.9–28.8 ms, while the corresponding recorded
pre-synchronization elapsed time was 0.03–0.05 ms, synchronization 1.13–1.40 ms,
and rendering 1.01–1.75 ms. These stages do not explain the complete interval.
The observation does not establish that Detailed's lightweight RowLayout is
the cause. No Detailed production change is justified by this pass.

## Grid controls and retained change

New Grid items originally constructed their normal ColumnLayout, icon cell,
label, and hover handler synchronously even while that content was hidden by
the lightweight resize surface.

The first control put that normal subtree inside a Loader and deferred creation
of new hidden subtrees until release. Existing subtrees stayed alive. Churn-frame
p95 fell from 19.58–20.15 to 14.75–15.55 ms, but release duration increased from
9.78–10.60 to 28.81–30.34 ms. This control was rejected: it moved work to release.

The retained control uses the same small Loader boundary with
`asynchronous: gridDelegate.lightweightActive`. Existing full content remains
alive. Newly needed hidden content is instantiated across frames while the
lightweight surface is displayed. Returning to normal mode completes any pending
instantiation. This is Qt's documented Loader behavior; it does not turn QML
item creation into a background worker. [Qt Loader documentation](https://doc.qt.io/qt-6/qml-qtquick-loader.html#asynchronous-prop).

The icon-cell alias preserves thumbnail readiness checks and drag hit testing;
the original icon, text, margins, and translation stay inside the loaded layout.
Thumbnail suppression and cache recovery retain their previous policy.

Two traced asynchronous repetitions gave steady p99 of 15.40–15.70 ms, versus
19.58–20.15 ms in the traced baseline. Release stayed at 9.77–11.07 ms. The trace
does not count nested Loader creation: later incubation work can occur on frames
classified as stable. Overall intervals therefore remain the deciding measure.

The result was then checked without optional tracing, using three independent
processes per cohort:

| Grid metric | Baseline | Asynchronous full-content creation |
|---|---|---|
| Steady interval p95, ms | 8.07–10.16 | 8.71–10.23 |
| Steady interval p99, ms | 19.31–20.15 | 15.66–15.80 |
| Steady interval maximum, ms | 21.23–26.65 | 25.38–26.46 |
| Press call, ms | 32.25–34.57 | 33.40–34.32 |
| Release call, ms | 9.73–10.68 | 10.30–10.81 |
| Maximum recovery heartbeat interval, ms | 13.61–23.84 | 24.74–26.91 |

The repeatable improvement is the p99 tail, about 18–22% in this sample. Typical
cadence and worst-case intervals have not demonstrably improved. Recovery
heartbeat maxima are somewhat higher in the asynchronous cohort; their origin
has not been isolated. Press/release did not show the large regression of the
deferred control. All 16 measurement runs passed the benchmark correctness gates.
This is a bounded Grid improvement, not completion of the smoothness task.

## Measurement and reproduction

Optional `--resize-trace` adds public Qt signal observations to the existing GUI
benchmark; the Python runner exposes it as `--trace`. It records an immutable
per-frame snapshot, estimated column counts from width/cellWidth, cumulative
direct-child additions/destructions, and Qt frame stages. GUI geometry is read
on the GUI thread; render callbacks consume copied values under a mutex.

The afterAnimating observation runs after the resize controller flush. Thus it
excludes the flush itself. `preSyncElapsedMs` includes polish and any wait for the
render thread; it is not pure layout CPU time. Qt frameSwapped intervals are not
physical compositor presentation or input-to-photon latency. Instrumentation
can affect scheduling, hence the separate runs without tracing.

All runs used Wayland, Qt 6.11.2, an 1800×900 window, 300 generated PNGs, the same
three-second two-cycle divider trajectory, and a nominal 143.98 Hz output. Each
repetition used an independent process and temporary XDG_CONFIG_HOME. Data/OS
caches were retained. The asynchronous non-traced cohort ran before its baseline;
these are small sequential cohorts, not randomized statistical trials.

```sh
python3 scripts/panel-resize-benchmark.py --trace --runs 2 --output-dir /tmp/resize-trace
python3 scripts/analyze-panel-resize-trace.py /tmp/resize-trace > /tmp/resize-summary.json
python3 scripts/panel-resize-benchmark.py --modes 1 --runs 3 --output-dir /tmp/resize-grid
```

Trace grouping uses `afterAnimatingMs >= 500`; the existing benchmark's steady
statistics use frameSwapped elapsed time greater than 500 ms. Boundary-frame
counts can consequently differ. Groups overlap: a frame can change columns and
populate children. The analyzer compares counters with the preceding captured
frame before applying the steady-state filter.

[Archived results](resize-frame-lifecycle-results.json) preserve all 16 reports,
derived groups, and the two control diffs. Raw trace rows use the accompanying
`frameTraceColumns` order to keep the archive compact; no timing values are rounded.

## Validation and remaining work

Final validation is recorded in the [progress report](resize-responsiveness-fix-progress.md).
An additional Grid geometry assertion checks that the loaded normal layout has
the original margins and dimensions and that the icon center remains a valid
drag surface after recovery. It passed on the retained implementation.
The remaining Detailed and stable-frame Grid outliers need attribution of GUI
work between frame callbacks, including the controller flush, versus scheduling
and presentation. Normal navigation cost with the new Loader was not benchmarked
in this comparison. Real-mouse appearance and smoothness require user validation;
neither this benchmark nor an XWayland run proves native X11 equivalence.
