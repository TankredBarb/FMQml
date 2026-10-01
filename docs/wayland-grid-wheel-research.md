# GridView mouse-wheel investigation on Wayland

Date: 2026-09-30. Application revision: `cb98320`. Qt: 6.11.2.

## Scope and conclusion

Investigate a subtle change in mouse-wheel scrolling feel after switching to
Wayland, rather than obvious freezes. Phases 1–2 were research only; phase 3
below records the subsequently authorized implementation and its verification.

Phase 2 below confirms the input-fragmentation effect on the real file panel
and evaluates an accumulated-destination prototype.

The isolated probe did not show a substantial frame-cadence regression between
Wayland and XWayland. It did reproduce a strong sensitivity to fragmentation
of wheel input in Qt's default GridView scrolling. This is a plausible reason
for changed scrolling feel, but the actual mouse event stream and the former
native X11 session were not captured. The user's perceived regression remains
unconfirmed.

## Actual application routes

- Over grid cells and empty areas, MouseAreas reject wheel events and let
  `GridView` / Qt `Flickable` handle them. There is no custom contentY animation
  over the cells. `pixelAligned` is false and bounds behavior is StopAtBounds.
- Over the vertical scrollbar, `FmScrollBar.routeWheel()` accumulates a target
  position and animates toward it with OutCubic easing over 110 ms. One delta of
  120 moves 48 px. The previous TextPreview fix changes accepted event devices;
  it does not change scrolling duration, step size, or the GridView input route.
- With this Qt version and default settings, native GridView wheel scrolling
  uses a 300 ms OutExpo transition; a delta of 120 moves 72 px with three
  wheel-scroll lines. These routes therefore have different speed and curves.

Relevant code: `qml/components/FilePanel.qml` (GridView around line 3580,
contentY callbacks around line 3628), `FilePanelGridDelegate.qml` (wheel rejection
around line 641), and `qml/components/framework/FmScrollBar.qml` (routeWheel).

## Measurements

A temporary C++/QML probe used the installed Qt and the actual `FmScrollBar`
QML/C++ visual. Its GridView had 10,000 synthetic entries, simple Rectangle/Text
cells, StopAtBounds, VerticalFlick, pixelAligned=false, cacheBuffer=1600 and
reuseItems=true. Theme colors were stubbed. It deliberately omitted the real
DirectoryModel, file delegates, thumbnail loading and panel callbacks.

Synthetic QWheelEvents had a mouse source, no pixel delta, no scroll phase and
explicit timestamps. Each case started away from the content boundaries.
The probe waited 650 ms after input to measure the completed movement.

The live sequence was Wayland, XWayland, Wayland. The current compositor was
KWin Wayland; XWayland is **not** a native X11-session baseline. Screen mode was
1920x1080 at 143.98 Hz, scale 1, VRR disabled. All probes reported three wheel
scroll lines. A separate offscreen run was used only as a functional check.

Final displacement in pixels:

| Synthetic input | Wayland A | XWayland B | Wayland A repeat |
| --- | ---: | ---: | ---: |
| single-120 | 72 | 72 | 72 |
| single-15 | 9 | 9 | 9 |
| eight-15 | 26 | 28 | 26 |
| pair-120-same-timestamp | 72 | 72 | 72 |
| pair-120-distinct-timestamp | 81 | 72 | 81 |
| ten-120 | 657 | 663 | 657 |
| eighty-15 | 191 | 206 | 191 |
| bar-single-120 | 48 | 48 | 48 |
| bar-single-15 | 6 | 6 | 6 |
| bar-eighty-15 | 480 | 480 | 480 |

`eight-15` sends eight deltas of 15 at approximately 12 ms intervals.
`ten-120` sends ten deltas of 120 at approximately 100 ms intervals.
`eighty-15` sends eighty deltas of 15 at approximately 12 ms intervals.
Both long sequences total 1200, but represent different event cadences.
Timer dispatch and animation updates are subject to scheduling, so small
cross-platform distance differences should not be interpreted as a precise
performance regression.

During movement, frameSwapped intervals had a median of 7 ms on both live
backends. The per-case p95 was 7 ms on Wayland and 7–8 ms on XWayland.
This measures application swap cadence for the simple probe, not hardware
presentation times, end-to-end input latency or real-folder frame performance.

## Confirmed issues and their relevance

1. **Pending distance is lost when Qt's wheel transition is restarted.**
   Qt resets its timeline and starts the next transition from the current
   position, rather than adding the unfinished previous destination. A single
   delta of 120 reaches 72 px; eight rapid deltas of 15 with the same sum reach
   only 26–28 px. The scrollbar retains the accumulated destination and reaches
   480 px for eighty deltas of 15. This issue also exists on XWayland; it is not
   intrinsically exclusive to Wayland.
2. **Repeated event timestamps are rejected by Qt Flickable.** Two events of
   120 with the same timestamp accept only one event and reach 72 px in this
   probe. Events with different timestamps are accepted, though closely spaced
   transitions can still lose unfinished distance. Whether the real mouse
   produces identical timestamps has not been measured.
3. **Two different wheel curves already exist in the application.** Grid cells
   use Qt's 300 ms / 72 px curve; the scrollbar uses 110 ms / 48 px. This is
   observable without dropped frames and can affect the perceived response.

Both wheel-capable devices listed in `/proc/bus/input/devices` advertise
REL_WHEEL_HI_RES. That establishes hardware capability, not the actual deltas
received by FM. libinput supports fractional v120 events, and Qt Wayland
preserves axis_value120. A different event distribution after changing session
is plausible but has not been proven for the user's mouse.

## Application overhead reviewed

Every contentY change updates preview/hover timers and saves a scroll anchor.
These callbacks also run on X11. The usual current-index anchor path is bounded;
there is no unconditional directory-wide scan in that common path. Reuse is
armed through movement/flick signals, and thumbnail loading is paused while the
active panel scrolls. No measured regression in these real-panel callbacks was
established by the isolated probe. No speculative cache/reuse/delegate changes
were made.

## Next diagnostic step

Record the actual mouse stream over the real grid: event timestamp, angleDelta,
pixelDelta, source and phase, plus contentY samples. Repeat the same input in
Wayland and a native X11 session if available. That determines whether input
fragmentation or timing explains the subjective change. The real-panel prototype below now provides additional correctness evidence
for an accumulated-destination handler. Physical-mouse and subjective review
would still be required before declaring the scrolling feel improved.

## Evidence and reproduction files

Temporary probe source, QML, binary and JSON samples are in
`/tmp/fmqml-grid-wheel-research/`. The copied scrollbar QML and stub Theme are in
`/tmp/fmqml-wheel-probe/qml/`. These are temporary diagnostic artifacts.

Sources inspected:

- [Qt 6.11.2 Flickable implementation](https://raw.githubusercontent.com/qt/qtdeclarative/v6.11.2/src/quick/items/qquickflickable.cpp)
- Installed `qquickflickablebehavior_p.h`: the proportional-scrolling threshold
  is 14999, below the default wheelDeceleration of 15000.
- [Qt 6.11.2 Wayland input implementation](https://raw.githubusercontent.com/qt/qtbase/v6.11.2/src/plugins/platforms/wayland/qwaylandinputdevice.cpp)
- [libinput wheel/v120 documentation](https://wayland.freedesktop.org/libinput/doc/latest/wheel-api.html)

## Phase 2: real-panel verification and diagnostic prototype

The follow-up request explicitly allowed investigating a general Qt issue,
without requiring proof that it first appeared on Wayland.

A temporary four-line insertion in `NavigationBenchmark.cpp` called a diagnostic
scenario from the existing GUI benchmark. It navigated the real FilePanel to the
standard 300-PNG fixture, exercising DirectoryModel, actual Grid delegates,
thumbnail bindings, scroll/hover timers and guarded reuse. Settings, cache and
application data were isolated under `/tmp/fmqml-grid-wheel-research/full-panel/`.
The diagnostic insertion was removed after the experiment and the normal app
was rebuilt. At the end of phase 2, no production scrolling change remained.

A temporary QML overlay evaluated three routes:

- Mode 0: the existing Qt GridView wheel handler.
- Mode 1: accumulated destination, with the same 72 px per full detent and
  300 ms OutExpo curve as the baseline.
- Mode 2: mode 1, additionally discarding the unfinished old destination when
  the wheel reverses direction. This starts the reverse movement from the
  current position immediately.

The prototype reused `handleScrollbarWheelActiveChanged()` to retain the
panel's guarded reuse and scroll/thumbnail-pause lifecycle. It cancelled on
folder changes. It was deliberately a vertical mouse-wheel experiment, not a
complete production handler for all pointing devices/modifiers/interactions.
Each route had a named warmup input before the measured cases. Case reset was
allowed 450 ms to settle, with no pending current-index or scroll restore.
Native Wayland and XWayland were measured separately. The viewport heights
varied slightly between backends, so timing differences are informational.

Final displacement, in pixels:

| Input | Qt Wayland | Prototype mode 2 Wayland | Qt XWayland | Prototype mode 2 XWayland |
| --- | ---: | ---: | ---: | ---: |
| single-120 | 72 | 72 | 72 | 72 |
| eight-15 | 26 | 72 | 27 | 72 |
| ten-120 | 656 | 720 | 652 | 720 |
| eighty-15 | 185 | 720 | 205 | 720 |
| same-timestamp | 72 | 144 | 72 | 144 |

The ordinary ten-detent case loses about 9% of requested movement with the
existing Qt route. The much faster fragmented input loses approximately
71–74%. Both input sequences sum to 1200. The prototype reaches the expected
720 px on both backends without changing the single-detent step or curve.
This confirms an input-cadence dependency in the real panel; it does not prove
a change in the user's physical mouse stream or subjective smoothness.

### Reversal and panel-state checks

For eight deltas of +120 followed immediately by -120, the naive accumulator
still moves **forward** after the reverse input: approximately +63 px over the
next 40 ms on Wayland. The direction-aware variant moves **backward** by about
45 px instead, matching the immediate direction response of the native route.
Therefore simply accumulating destinations without handling reversal is not a
suitable fix.

For measured mode-2 cases on both backends:

- Selected paths and current index stayed unchanged.
- Scroll pause and guarded reuse were observed during motion.
- Thumbnail loading resumed after the animation/idle period.
- Starting 10 px from either end clamped exactly to that end.
- Navigating from Photos to Documents during animation cancelled the prototype;
  the new model had 250 entries and its scroll position remained stable.

The existing Qt route kept `moving=true` at the lower bound after 650 ms in the
Wayland case; thumbnail pause had cleared after an additional one-second wait.
This was a transient state rather than a permanently stuck pause. It is a
secondary observation, not evidence of the user's reported subtle regression.

FrameSwapped p95 values in the real-panel experiment were approximately
7–10 ms. These do not establish a compositor presentation-time improvement;
the prototype's demonstrated benefit is retention of requested movement.

Raw full-panel reports and logs:
`/tmp/fmqml-grid-wheel-research/full-panel-wayland.{json,log}` and
`/tmp/fmqml-grid-wheel-research/full-panel-xwayland.{json,log}`.
Temporary source is in `FullPanelResearch.h` and `accumulated.qml` in the same
directory. The prototype has not been shipped or judged subjectively.

## Phase 3: bounded implementation and regression evidence

The user subsequently authorized a small fix with factual verification. The
new `FilePanelGridWheelHandler.qml` applies to Linux folder GridView only.
It accumulates the unfinished vertical angle-wheel destination, preserving the
measured baseline step (`wheelScrollLines * 24` per detent) and 300 ms OutExpo
curve. On direction reversal it starts a fresh destination at the current
position. Bounds include `originY`. Mouse buttons pass through the wheel
surface. Pixel gestures stop its animation immediately and are rejected so
native Flickable handles them. Native movement also cancels pending animation.

The handler uses the existing guarded wheel-scroll lifecycle. Reuse invalidation
cancels it for navigation, selection/model changes and other existing invalidation
routes. It also stops on grid delegate press (including an already-selected
entry), scrollbar press, and disablement through the existing motion gates.
List/Brief and the scrollbar's existing 110 ms curve are outside this slice.

### Permanent regression test

`file_panel_grid_wheel_test` loads the real QML component into a GridView and
sends Qt input events. It checks single/repeated/fractional detents, equal
timestamps, immediate reversal, both boundaries, disabling, native flick
cancellation, Begin/Update/Update/End pixel-gesture handoff and click delivery.
The same test with `--native-baseline` disables the new surface and fails at
`fragmented detent lost pending distance`, establishing regression sensitivity.
The gesture fixture requires multiple updates to initiate native dragging;
a single update does not constitute an adequate handoff check.

### Real panel: before and after

The temporary full-panel scenario was adapted to exercise the actual production
handler rather than the phase-2 prototype. A fresh Wayland native-control run
used the same panel with its wheel surface disabled.

| Input | Native Qt Wayland control | Fixed Wayland | Fixed XWayland |
| --- | ---: | ---: | ---: |
| single-120 | 72 px | 72 px | 72 px |
| eight-15 | 25 px | 72 px | 72 px |
| ten-120 | 645 px | 720 px | 720 px |
| eighty-15 | 188 px | 720 px | 720 px |
| same-timestamp, two detents | 72 px | 144 px | 144 px |

The normal-control distance varies slightly between runs (phase 2 recorded
652–656 px for ten detents), consistent with its dependence on event cadence.
The fixed endpoint retains the full requested sum in these tests.
Selection/current index, scroll pause, guarded reuse and thumbnail resume checks
passed. The direction after reversal was negative over the next 40 ms, both
bounds were respected, and navigation cancelled the active animation.
Pressing an already-selected visible delegate stopped the animation without
changing selected paths or later drifting the viewport.

### Diagnostic failure and limits

One expanded Wayland run crashed in `QSGNode::destroy()` on QSGRenderThread
while navigating after the selected-delegate press. The core and stack trace
were retained in `/tmp/fmqml-grid-wheel-research/`. Inspection found a lifetime
error in the temporary frame collector: disconnecting a queued frameSwapped
connection does not guarantee that pending callbacks cannot access expired
scenario-local references. The collector was changed to use a per-scenario
QObject context; a separate alive guard was also used while investigating.
No stale-callback marker was observed in the follow-up runs. Thus that collector
error was real, but its causal connection to the single render-thread crash
has **not** been established.

The native-control and fixed follow-up runs passed. Five additional consecutive
wheel/selected-press/navigation cycles passed with the fix, and five passed
with native Qt. The crash has not reproduced in those follow-ups. It is not
reported as conclusively diagnosed or fixed. The temporary collector and all
benchmark entry-point insertions are removed from the final production source.

This validates input-distance retention and the tested cancellation contracts.
It does not prove subjective smoothness, physical mouse event distribution,
hardware presentation timing, or all desktop/Qt configurations. Manual mouse
review in the normal live application remains necessary.

Final evidence artifacts are `fixed-panel-wayland.{json,log}`,
`fixed-panel-xwayland.{json,log}`, `native-control-wayland.{json,log}`,
`fixed-stress-wayland.{json,log}` and `native-stress-wayland.{json,log}` in
`/tmp/fmqml-grid-wheel-research/`.

Final clean-source validation: `cmake --build build -j 12` passed.
`ctest --test-dir build --output-on-failure -R
'^(file_panel_grid_wheel_test|navigation_gui_benchmark_smoke)$'` passed
both tests (5.34 s and 8.20 s). `git diff --check` passed.
`src/tools/NavigationBenchmark.cpp` has no remaining diff. The final XWayland
run also passed selected-click/navigation cancellation and all lifecycle checks.

### Follow-up: slightly slower wheel distance

After manual review confirmed the improved feel, the user requested slightly
lower speed. The distance multiplier was reduced from 24 to 20 pixels per
configured wheel line (72 to 60 pixels per detent with three lines, a 16.7%
reduction). The 300 ms OutExpo animation and accumulation rules are unchanged.
The measurements above describe the original 24-pixel multiplier. The regression
test now expects the 20-pixel multiplier; its native-baseline mode consequently
fails at the single-detent check because native Qt still uses 24.

## Phase 4: common wheel behavior in Grid, Brief and Detailed

After the user accepted the slower Grid setting, they requested the same speed
and smoothness in all three file views. `FilePanelGridWheelHandler` was renamed
to `FilePanelWheelHandler` and attached to the Detailed ListView and Brief
GridView as well as the icon GridView. All three use 20 pixels per configured
wheel line, 300 ms OutExpo, accumulated unfinished distance and reversal reset.
The shared bounds now include top/bottom margins, so Detailed/Brief can reach
the full content above bottom chrome.

The three vertical FmScrollBars delegate angle-wheel input to the same handler.
Their pixel-input route is preserved; view-area pixel gestures still pass to
native Flickable. Scrollbar presses cancel the shared animation through the
existing panel callback. Detailed/Brief delegate presses also cancel it,
including clicks on an already-selected entry. The existing Linux scope and
reuse/motion gates remain in place.

### Verification

The permanent test now runs against Grid, a two-column Brief fixture and a
Detailed ListView fixture. All three passed step, fractional/repeated/equal-time
input, reversal, bounds including explicit margins, cancellation, gesture
handoff and click delivery checks (6.23 seconds each).

Temporary real-panel instrumentation exercised actual file delegates and the
production handler under Wayland, over both content and the vertical scrollbar.
All six runs passed these measured endpoints:

| Input | Detailed | Grid | Brief |
| --- | ---: | ---: | ---: |
| single detent | 60 px | 60 px | 60 px |
| eight fractional deltas of 15 | 60 px | 60 px | 60 px |
| ten full detents | 600 px | 600 px | 600 px |
| eighty fractional deltas of 15 | 600 px | 600 px | 600 px |
| two detents with equal timestamp | 120 px | 120 px | 120 px |

The endpoints were identical over content and scrollbar. Selection, guarded
reuse, scroll pause/resume, both bounds, selected-entry click cancellation and
navigation cancellation also passed in every run. The curve is defined by the
same shared animation; timing samples depend on frame/event-loop cadence and
are not expected to match at every wall-clock instant. Subjective comparison
still requires normal-app manual review. No crashes occurred in these six runs.

Artifacts: `/tmp/fmqml-grid-wheel-research/common-wayland-{0,1,2}-{content,scrollbar}.{json,log}`;
mode 0 is Detailed, 1 is Grid, 2 is Brief. This phase uses automated Qt input
in the live app, not an observed physical mouse event stream.

Three additional real-panel XWayland content-area runs passed the same endpoints,
selection/reuse/pause-resume and selected-click/navigation cancellation checks.
Artifacts: `common-xwayland-{0,1,2}-content.{json,log}` in the same directory.
XWayland is not a native X11 desktop session. No crashes occurred in these runs.
The temporary NavigationBenchmark insertion was removed; the final ordinary
`cmake --build build -j 12` and `git diff --check` passed.
Final clean-build CTest run passed all four relevant tests: Grid, Brief, Detailed
wheel regressions and `navigation_gui_benchmark_smoke` (14.30 seconds total).
