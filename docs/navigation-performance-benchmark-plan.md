# Navigation Performance Benchmark Plan

## Goal

Create a repeatable benchmark for everyday local-folder navigation before
changing the navigation, model, or thumbnail pipelines. The benchmark must
separate correctness failures from timing observations and must not claim that
headless timings prove compositor smoothness.

## Constraints

- Use the real `DirectoryModel` and local provider path.
- Generate deterministic, app-owned fixtures for every run.
- Keep absolute timing informational until enough baseline runs exist.
- Fail automatically on wrong final path, wrong entry count, timeout, or stale
  publication after a replacement navigation.
- Do not require administrator privileges or modify user files/settings.
- Keep manual KWin/compositor and genuinely cold OS-cache checks separate.

## Metrics

### Selection and action checks

The same runner supports `--selection`. It automatically creates and removes
temporary directories containing 100, 1,000 and 10,000 files. Fixture creation
and directory loading are outside the selection timings. No manual dataset
preparation is required.

```bash
QT_QPA_PLATFORM=offscreen build/fm --navigation-benchmark-suite --selection \
  --runs 5 --output build/selection-before.json
# Rebuild after an optimization, then compare on the same machine/build configuration:
QT_QPA_PLATFORM=offscreen build/fm --navigation-benchmark-suite --selection \
  --runs 5 --baseline build/selection-before.json --output build/selection-after.json
ctest --test-dir build --output-on-failure -R 'selection_benchmark_regression|navigation_gui_benchmark_smoke|linux_file_access_resolver_test|local_file_badge_resolver_test'
```

Each selection scenario records `actionMs` using a nanosecond timer, plus
`dataChangedSignals`, `selectionChangedSignals` and the final selected count.
The action interval includes synchronous signal subscribers in the harness;
checking final paths and roles is outside it. `selection-capabilities` records
the real controller getters separately as `copyMs`, `renameMs`, `deleteMs`,
and their sum as `actionMs`. These checks run after selecting all files and
include whatever access-cache state the normal load produced. They are not
cold filesystem measurements.

The suite reports median, nearest-rank p95 and baseline percentage changes.
Absolute timings remain informational, not machine-dependent CTest gates.
Correctness failures always produce a nonzero exit status. The automatic
regression verifies exact selected paths, every visible `IsSelectedRole`,
notification coverage and single/no-op `selectionChanged` delivery for bulk,
single, reversed-range, trimmed-range, sparse, inverted, sorted and filtered
selections. Synthetic provider batches cover `Load more`, hidden selected
rows, metadata updates that hide a selected row, and empty models.

The existing GUI benchmark also verifies selection notifications in List,
Grid and Brief against the real visible delegates after select-all, clear,
select-one and invert. Its `selection-notifications` result records total
`actionMs` and `settledMs` for that sequence. This checks QML propagation
automatically; it does not measure physical input latency or compositor
smoothness. Use `--selection` without `--gui` for the large model/controller
datasets; GUI checks remain part of `--navigation-gui-benchmark`.

On Linux, action checks use `FileAccessResolver::resolveAccess()`, which shares
the existing effective-access rules but skips owner/group name resolution and
presentation metadata. It checks current permissions without the full
metadata cache. Full `resolve()` consumers and `LocalFileBadgeResolver` retain
their existing behavior. On other platforms and for archive paths, the new
entry point delegates to the full resolver. The access regression compares
all access fields and property rows between both paths across permission
modes, valid/broken symlinks, missing paths and parent-permission changes.

Measured on the local Linux development build on 2026-09-05, five runs per
version, with identical final harness code and build flags (the existing build
has an empty `CMAKE_BUILD_TYPE`; these are not Release-build claims):

| Scenario | Files | Before median, ms | After median, ms |
| --- | ---: | ---: | ---: |
| Clear selection | 100 | 0.158 | 0.045 |
| Clear selection | 1,000 | 8.366 | 0.399 |
| Clear selection | 10,000 | 754.202 | 4.176 |
| All selected to one | 10,000 | 754.420 | 4.128 |
| Copy + Rename + Delete checks | 100 | 22.877 | 4.398 |
| Copy + Rename + Delete checks | 1,000 | 591.916 | 42.028 |
| Copy + Rename + Delete checks | 10,000 | 6,264.866 | 419.965 |

At 10,000 files the after p95 is 4.181 ms for clear and 425.919 ms for the
three capability checks. Clear/select-all emit one `dataChanged` range instead
of 10,000 single-row notifications; selecting the middle row from select-all
emits two ranges. Ordinary one-to-one selection is approximately unchanged
(0.229 to 0.252 ms at 10,000 files). Capability checks still synchronously
visit each selected path, so 420 ms is a remaining cost rather than a claim
that every large-selection interaction is now instantaneous.

Raw runs, p95 and automatic percentage comparisons are saved locally as
`build/selection-before.json` and `build/selection-after.json`. Build artifacts
are not tracked; the commands above regenerate reports. The final harness was
also run against the previous implementation to verify behavior equivalence.

### Navigation

For each scenario record:

- elapsed time until the first visible model rows;
- elapsed time until loading settles;
- final path and row count;
- timeout or correctness failure;
- median and p95 across repeated runs.

Later GUI/thumbnail phases add:

- time until the first visible delegate is ready;
- time until visible thumbnails settle;
- largest main-thread/UI chunk;
- thumbnail memory/disk hits, misses, coalesced requests, queue drops, retries,
  cancellations, and stale-result rejections.

## Deterministic Datasets

1. `Documents`: approximately 250 ordinary document files.
2. `Photos`: approximately 300 valid small JPEG/PNG images.
3. `Mixed`: approximately 600 files and folders, including hidden entries,
   links, archives, documents, and media suffixes.

Dataset creation is outside the measured interval. Names and contents must be
stable so sort order and expected visible counts are known.

## Phases

### Phase 1: model navigation harness

Add an explicit `fm --navigation-benchmark` mode. It creates temporary
fixtures, drives the real `DirectoryModel`, and prints a versioned JSON report.

Scenarios:

- cold process load of Documents, Photos, and Mixed;
- warm revisit after the model/provider has already been used;
- immediate replacement navigation `Documents -> Photos -> Mixed`.

Acceptance:

- the command exits zero only when all final paths/counts are correct;
- replacement navigation settles on Mixed and never publishes an older path as
  the final result;
- every scenario has first-row and settled timings;
- a CTest smoke run checks correctness without enforcing timing thresholds.

This phase measures scanning plus `DirectoryModel` consolidation/filter/sort.
It does not measure QML delegate creation, image decoding, or compositor work.

### Phase 2: repeated-run report

Add a small runner around the Phase 1 command:

- execute 5-7 isolated repetitions;
- preserve individual JSON reports;
- calculate median and p95;
- support an optional baseline report;
- report percentage changes without failing on small timing noise.

Acceptance:

- malformed/incomplete runs are rejected;
- correctness failures always fail the runner;
- timings remain non-blocking until a stable-machine threshold is approved.

Implemented command:

```bash
build/fm --navigation-benchmark-suite --runs 7 --output current.json
build/fm --navigation-benchmark-suite --runs 7 \
  --baseline current.json --output comparison.json
```

The suite embeds every raw child report, emits median and nearest-rank p95 for
each scenario/metric, and reports median percentage changes when a compatible
baseline is supplied. Percentage changes remain informational.

### Phase 3: QML viewport and thumbnail readiness

Expose a narrow benchmark-only command surface to the real file panel. Drive
semantic actions (`openPath`, view-mode changes, viewport settling) rather than
mouse coordinates.

Acceptance:

- Grid, List, and Brief modes are covered;
- visible delegate and thumbnail readiness are reported separately;
- rapid navigation rejects post-cancellation thumbnail requests/results;
- existing pause-during-scroll/resize behavior remains intact.

Implemented first GUI command:

```bash
QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software \
  build/fm --navigation-gui-benchmark
```

It loads the real application QML at 1120x720, uses semantic panel methods,
and reports model-settled, visible-delegate, and visible-thumbnail readiness
for List, Grid, and Brief. The replacement scenario drives
`Documents -> Mixed -> Photos` without coordinate clicks and verifies that all
visible delegates belong to the final path. List currently has no thumbnail
surface, so its thumbnail timing is intentionally `-1`.

Repeated GUI baseline and comparison:

```bash
build/fm --navigation-benchmark-suite --gui --runs 7 \
  --output gui-baseline.json
build/fm --navigation-benchmark-suite --gui --runs 7 \
  --baseline gui-baseline.json --output gui-comparison.json
```

GUI aggregates are separated by scenario, dataset, and view mode. They include
median/p95 for model settling, viewport readiness, and thumbnail readiness;
List omits thumbnail aggregates because it has no thumbnail surface.

Thumbnail timing is decomposed into first/all visible requests scheduled and
first/all visible thumbnails ready. This separates the intentional QML stagger
from the remaining image-provider/decode tail before changing either stage.

### Phase 4: baseline and optimization

Collect at least 5-7 runs on the same machine and filesystem. Optimize only the
stage identified by the report. Candidate work includes `DirectoryModel`
consolidation/reset costs and splitting cheap local images from expensive local
thumbnail decoders, but neither is assumed to be the bottleneck in advance.

First measured optimization:

- Added first/all-visible scheduled and ready timestamps.
- Five-run pre-change Grid baseline showed the last visible request scheduled at
  median 619 ms and all visible thumbnails ready at 621 ms. The approximately
  2 ms tail showed that the deterministic QML stagger, not decoding these small
  PNG fixtures, dominated this scenario.
- Changed only the Grid initial stagger from `100 + index % 16 * 28` to
  `60 + index % 12 * 16`. Scroll/resize pause gates and Brief timings were not
  changed.
- Seven-run comparison reduced normal Grid thumbnail readiness from median
  621 ms to 358 ms (about 42%) and replacement-navigation Grid from 659 ms to
  391 ms (about 41%). Brief remained within roughly 2% timing noise.

These figures are offscreen benchmark evidence. A live compositor check is
still required before treating perceived smoothness as visually confirmed.

Second measured optimization:

- A seven-run Brief baseline instantiated 160 cached delegates while only 44
  were visible. Every cached delegate was allowed to start thumbnail loading,
  so invisible work competed with the current viewport.
- Restricted Brief thumbnail eligibility to the current viewport. Existing
  scroll/resize pause and resume behavior remains responsible for starting the
  newly visible rows after navigation.
- Aligned the remaining Brief request stagger with the measured Grid value,
  changing `90 + index % 12 * 24` to `60 + index % 12 * 16`.
- Against the original seven-run baseline, all-visible request scheduling fell
  from median 469 ms to 371 ms (about 21%), and all-visible thumbnail readiness
  fell from 1761 ms to 1141 ms (about 35%). The p95 readiness time fell from
  2268 ms to 1284 ms (about 43%).

These Brief figures use the same offscreen workload and remain subject to a
live scrolling check under the compositor.

Third measured optimization — Folder Peek:

- Added benchmark-only snapshots to the real Folder Peek Grid/List delegates.
  Reports now cover entries ready, viewport ready, first/all visible thumbnail
  scheduling and readiness.
- Added local lifecycle scenarios for warm reopen, rapid A-B-C replacement,
  and close during load. The latter verifies that the controller remains
  closed with an idle state and empty path/entries after stale work can return.
- Seven-run baseline showed that the old FolderPreviewIcon stagger dominated:
  normal Grid scheduled all visible requests at median 564 ms and displayed all
  visible thumbnails at 567 ms.
- Changed only the shared Folder Peek/preview icon stagger from
  `100 + index % 16 * 28` to `60 + index % 12 * 16`, preserving pause, retry,
  generation, and cancellation behavior.
- Seven-run comparison reduced normal Peek Grid readiness from median 567 ms to
  280 ms (about 51%) and p95 from 570 ms to 282 ms. A-B-C readiness fell from
  563 ms to 279 ms (about 50%); Peek List fell from 366 ms to 229 ms (about 37%).

Remote-provider warmup and network latency are intentionally not simulated by
the deterministic local fixture. They require provider-specific live checks.

## Automatic Versus Manual Checks

Automatic gates:

- expected final path and count;
- no timeout;
- no stale final publication;
- no post-cancellation work once thumbnail coverage is added;
- valid complete JSON report.

Manual checks:

- perceived scrolling/animation smoothness;
- KWin/compositor artifacts;
- cold kernel/filesystem cache;
- remote-provider latency and throttling.

## Normal Validation

```bash
cmake --build build -j 12
QT_QPA_PLATFORM=offscreen build/fm --navigation-benchmark
ctest --test-dir build --output-on-failure -j 12
git diff --check
```
