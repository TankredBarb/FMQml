# Hidden-page work during panel resize

Date: 2026-10-01. After the transition-pause improvements, the user found entry
and exit comfortable. The next priority was Grid movement, followed by Detailed.

## Confirmed cause and fix

StorageView and FavoritesView remained anchored to contentArea even when both
pages were hidden in an ordinary folder. Every divider step consequently changed
the widths of device/quick-access cards and favorite rows.

A separate Grid QML profile covering the last 2.5 seconds of movement showed:

| File | Profile ranges before → after | Self time before → after, ms |
|---|---|---|
| StorageDriveGrid.qml | 7157 → 0 | 81.47 → 0 |
| QuickAccessGrid.qml | 13742 → 0 | 59.49 → 0 |
| FavoritesRowDelegate.qml | 16860 → 0 | 10.78 → 0 |

Range counts include nested binding/JavaScript events, not independent calls.
Self time subtracts directly nested ranges; no partial overlaps were found.
Qt instrumentation affects this profile, which identifies work rather than
measuring ordinary smoothness. Raw XML remains in /tmp;
[the summary and normal measurements](resize-hidden-views-results.json) are
preserved in the repository.

The fix consists of two conditional anchors.fill bindings in FilePanel:
`visible ? parent : null`. Hidden pages stop following folder geometry. Opening
devices or favorites restores their anchors. Objects, models, and page state are
retained. Thumbnail suppression and the accepted transition-pause fix are unchanged.

## Comparison without profiling

Two independent Wayland processes per view before and after. Window: 1800×900;
300 PNG files; identical three-second central-divider trajectory. Steady-state
statistics exclude the first 500 ms. Temporary XDG_CONFIG_HOME isolates settings;
data and OS caches are retained.

| View | Whole-drag p95 before → after, ms | p95 after the first 500 ms before → after, ms |
|---|---|---|
| Grid | 14.25–15.19 → 10.61–11.12 | 12.86–13.63 → 7.96–8.41 |
| Detailed | 13.59–16.27 → 8.00–9.28 | 12.58–15.15 → 7.85–8.67 |
| Brief | 8.63–8.64 → 7.14–7.19 | 8.13–8.33 → 7.10–7.10 |

All 12 runs completed resize correctly and restored cache and thumbnails.
Measurements describe Qt frameSwapped intervals, not physical presentation.
Steady-state maxima after the fix still reached 24–30 ms. Guaranteed 144 FPS and
complete removal of stutter have not been established.

## Regression validation

The real GUI benchmark now checks that hidden-page width remains constant during
folder resize. Before the fix this check failed while the other acceptance
criteria passed. After the fix it passed. The same scenario opens devices:// and
favorites://, checks page width/height on opening and width following during
another resize, then returns to the folder.

The final build, all eight selected CTest cases (78.81 seconds), and git diff
--check passed. Temporary QML debugger/console.profile instrumentation was
removed; the final build is normal. Live Grid/Detailed evaluation was initially
pending; the user subsequently confirmed good progress, with Brief still smoothest.
Grid relayout and hidden hover-surface calculations remain visible in the profile
and were not changed in this fix.
