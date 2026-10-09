# GPU folder map

Choose **Map** with Ctrl+4 or in the panel's view menu, or cycle through Details, Grid, Brief
and Map with the toolbar view button. Map supports local folders and browsable
local archives when the archive backend is available.

- Wheel: smoothly zoom toward the pointer.
- Drag: pan; middle/right drag and Space+drag also pan.
- Click: select an entry, including one in a nested folder, without changing the panel path.
- Ctrl-click: add or remove an entry from the selection, including nested entries.
- Shift-click: extend or trim a sibling range from the first entry in display order, regardless of the prior selection or focus, matching the other views. Disjoint selections are preserved.
- Arrow keys, Home/End: move among siblings; Shift extends selection, Ctrl moves focus without replacing selection. Ctrl+Space toggles the focused entry.
- Double-click: enter a directory or open a file.
- Right-click: select the entry and open its shared file actions menu without changing the panel path; copy and cut/paste use the panel clipboard actions.
- Incoming drops copy into the current panel folder. Dragging within Map pans; it does not start a file drag.
- F or 0: fit the current folder.
- F2: rename one selected item in a themed dialog, or open Batch Rename for multiple items, including nested entries.
The focused entry uses the panel controller's `currentItemPath`, including nested
entries without a row in the current directory model. Preview pane and Quick Look
use this path, and deferred row initialization preserves it. Removed entries are
cleared from focus and selection when the map listing changes.
Nested selections participate in the panel's file actions and are cleared when
leaving Map or changing the panel path. Once an empty folder is large enough,
its interior shows the same folder illustration and message as the other views.

Selection uses a two-pixel GPU outline drawn over the tiles. Map preserves the
camera scale when selection moves into an already loaded folder, when panel
selection is restored, and when the panel is resized or briefly has no size.
Back retains the camera while returning through the loaded hierarchy.
Selecting a breadcrumb fits the target folder to show its overview.
The **Use gradient colors** setting adds theme-derived vertical shading to
folder headers and tiles; turning it off keeps flat fills.

The current panel directory is the map's root. Zooming out fits that directory,
and panning stays within it; ancestors and siblings are not exposed by the camera.
Use Up, Back or the address bar to navigate outside the root. Folder areas
represent structure, not disk usage. The current folder follows the panel model's
sorting and filtering; nested listings use name order with folders first.
Single-item renaming uses the shared dialog shell, input and button styles, including
dialog transparency settings. Enter submits, Escape cancels, and name conflicts
keep the dialog open with an error. Focus, selection and the camera are preserved;
renaming a loaded folder remaps its descendants without collapsing the branch.
Gesture help is available through the small
info button in the bottom right corner.

`MapGpuProbe` checks the active scene graph's QRhi device. Software/Null backends,
CPU devices and known software drivers (including llvmpipe and SwiftShader)
do not render the folder map. Map remains selectable everywhere through the menu,
toolbar or Ctrl+4. Remote providers, virtual locations and unavailable GPU devices
show a themed placeholder with manual Details, Grid and Brief choices. Navigation
and startup never change the selected view. Returning to a supported local folder
resumes the GPU map. There is no CPU map renderer.
Both the panel and toolbar use the same native path capability check.
The native view requires matching Qt GuiPrivate and QuickPrivate development
packages (QRhi probing and Qt Quick asynchronous image requests).

`FolderMapItem` builds one colored triangle batch and uses Qt's GPU distance-field
text nodes. The 16 file-type icons follow `useNativeIcons`: the shared native
`IconProvider` supplies system icons when enabled, otherwise bundled SVGs are used.
Each set is prepared once in a shared worker future and cached in a padded atlas;
split panels share preparation to avoid concurrent native icon cache access. At application
shutdown, pending atlas tasks finish before the GUI application is destroyed,
including early benchmark returns. Changing the
setting replaces the GPU texture after preparation, keeping the old atlas visible
until then. Icons use a single textured batch with mipmap filtering. Icon space is reserved before loading, and small cells
hide details. Text nodes and vertex storage are reused across camera frames.
QPainter only prepares the icon atlas off the UI/render threads; the map is drawn
by the GPU. Directory enumeration and sorting run in workers; at most
two listings are pending per map. Old results are discarded after root changes.
Symlinks expand only when explicitly entered.
Archive opening follows the shared panel controller, including nested-archive
confirmation and password handling; Map does not override ordinary-file launch
behavior. Once an archive is open, its root follows `DirectoryModel`, and nested
folders read the provider's prepared catalogue in the same bounded worker queue.
Zoom does not reopen or extract archive containers. Native filesystem watchers
are only attached to local directories, not `archive://` entries.

The current folder consumes `DirectoryModel` changes and its existing watcher,
without a second directory scan or watcher. Model updates are coalesced over
50 ms, preserving matching loaded branches, selection and camera scale/position.
Removed node slots are reused; stale hit-test geometry cannot select a replacement
file before the next frame. Selection-only changes do not rebuild the tree.
Up to 32 visible loaded nested directories use the existing native directory
watcher backend. Events are coalesced over 75 ms and refresh affected listings
and thumbnails through the bounded worker queues. Returning to an unwatched
branch reattaches its watcher and rescans it. Hiding Map releases its watchers.

Work is bounded to 4,096 entries per listing, 40,000 loaded nodes and 2,048 visible
cells. Dense branches remain an aggregate cell until zoom exposes fewer entries.
Listing limits are reported in the map; List remains the complete listing.

## Verification

```sh
cmake --build build -j 12
XDG_CONFIG_HOME=/tmp/fm-map-tests ctest --test-dir build \
    -R 'folder_map|app_settings_controller_test' --output-on-failure
build/folder_map_test --gpu-smoke
build/folder_map_test --gpu-smoke --bundled-icons
build/folder_map_test --gpu-smoke --without-icons
build/folder_map_test --gpu-smoke --cold-icons
XDG_CONFIG_HOME=/tmp/fm-map-panel XDG_DATA_HOME=/tmp/fm-map-panel-data \
    ./build/fm --navigation-gui-benchmark --map
```

The native tests also launch fresh processes with both icon atlas tasks pending
and exit immediately, checking that GUI shutdown finishes without a crash.

The GPU smoke commands use the same mixed-file hierarchy, wait for icon
preparation before timing, and record 210 zoom frames with the first 30 excluded
from statistics. Alternate icon-on/off runs on the same GPU for a matched
comparison; label space is identical in both cases. The capture is written to
`/tmp/folder-map-gpu.png`. The panel benchmark verifies
the integrated view, live `useNativeIcons` changes in both directions, navigation,
root boundaries and GPU/path capabilities while keeping the toolbar's Map choice
available everywhere. It checks the virtual-location placeholder, its manual view
button and Ctrl+4 on GPU runs, plus returning to the GPU map without a mode change.
It also checks real clipboard
copy and cut/paste, incoming window drag/drop, watcher-driven rename/removal,
click selection and camera preservation. It inserts a sorted sibling before the
focused folder and checks that Enter still opens that folder. It also checks ZIP
and ISO/UDF menu capabilities for root and nested entries. When the archive backend is available,
it creates a real ZIP, opens it through the controller, checks lazy expansion of
two nested folders, copies root and nested archive entries through Map clicks,
extracts a copied file into a local folder, and returns without changing mode 3. It writes
`/tmp/folder-map-panel.png` on GPU runs. Under `QT_QUICK_BACKEND=software` it
checks that Map remains selected with its GPU placeholder. Automated timing does not establish physical
input latency or subjective smoothness in the user's normal workspace.
`folder_map_startup_test` seeds Map for both panels and exercises the first
frames plus deferred workspace restoration. Its `--map-startup` probe records
every view-mode change: GPU and software startup must remain at 3.

On 2026-10-08, three alternating on/off pairs on the RX 6600 XT with 109
visible cells using the bundled icons gave median per-run p50/p95 intervals of 6.80/7.83 ms without
icons and 6.86/7.85 ms with icons. `--cold-icons` starts atlas preparation at
frame 60 while zooming, so its statistics include icon arrival and first upload.
Two cold-arrival runs gave p95/max intervals of 7.82/8.26 and 7.80/7.92 ms;
a neighboring icon-off run gave 7.81/8.19 ms. These checks did not show an
additional arrival stall in this fixture.

After connecting `useNativeIcons`, a matched native-icon/off check on the same
GPU gave p95/max intervals of 7.88/8.28 ms with native icons and 7.84/8.56 ms
without icons. Loading the native atlas during zoom gave 7.88/8.27 ms. The
integrated GPU panel check also passed both live setting changes and texture
replacements.

Map thumbnails use the existing asynchronous `image://thumbnail` provider and
its shared memory/disk caches and image, PDF, font, video, audio-cover and book
decoders. They follow `showThumbnails`. Eligible visible tiles of at least
64×64 pixels request a fixed 256×256 source bucket, preserving aspect ratio.
The 64 tiles nearest the viewport center form the interest budget; at most two
requests are active. Camera movement pauses new requests and preserves ready
previews while their tiles remain visible. Leaving interest, hiding Map,
disabling thumbnails or changing the directory releases the Qt Quick request;
an already executing decoder may finish, but its detached result is not published.

The provider validates file mtime/size; Map bypasses Qt's URL-only pixmap cache
so refresh cannot reuse a stale image. Icons remain until a valid preview
arrives, including audio without cover art (transparent 1×1 soft misses).
Tiles and labels retain their geometry. At most one 256×256 texture is created
per frame, and no more than 64 preview textures are retained. Decode and disk
access stay on provider workers. The fixed bucket bounds Map's image/texture
working set to about 16 MiB each, in addition to shared application caches.
PDF previews have a white backing under the aspect-fitted page, matching the
other panel views and keeping transparent pages readable on dark themes.

Additional GPU probes:

```sh
build/folder_map_test --gpu-smoke --thumbnail-smoke
build/folder_map_test --gpu-smoke --thumbnail-smoke --without-thumbnails
build/folder_map_test --gpu-smoke --thumbnail-smoke --cold-thumbnails
```

These use 72 valid JPEGs plus 36 folders at 1500×1050. The thumbnail harness
uses a simple asynchronous image provider; the integrated panel check uses the
real application provider, verifies the photo's pixels, PDF white paper and live thumbnail/icon
settings. The delayed provider run starts decoding before zoom, includes all
210 measured frames, then waits for the camera to settle and remaining requests
to complete outside the measured interval. Standard thumbnail runs drain the
one-texture-per-frame upload budget over 70 static frames before timing and
exclude the first 30 zoom frames. This distinction keeps arrival and steady
drawing measurements separate.

On 2026-10-08, three alternating thumbnail-off/on pairs on the RX 6600 XT
with 109 visible cells gave median per-run p50/p95 intervals of 6.90/7.83 ms
off and 6.96/7.78 ms on. The largest interval in those controlled runs was
8.12 ms off and 8.49 ms on. The delayed-arrival run gave p95/max 7.77/8.11 ms.
Earlier runs that started timing before the upload budget drained had isolated
12–23 ms intervals; the controlled steady runs did not reproduce them. These
measurements describe the harness, not physical latency or subjective session
smoothness.

After model/watcher integration, two further alternating off/on pairs gave
p95 intervals of 7.79/8.02 ms off and 7.86/7.81 ms on. Isolated maximum
intervals reached 23.24 ms off and 13.61 ms on; these runs do not establish
stall-free presentation. Delayed thumbnail arrival gave p95/max 7.30/11.92 ms.

## Integration stress check

```sh
XDG_CONFIG_HOME=/tmp/fm-map-stress/config XDG_DATA_HOME=/tmp/fm-map-stress/data \
    XDG_CACHE_HOME=/tmp/fm-map-stress/cache \
    ./build/fm --navigation-gui-benchmark --map-stress
```

This opt-in check requires a hardware GPU and opens the real application with two
Map panels at 1800×1000. It creates 256 JPEGs and nested folders, uses the real
thumbnail provider, and measures 300 frames each for cold thumbnail arrival with
bulk filesystem changes, warm thumbnails, and thumbnails disabled. It verifies
actual GPU uploads, root model updates and nested watcher updates after concurrent
create/rename/delete bursts, then makes 48 directory/view switches with pending
asynchronous work and checks that only the final directory's results are accepted.

Eight reentry/exit cycles check that both maps release their node/visible buffers,
watchers, listing futures, thumbnail requests/images and owned GPU textures.
The harness processes deferred deletion and waits for rendered retirement frames,
matching normal application lifecycle rather than retaining old QML delegates.
On Linux it also records process RSS; on glibc it records live heap allocations.
After three warmup cycles, final growth must stay below 64 MiB RSS and 32 MiB live
heap. Shared application/Qt/driver caches are not expected to become empty.
Allocator trimming is recorded separately as a diagnostic and is never performed
by Map in normal use.

The check writes `/tmp/folder-map-stress.json` and exits nonzero on failure (2 if
hardware GPU is unavailable). Its cadence guards are p95 below 33.4 ms and maximum
below 250 ms, intended to catch large regressions. Recorded Qt frame intervals
describe this fixture and do not establish physical presentation or input latency.
The ordinary `folder_map_test` also covers rapid root changes, discarding late
results after disabling both maps, buffer release and coalesced nested file bursts;
these lifecycle checks run under the software/offscreen CTest environment.

The [2026-10-09 validation report](folder-map-stress-results.json) records a passed
82-test CTest run, the integrated GPU panel checks and this two-panel stress check
on an RX 6600 XT with Qt 6.11.2. The final cold/warm/disabled-thumbnail phases
gave p95 intervals of 7.07/7.34/7.46 ms; the largest interval was 8.40 ms. Both
maps released all checked owned resources in every exit cycle, and process memory
stayed within the post-warmup growth bounds. The regression check originally caught
retained node/visible-vector capacity after leaving Map; those buffers are now
released on disable while remaining reusable during navigation within Map.
An unrelated MEGA SDK shutdown mutex deadlock found by `local_transfer_regression`
was also fixed; that test subsequently passed ten consecutive runs.
