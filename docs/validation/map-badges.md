# Map status badges

Map now displays the same primary local status as the other file views:
broken link, link, mount point, locked, archive, in that priority order. A pinned
favorite is independent and can coexist with the primary badge. Both files and
folders are covered, including nested branches. Status colors and glyphs follow
the shared FileIconCell palette and SVG assets.

Data and rendering costs are kept separate:

- Root children reuse primaryBadgeKind from the directory model snapshot, with
  no new per-child metadata checks. The current root's status is resolved once
  when its tree is initialized.
- Nested listings resolve badges alongside their existing background metadata
  scan through LocalFileBadgeResolver. This includes an access check per entry
  and target checks for symlinks. Existing lazy loading and listing limits apply.
- Status is cached as an integer in each entry. Rendering does no filesystem I/O,
  SVG parsing, favorites-store reads, or per-entry texture uploads.
- Favorites publish their existing path snapshot as a QML property. Map keeps
  a hash set and reacts to changes without reloading directory listings.
- Six glyphs and their circular backgrounds share a 432 x 72 RGBA atlas:
  124,416 bytes before GPU mipmaps. One textured geometry node batches all badges
  in each Map. There are no per-entry QML objects or textures. The atlas is
  rebuilt only after palette changes, outside the frame rendering function.
- Folder headers reserve room for status glyphs. Small tiles separate them from
  the type icon; file thumbnails retain overlays. Entries too small to display
  their type icon do not acquire floating badges.

Automated coverage:

- FolderMapTest checks real temporary locked entries, symlinks, broken links,
  mount-point classification, archives, cached root metadata and live pin changes.
- The native GPU scenario is `fm --navigation-gui-benchmark --map-badges`.
  It prepares 512 archives, pins them through the real favorites controller and
  alternates disabled/enabled/enabled/disabled atlas rendering on one moving Map.
  Warmup frames are excluded. It records frame intervals, render-thread sync
  time and the actual number of badge quads in the Scene Graph. Disabling the
  atlas is test instrumentation only, not an application preference.
- The same scenario checks real folder/file warnings, pins and a thumbnail,
  changes light/dark palettes, saves screenshots, then verifies live unpinning.
  Test changes to favorites, theme, thumbnail settings and file permissions are
  restored. All generated files are temporary.

Logs and screenshots:
`/tmp/fmqml-map-badges-build.log`, `/tmp/fmqml-map-badges-unit.log`,
`/tmp/fmqml-map-badges-gpu.log`, `/tmp/fmqml-map-badges-tests.log`,
`/tmp/fmqml-map-badges-dense.png`, `/tmp/fmqml-map-badges-light.png`,
`/tmp/fmqml-map-badges-dark.png`.

Frame cadence at the display's refresh limit does not establish zero cost.
The synchronization timings quantify CPU-side work; they do not measure GPU
execution time. Results from this RX 6600 XT workstation do not establish the
same timings on other machines or network filesystems.

Final isolated GPU run (Qt 6.12, RX 6600 XT, 239 measured frames per round):

| Rendering | Badge quads | Frame p50, ms | Frame p95, ms | Sync p50, ms |
| --- | ---: | ---: | ---: | ---: |
| Disabled, first | 0 | 6.939 | 7.143 | 1.326 |
| Enabled, first | 836 | 6.942 | 7.155 | 2.114 |
| Enabled, second | 836 | 6.941 | 7.105 | 2.065 |
| Disabled, second | 0 | 6.942 | 7.069 | 1.318 |

The added median synchronization work is about 0.77 ms on this deliberately
heavily badged scene. Frame cadence remains near 144 Hz. There were no listing
reloads during these rounds. A complete nested listing of 512 entries, including
badge resolution and sorting, took 3.04 ms; this is not an isolated measurement
of the badge metadata overhead. Light/dark examples and live unpinning passed.

Final build, all 93 CTest targets (75.71 s), the focused 32-case Map unit run,
and git diff --check passed. Added source lines contain no Cyrillic.

## Badge color binding follow-up

The four badge color properties originally shared paletteChanged. A foreground
binding that reads badgeColor was therefore notified by its own foreground
write, causing a QML binding loop. Each property now has its own notification.
Only badge color notifications invalidate the badge atlas; ordinary Map palette
changes no longer rebuild it.

The regression binds both foregrounds to their backgrounds through QML, changes
the backgrounds, and captures QQmlEngine warnings. It failed with binding-loop
warnings before the fix and passed afterward. It also checks that changing the
folder color leaves the badge atlas clean. The native badge scenario now fails
if FilePanelMapView reports a binding loop during its theme checks.

The focused Map/software/hover CTest run passed 3/3 in 18.50 s.
Logs: `/tmp/fmqml-badge-binding-before.log`,
`/tmp/fmqml-badge-binding-after.log`, `/tmp/fmqml-badge-binding-focused.log`,
and `/tmp/fmqml-badge-binding-gpu.log`.
The final native GPU/theme scenario passed with badgeBindingLoops=[] and
examples=true. The build and git diff --check passed.

## File icon zoom follow-up

File icons previously used only 80% of their available square and stopped growing
at 64 screen pixels. They now fill the available area above the filename while
preserving their aspect ratio. Folder headers remain container headers. The shared
type atlas uses 128-pixel source icons instead of 64-pixel sources for larger zoom
levels; this adds about 1.02 MiB of RGBA storage per atlas before GPU mipmaps and
does not add textures or draw calls per entry.

The native GPU test reads the actual icon quads after Scene Graph synchronization
at two zoom levels. In the final run, tile heights of 237.13 and 352.94 pixels
produced icons of 213.13 and 328.94 pixels, filling the area reserved above the
filename. Both exceed the former cap and the second grows with zoom. Screenshots
`/tmp/fmqml-map-icon-zoom-1.png` and `/tmp/fmqml-map-icon-zoom-2.png` were inspected.
The native and bundled atlas comparisons now validate 128-pixel source rendering.

Build and 3/3 focused Map/software/startup CTest targets passed (12.29 s).
The native GPU scenario passed with iconFillsTile=true, examples=true and no
binding loops. Frame p50 remained around 6.94 ms on the same GPU. Logs:
`/tmp/fmqml-map-icon-size-build.log`, `/tmp/fmqml-map-icon-size-tests.log`,
`/tmp/fmqml-map-icon-size-gpu.log`.

## Folder header badge zoom

Folder header badges no longer stay at 12 screen pixels while their header grows.
They use half the header height, bounded to 12-64 pixels and constrained by the
available header width. Text reserves the corresponding space. File overlay badge
dimensions are unchanged. The shared glyph atlas now uses 64-pixel source badges
to keep the enlarged header badges crisp; this adds 84 KiB of RGBA storage before
GPU mipmaps, with no additional textures, draw calls or file reads.

The native test measures the actual badge geometry after synchronization at three
zoom levels. Header heights 47.28, 70.37 and 104.74 pixels produced badges of
23.64, 35.18 and 52.37 pixels. headerBadgeScales=true, examples=true and no binding
loops were reported. The final screenshot
`/tmp/fmqml-map-header-badge-2.png` was inspected. Frame p50 remained around 6.94 ms.

Build, git diff --check and 3/3 focused Map/software/startup tests passed (12.23 s).
Logs: `/tmp/fmqml-map-header-badge-build.log`,
`/tmp/fmqml-map-header-badge-tests.log`, `/tmp/fmqml-map-header-badge-gpu.log`.
