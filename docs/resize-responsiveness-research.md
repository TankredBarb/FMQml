# Panel resize responsiveness research

Date: 2026-09-30. Scope: central divider between file panels, sidebar/workspace
divider, and workspace/right-preview divider. The user requested maximum
smoothness while preserving temporary thumbnail suppression. This investigation
made no permanent resize behavior change. Temporary instrumentation, QML names,
experimental bindings, and a private Qt profiling dependency were removed;
previously accepted wheel and MEGA changes were preserved.

## Result

There is substantial, measured application-side work in the resize path. The
central divider synchronously changes both panels on every delivered mouse move.
Detailed additionally updates column bindings in hidden full delegates; Grid
continues relayout as its viewport width changes despite frozen cell width;
Brief updates the width of every lightweight row. Switching resize mode also
changes the delegate/cache population and incurs a separate startup pause.

These are concrete optimization targets. They do not establish an exclusively
Wayland regression. XWayland exhibits expensive central resizing too. No tested
control demonstrated sustained 144 fps, and physical scanout tearing was not
measured.

[Comparison chart](resize-responsiveness-comparison.svg).
[Per-repetition measurements and snapshots](resize-responsiveness-data.json).

## Measurement method and exclusions

Qt 6.11.2, OpenGL scene graph, swap interval 1, output refresh reported by Qt as
approximately 143.98 Hz. Window size was checked to be exactly 1800×900 on both
platforms. Each process opened the same deterministic 300-image local fixture
in both panels: 1024×768 striped PNGs generated before timed intervals. Loading
settled before input. The production MEGA fix and accepted wheel changes were
already present in the measured working tree; its base commit was cb983204.

Each case had three three-second drags, two sinusoidal cycles per drag, followed
by recovery. Central amplitude was 180 pixels, sidebar 65, preview 100, avoiding
minimum-width clamping. Input was requested by a precise 4 ms timer; actual
input count falls under load. Panel geometry was restored between repetitions.
This is synthetic Qt input delivered through the same exported helper used by
Qt's GUI tests, not physical mouse input or a native libinput trace. Its helper
uses synchronous delivery. [Qt input implementation](https://github.com/qt/qtbase/blob/6.11/src/gui/kernel/qwindowsysteminterface.cpp).

Recorded separately: synchronous mouse-handler elapsed time, 2 ms GUI heartbeat,
frame submission timestamps, synchronization and rendering elapsed time, and
sparse before/during/after snapshots. Fields named `*Cpu` in the JSON are elapsed
wall time around synchronous work, not operating-system CPU accounting. Render
callbacks ran directly on the render thread and did not access GUI objects.

Only repetitions with normal resize state before and after, active resize during
the drag, and consistent pointer tracking are accepted. Some synthetic drags lost
the mouse grab and left resize state active. Those repetitions and subsequent
ones starting in that state are explicitly excluded in the JSON. The cause of
that benchmark failure is unresolved; it is not evidence of a production grab
bug. Physical input was filtered during injection; recorded filtered native
mouse event counts were zero. Selected paths remained unchanged in accepted runs.

Earlier exploratory runs used different window sizes, direct `sendEvent`, or
incorrect C++ property writes that did not remove QML bindings. They are excluded
from the tables and chart. All reported control states were checked in snapshots;
binding replacements used `QQmlProperty::write`.

Numbers below are ranges of individual repetitions' p95, not one pooled p95.
Cases were sequential, not randomized; compositor/background load was not
controlled. These small samples support targeted experiments, not universal
speedup percentages. Qt's `frameSwapped` measures queuing for presentation;
rendering callbacks measure CPU-side work, not full GPU execution or physical
scanout. [Qt QQuickWindow documentation](https://doc.qt.io/qt-6/qquickwindow.html#frameSwapped).

## Baseline

| Divider / view | Accepted repetitions | Move-handler p95, ms | Frame interval p95, ms |
|---|---:|---:|---:|
| Wayland central / Detailed | 3 | 9.36–10.62 | 14.62–21.06 |
| Wayland central / Grid | 3 | 3.51–3.75 | 14.89–17.74 |
| Wayland central / Brief | 3 | 4.66–5.12 | 15.22–16.81 |
| Wayland sidebar / Grid | 1 | 1.45 | 8.97 |
| Wayland right preview / Grid | 3 | 0.57–0.62 | 8.78–11.76 |
| XWayland central / Detailed | 1 | 7.86 | 19.26 |
| XWayland central / Grid, clean grab run | 3 | 3.23–3.55 | 13.37–13.74 |
| XWayland central / Brief | 1 | 3.98 | 11.01 |

The single accepted sidebar/XWayland Detailed/Brief repetitions are exploratory.
This was `QT_QPA_PLATFORM=xcb` inside the current Wayland session, not a separate
native X11 login. Grid was somewhat better under XWayland here, but these data
cannot attribute the user's perceived regression to the Wayland backend.

The 144 Hz budget is approximately 6.94 ms. Detailed's synchronous move handler
alone exceeds it. In baseline central resizing, synchronization p95 was about
4.3–4.9 ms for Detailed, 3.1–3.3 ms for Grid, and 3.4–3.6 ms for Brief. These
quantiles must not be added together as if they describe the same frame.

## Confirmed mechanisms and controlled experiments

### Central SplitView performs immediate layout

In Qt 6.11, `QQuickSplitViewPrivate::handleMove` calls `updatePolish()` immediately
while a handle is pressed; that performs layout and updates child geometries.
This is deliberate Qt behavior, not a Wayland-only branch.
[Qt SplitView source](https://github.com/qt/qtdeclarative/blob/6.11/src/quicktemplates/qquicksplitview.cpp).

The app's outer divider first changes workspace width; the nested split applies
its ratio through `Qt.callLater`. The central handle directly changes both
`FilePanel` widths inside the synchronous input path. See
[FileWorkspace.qml](../qml/components/FileWorkspace.qml).

A separate diagnostic called the installed Qt 6.11.2 private `handleMove`
directly, preserving ordinary press/release, actual panels, and rendering. It
bypassed Qt GUI/Quick mouse dispatch, not layout. All nine repetitions completed
correctly. Move p95 remained 8.60–9.14 ms for Detailed, 3.55–3.86 ms for Grid,
and 5.03–5.34 ms for Brief. Removing input dispatch did not remove the dominant
work; layout/binding propagation is the useful target. This private ABI probe
was temporary and is not a proposed implementation.

A control forwarding only the latest requested move once per `afterAnimating`
improved Grid frame p95 to 9.28–10.06 ms. This changes the synthetic input feed;
it is not proof that real platform input will behave identically. It introduces
approximately one-frame pointer following: p95 error 6–9 pixels at the tested
speed. A production solution must preserve press/release/cancellation, flush the
final coordinate, and avoid adding another frame of delay.

### Detailed hidden column bindings remain live

[FileTableAdaptiveDelegate.qml](../qml/components/FileTableAdaptiveDelegate.qml)
keeps `fullLoader.active: true` while hiding it and showing the lightweight
surface. Its full delegate's outer size is frozen, but
[FilePanel.qml](../qml/components/FilePanel.qml) still fits `detailsEffectiveLayout`
to live available width. Effective column widths propagate to the hidden
[FileTableDelegate.qml](../qml/components/FileTableDelegate.qml).

Snapshotting the effective layout on resize entry and releasing the snapshot on
exit reduced move p95 to 5.99–6.73 ms and frame p95 to 13.73–16.24 ms. This is a
measured unnecessary binding cascade. Freezing the lightweight surface width as
well did not improve the result; that additional change is not justified by
these measurements.

### Grid viewport relayout and Brief live row widths

Grid already freezes `cellWidth`, but its view still follows panel width. Qt
therefore continues relayout of the grid. Keeping actual GridView dimensions
fixed in a diagnostic reduced frame p95 to 10.09–10.94 ms; snapshots confirmed
panel width moved while view width stayed fixed. Startup gaps remained
approximately 53–58 ms. This separates steady relayout from entry cost; freezing
a complete view would leave blank/clipped content on expansion and is not a
ready production fix.

Brief's resize branch uses `cellWidth = max(160, width)`, while the captured
`resizeFrozenBriefCellWidth` is otherwise unused. A control captured the starting
**one-column viewport width**, preserving the existing lightweight one-column
concept rather than restoring normal two-column layout. Move p95 fell to
3.73–3.94 ms, frame p95 to 10.96–13.07 ms. Freezing the whole view gave 9.09 ms
frame p95 in one accepted repetition; that is supporting evidence only.

### Resize entry and exit change the delegate population

Resize entry disables reuse, sets cache buffer to zero, switches lightweight
rendering, and pauses thumbnails. The resize flags and thumbnail pause were
actually active in both panels; the existing optimization was not missing.

Before/during content-item child counts per panel changed approximately:
Detailed 64→22, Grid 164→50–57, Brief 173→30. These counts include cache/pool
objects and are not counts of visible files alone. Brief also changes from two
columns to one. Synchronous press cost was 15–16 ms Detailed, 19–20 ms Grid,
30–33 ms Brief; release was approximately 16–18 ms. The first submitted-frame
gap in Brief was 94–115 ms, extending beyond the press handler into the first
layout/render cycle. This is a separate startup problem, not a steady input
cost or proof that thumbnail suppression must be removed.

Forcing reuse on kept Grid child counts at 164 and reduced maximum drag frame
gaps from approximately 54–61 to 36–37 ms. However press cost increased to
25–28 ms and release to 23 ms. The combined reuse/coalescing control had frame
p95 7.99–9.67 ms, but one repetition still had a 101 ms mid-drag gap. Blindly
turning reuse on is not a complete fix; lifecycle correctness and startup cost
need their own work.

## Fix order and acceptance criteria

1. Freeze Detailed effective column layout and Brief lightweight cell width only
   for active panel resizing; restore current geometry once on exit. Preserve
   header resizing, rename, selection, minimum widths, and thumbnail pause.
   Test entry/exit and late-created/reused delegates, then check actual appearance.
2. Make central divider geometry updates follow the render cadence. Apply the
   latest coordinate before layout/synchronization, at most once per frame,
   rather than recomputing an expensive scene for intermediate mouse positions.
   Use public Qt APIs; preserve cursor grab, immediate final release geometry,
   cancellation, persisted ratio, sidebar/preview behavior, and display refresh
   independence. The input-coalescing control justifies this direction, not a
   particular event-filter implementation.
3. Reduce Grid relayout/delegate churn and make resize entry cheaper while
   retaining thumbnail suppression. Compare a stable lightweight scene versus
   current live reflow. Avoid shifting all startup work onto the first frame or
   all recovery work onto release. Keeping reuse is only a candidate with a
   measured tradeoff, not an unconditional recommendation.

For maximum smoothness, both steady cadence and transition tails must improve.
Retest all three views and all three divider routes at matched sizes, then use
physical mouse input in Wayland and a real X11 session. Report p95/p99/max,
tracking delay, press/release costs, and correctness separately. Sustained 144 Hz
requires physical presentation verification beyond this Qt submission probe.

Dolphin, sidebar vertical sections, Detailed column-header drags, middle-preview
placement, remote folders, and mixed view modes were not benchmarked here.
Temporary scripts/raw traces remain in `/tmp/fmqml-resize-research`; the durable
workspace artifacts above retain summarized measurements, excluded repetition
markers, state snapshots, and hashes of the pre-instrumentation source files.
