# Review of pending scroll and resize changes

Date: 2026-10-01. Scope: all 36 files in git status at review entry: 20 source,
build, benchmark, runner, and test files, plus 16 research artifacts. Production
sources and the normal executable were not modified during the initial review.
All three findings below were subsequently fixed and verified; their original
evidence is retained for traceability.

## Original findings

### P2: Scrollbar arrows leave the shared wheel animation running

Locations: `qml/components/framework/FmScrollBar.qml:55` and
`qml/components/filepanel/FilePanelWheelHandler.qml:45`.

After a wheel event in the file viewport, pressing a scrollbar arrow changes the
position but does not cancel the shared handler's NumberAnimation. The arrow's
MouseArea calls increase/decrease without setting the ScrollBar's pressed flag,
so FilePanel's existing pressed-change cancellation does not run. The animation
then writes the old wheel destination over the arrow's result.

A diagnostic run of the actual Grid panel recorded contentY 318.6 before the
click, 683.4 immediately after it, and 360.0 after the pending wheel animation.
The handler was still active after the click. With only the shared handler
disabled, native viewport scrolling preserved the arrow result: 322.3 -> 687.1
-> 687.1. Brief and Detailed also retained the shared animation after arrow
clicks. In Detailed, a small arrow step can instead be overtaken by the remaining
wheel distance rather than reversed.

Cancel pending shared wheel motion before both arrow actions, including repeats.
Add a consumer-level test using the real scrollbar and shared handler; the
current isolated wheel fixture does not cover this interaction.

### P2: A narrow Brief panel can regain multiple columns during resize

Location: `qml/components/FilePanel.qml:3487`.

The frozen cellWidth equals the viewport width at entry. FlowLeftToRight GridView
can therefore fit multiple cells after the viewport grows to twice that width.
This defeats the intended single-column lightweight Brief mode and introduces
new wrapping and population changes during the gesture.

The actual FM probe began with a 262 px Brief viewport. While the resize was
still active, the viewport crossed 531 px with cellWidth still 262 px. After
polish, item 1 was at x=262, y=0 rather than in the following row. A positive-only
620 px divider trajectory stayed within panel limits and passed the existing
benchmark gates despite this behavior. Existing tests begin with equal panels
and use only +/-180 px motion, so they miss the transition.

Preserve single-column placement independently of frozen content geometry.
Add a narrow-to-wide resize case and verify actual item placement, rather than
only checking that cellWidth stayed constant.

### P2: Grid recovery assertion includes unfinished cache delegates

Location: `src/tools/PanelResizeBenchmark.cpp:447`.

The new geometry/hit-test loop examines every contentItem child with the delegate
property. It can include a cache delegate that Qt has started creating but has
not completed. The assertion then requires its full Loader item and final size
immediately after a fixed 600 ms delay.

This produced a false rejection in the full CTest run and in repeated diagnostic
runs. A failing object had index 175, size 0x0, and no loaded full layout. The
other recovery, selection, final-geometry, rename, and virtual-page gates passed.
One four-repetition diagnostic series passed repetitions 0, 2, and 3 and failed
only repetition 1 at this assertion. A size-zero construction stub is not proof
of a rendered Grid defect.

Check completed viewport delegates and use bounded readiness waiting where
necessary. Treat outstanding cache construction separately; do not require every
partially instantiated child to already support full-layout hit testing.

## Validation and limits

The full suite in the filesystem/network sandbox passed 72 of 75 tests in
26.74 seconds. AppSettingsControllerTest could not persist its isolated settings;
GDriveThumbnailNetworkTest could not bind its loopback server. Both passed when
rerun outside the sandbox (5.57 seconds combined). The remaining Grid geometry
failure is the reproducible assertion issue above. Thus the suite is not fully
green as written, even though it has passed in earlier individual runs.

MEGA account-details, public-link/provider, controller, and all three wheel tests
passed. Source review confirmed that the account session already registers the
global SDK listener and that the storage refresh does not require the removed
authorization notification. No additional MEGA finding was established.

Diagnostic scenarios used a separate executable linked from the current build's
objects with only temporary benchmark instrumentation. Production QML and C++
behavior were unchanged. The narrow Brief scenario and scrollbar interaction
used the actual FM panels and public QTest/QWheelEvent input. Offscreen execution
establishes these state/geometry failures, not physical Wayland presentation.

All task JSON files parsed, both SVG files parsed as XML, and decoded artifact
strings contained no Cyrillic text, email addresses, or credential values under
the checks used. git diff --check passed. Existing research reports describe
small-cohort timing and distinguish Qt frameSwapped from physical presentation.

No P1 issue was established in this review. This is not a guarantee of absence:
native X11, Windows, real touch gestures, and compositor cancellation remain
outside these runtime checks. The three reproducible P2 findings were resolved
in the follow-up below.


## Fixes and regression verification

All three findings are closed:

- Scrollbar arrows cancel the shared wheel handler before each initial action
  and repeat. Real-panel tests cover both directions and held arrows in Grid,
  Brief, and Detailed, and verify that the old wheel destination cannot overwrite
  the arrow result.
- Lightweight Brief cells follow the current viewport width, preserving one
  column as a narrow panel expands. The inner resize RowLayout retains its
  entry width, so this placement correction preserves the frozen content
  geometry. A new narrow-to-wide trajectory checks actual delegate placement
  as well as the frozen content width and normal recovery.
- Grid recovery checks every expected viewport index through itemAtIndex,
  including loaded layout geometry and icon hit testing. Readiness retries are
  bounded to one second. Unfinished offscreen cache children are excluded, while
  missing visible content still fails: the test temporarily disables a visible
  delegate's full Loader, verifies rejection, restores it, and checks recovery.

The four new regression cases all failed on the original production code
(17.91 seconds). After the fixes, seven focused cases passed (31.97 seconds).
The final build succeeded and the complete CTest suite passed 79/79 tests in
44.07 seconds outside the sandbox, allowing isolated settings persistence and
loopback server tests. The formerly intermittent Grid geometry case also passed
four consecutive repetitions (69.76 seconds).

Two independent Wayland Brief resize smoke runs passed their correctness gates.
Steady Qt frame interval p95 was 7.27 and 7.37 ms; p99 was 13.78 and 14.70 ms.
These runs are a small post-fix cohort, not a randomized before/after comparison
or proof of physical presentation at 144 FPS. Entry and recovery pauses remain
separate from steady movement. Raw reports and test outcomes are archived in
[review-fixes-results.json](review-fixes-results.json).

All task documentation and artifact annotations remain in English. No commit
was made. Real-mouse confirmation and native X11 validation remain separate
from the automated results.
