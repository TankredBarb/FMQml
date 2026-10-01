# Small Grid and Detailed resize follow-up

Date: 2026-10-01. The user confirmed good progress after the hidden-page fix,
while Brief remained smoothest. This pass tested two narrow controls rather
than changing accepted resize behavior speculatively.

## Controls and measurements

The existing post-fix Grid profile still showed hover geometry and backdrop
sourceRect calculations. Two controls were tested independently:

1. Return an empty TranslucentSurface sourceRect while the surface is inactive
   or blur is disabled, matching its already-null sourceItem.
2. Stop forwarding the panel anchor rectangle to invisible file/folder hover
   cards. Restore the original TranslucentSurface before this second control.

Each cohort used two independent Wayland processes per view, a 1800×900 window,
300 PNGs, and the same three-second central-divider trajectory. Settings were
isolated; data/OS caches were retained. Steady-state intervals exclude the first
500 ms. These are Qt frameSwapped measurements, not physical presentation.

| View and cohort | Steady p95, ms | Steady p99, ms | Steady maximum, ms |
|---|---|---|---|
| Detailed baseline | 7.90–8.19 | 12.56–14.17 | 28.95–29.90 |
| Detailed inactive-backdrop control | 7.81–8.00 | 14.96–15.35 | 27.61–29.63 |
| Detailed hidden-hover-anchor control | 7.92–8.78 | 13.95–16.57 | 29.27–32.37 |
| Grid baseline | 8.00–10.55 | 19.22–20.04 | 24.78–25.73 |
| Grid inactive-backdrop control | 9.55–10.65 | 20.37–21.11 | 26.39–26.59 |
| Grid hidden-hover-anchor control | 9.06–10.38 | 20.21–20.58 | 25.45–29.46 |

All 12 runs passed lifecycle, following, final geometry, selection, and recovery
checks. Neither control produced a consistent improvement in p95 or long-frame
tails. Both were reverted. The small sample does not establish exact equivalence,
but does not justify shipping either change. [Raw results](resize-follow-up-results.json).

## Remaining leads

Grid freezes cellWidth but still changes viewport width and column count. New
items created during resize contain both the full Grid delegate and its
lightweight surface. The preceding post-fix profile recorded 154 full-root and
154 resize-surface creation ranges in its last 2.5 seconds. Those observations
survived the hidden-page fix; they do not by themselves attribute individual
long frames to creation. [Profile summary](resize-hidden-views-results.json).

Detailed freezes effective column widths and the hidden full delegate size.
Its lightweight FileTableResizeDelegate still follows row width and contains
nested RowLayouts with text elision. Brief freezes lightweight row width and
keeps the single-column layout during resize. These structural differences are
plausible remaining contributors, not isolated timing results.

The next focused measurement should correlate Grid column changes and delegate
creation/destruction with per-frame layout cost and long-frame timing. Detailed
needs separate attribution of lightweight-row layout versus synchronization.
Previous controls already failed to justify freezing the whole Detailed resize
surface or changing Grid cell width to retain a fixed column count. Repeating
those changes without new evidence is not warranted.

## Documentation language and final state

The plan, progress report, and hidden-page research were translated to English,
preserving measurements, links, and checklist status. All task Markdown/SVG files
and decoded strings in JSON artifacts were checked for Cyrillic text. New task
documentation and annotations must remain in English.

The accepted production behavior is unchanged by this follow-up. Both temporary
controls were removed and the normal build restored.
