# Qt 6.12 wheel scrolling regression

Date: 2026-10-14. Local Qt: 6.12.0.

## Reproduction and cause

The file-panel wheel handler rejected any event carrying pixel deltas. A
NoScrollPhase event with angleDelta.y=-120 and pixelDelta.y=-15 therefore
bypassed the accumulated-destination animation used by Grid, Brief and Detailed.
The existing wheel test only sent angle deltas and did not cover this route.

The shared scrollbar and document viewer preferred pixel deltas. In a fixture
loading their real QML and native visuals, angle-only events moved 48 px, while
otherwise identical dual-delta events moved 6 px. The panel scrollbar moved
60 px with angle-only input but 6 px with dual-delta input. Horizontal scrollbar
tests use vertical wheel rotation over the bar, as in the existing UI route.

Both the new framework test and the dual-delta panel test were run against the
original production QML and failed before the fix. Temporary baseline file
substitutions were restored immediately after measurement.

## Change and coverage

Match the existing Map fix: NoScrollPhase events with nonzero rotation use
angle deltas even when pixel deltas are present. Phased and pixel-only gestures
keep their existing pixel/native handling. Scroll distances and animation
durations are unchanged.

The panel regression runs Grid, Brief and Detailed with both input formats,
including fractional/repeated detents, equal timestamps, reversal, bounds,
cancellation, phased gesture handoff and delegate clicks. The framework test
covers standalone vertical/horizontal scrollbars, panel scrollbar forwarding
and document scrolling, including phased and pixel-only input. The real-panel
arrow tests also send dual-delta events over the scrollbar before checking
animation cancellation.

Validation: the full application build succeeded; all seven wheel tests passed,
as did the three real-panel scrollbar-arrow tests, GUI navigation smoke test and
document visual test (12 focused checks total). `git diff --check` passed.

These are synthetic Qt input checks. They establish the routing/distance
regression and its correction, not frame performance in ~/Wallpapers or the
subjective feel of the physical mouse. Those still need live confirmation.
