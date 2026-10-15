# Details hover hit-area geometry

Date: 2026-10-14. Qt 6.12.0, native Wayland.

The original hover regression targeted the hit area's own center. This could
pass even when that area no longer covered the rendered icon and name.

A read-only snapshot of the failing normal application showed enabled hover
handlers and no suppression, but the Details hit areas were only 24–28 px wide
and positioned left of the visible names. The derived `mapToItem()` bindings
retained coordinates from an earlier layout. A fresh diagnostic launch did not
consistently expose the same initial layout state.

`FileTableDelegate.qml` now explicitly binds these coordinates to the name row,
icon, text row and label positions. The full/lightweight delegate lifecycle,
frozen resize geometry, caches and resize gates are unchanged.

The regression now targets rendered text and compares exact hit-area bounds
with the actual icon/name positions after toggling selection-badge spacing.
It fails against the previous delegate (for example, hit bounds `[0,266]`
versus content bounds `[24,216]`). Media and folder cards and view-change
clearing remain covered.

## Resize smoothness check

The existing native Wayland Details benchmark ran two divider-drag repetitions
before and after the fix, using the same profile, 300-image fixture, 1800×900
window and approximately 144 Hz output. No compilation/tests ran concurrently.

| Measurement (ms), runs 1 / 2 | Before | After |
| --- | --- | --- |
| Frame interval p95 | 7.29 / 8.01 | 7.41 / 7.96 |
| Frame interval maximum | 29.66 / 24.42 | 31.34 / 32.93 |
| Steady drag interval p95 | 7.19 / 7.66 | 7.41 / 7.55 |
| Synchronization p95 | 1.71 / 1.77 | 1.64 / 1.65 |

Both reports passed. Both panels still entered the lightweight resize path.
The p95 measurements are comparable; isolated maxima varied. Two repetitions
do not establish absence of every hitch. These are application frameSwapped
intervals, not physical presentation/input-latency measurements.

Raw reports: `/tmp/fmqml-hover-inspect/resize-{before,after}.json`.

The application build and six focused checks passed: hover preview, GUI
navigation smoke, Grid/Brief/Details resize geometry and wide Brief resizing.
`git diff --check` passed. The corrected normal-launch hover still requires
the user's live confirmation; earlier diagnostic-launch success was insufficient.
