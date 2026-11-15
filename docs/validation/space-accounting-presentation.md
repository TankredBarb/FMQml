# Space accounting presentation

Properties and Disk Usage distinguish content size from on-disk regular-file
allocation. Filesystem type appears in the Properties General statistics block
and in the Disk Usage summary. Disk Usage tables, sorting, percentages, map and
chart continue to use content sizes; the UI identifies that basis explicitly.
Filesystem-wide used/total values have a separate label and scope.
The summary shows separate compact coverage statuses for Contents and On disk.
Measurement explanations, full error paths and skipped content paths are in
the scrollable Details popup; the summary keeps only a short size-basis note.
Allocation skip counts are cached with the corresponding allocation result.

The shared `FileSpaceUsage` measurement runs off the UI thread and returns a
complete total, an explicitly marked lower bound, or `Unavailable` with a reason.
Unreadable/unsupported entries are skipped and counted rather than discarding
all readable data. Partial totals display `At least …`, with the first skipped
path and its reason in the explanation. A wholly unreadable selection remains
unavailable; a readable empty file plus unreadable data is explicitly partial
zero. Missing/incompatible helper service fails immediately without retrying
every file or substituting content sizes. Every explicitly selected local root
is measured on its own filesystem, independently of selection order. Directory
traversal skips filesystem boundaries and marks those omissions as partial.
Delayed allocation (`FIEMAP_EXTENT_DELALLOC`) is tracked separately from
unsupported allocation. Single-file Properties shows Pending allocation and
retries asynchronously every two seconds until the kernel provides the size.
Closing Properties or changing selection cancels pending retries. Folder and
multi-selection scans retain explicit lower bounds with pending counts and
can be refreshed; they do not repeatedly traverse the whole tree. Measurements
never request synchronous writeback. The Btrfs helper reports pending state
through an optional response field without changing the batch protocol.
Hardlinks and overlapping
paths are counted once; directory metadata and symlink targets are excluded.
Cancellation and generation checks prevent obsolete results from changing the
next selection. Disk Usage caches content and completed allocation results
together. Cache entries belong to a scan generation: allocation from an
unfinished or cancelled Rescan cannot overwrite the previous snapshot.
Reopening uses that snapshot; Rescan refreshes both. Navigation in finished
content results, including Map and Chart, does not wait for allocation.

On Linux, Btrfs data is read by the dedicated `fm-space-helper` service. FM opens
the regular file as the ordinary user and transfers the readable descriptor
over the system bus. The root helper never accepts or opens arbitrary paths.
It rejects O_PATH descriptors, write-only descriptors, and non-regular files.
The only operation reads the descriptor's own subvolume extent metadata;
there is no copy, delete, permission-editing, command-execution or content-read
interface and no polkit/pkexec authentication path.

Compressed extent sizes come from Btrfs metadata, not stat blocks or FIEMAP
logical lengths. Shared extents are counted once by numeric extent address in
a separate set for each filesystem UUID, including across explicitly selected
Btrfs subvolumes. Inline encoded payload bytes are counted
as file data, including when stored in metadata; filesystem metadata overhead
and storage replicas are excluded. This measures referenced data, not data
exclusive to a file. Pending allocation or changes during measurement are
reported as skipped data, not stale numbers. Non-Btrfs files use reported
allocation when FIEMAP can establish it. Unsupported cases stay unavailable;
zero is displayed only for a successfully measured zero.

## Installation and response time

`install.sh` builds and installs the service, stops its previous process using
the installed helper's root-only `--stop-running` maintenance command, then
reloads the system bus policy. Maintenance validates the service PID's owner
and executable and signals it through a pidfd; ordinary clients cannot stop it.
The files are the root-owned executable under `libexec/fm`, the D-Bus activation
file under `share/dbus-1/system-services`, and its narrow access policy under
`/etc/dbus-1/system.d`. The normal install manifest includes them for uninstall.
The existing administrator helper and its authorization policy are unchanged.

Installation requires administrator permission once. Ordinary measurements do
not. D-Bus starts the helper on demand as root; it exits after 90 idle seconds.
No background filesystem scan is performed. The client waits only in a worker,
with a 1500 ms IPC timeout. Requests carry up to 16 readable descriptors;
the batch has a shared 1000 ms budget and each file a 65536-extent limit.
The service reports that longer analysis is needed if that bound is
reached. Large directory totals still require traversing their files, in the
background; they are not promised to finish instantly.

Observed on this host, as UID 1000, using the installed system service:

| Fixture | Reported file data | Initial request including activation | Warm median | Warm p95 |
|---|---:|---:|---:|---:|
| 4 MiB compressible file | 128 KiB | 21.35 ms | 0.297 ms | 0.343 ms |

The service's UID was independently read from the system bus as 0. After idle
exit, the next ordinary request activated it again without authentication.
These timings are observations on this host, not fixed performance guarantees.

The batch-protocol regression measured `/home/tankred/s98investigation` as UID
1000 before and after the update: 1849 ms versus 986 ms, with identical
656489012-byte partial totals and the same two inaccessible paths. The updated
scan measured 8431 files in 524 helper requests; peak client RSS was 16292 KiB
versus 17668 KiB. This is one comparison, not a speed guarantee.

Allocation progress is delivered at most every 200 ms, plus initial/final
updates. The UI shows the measured lower bound and file/skip counters while
working. READY is withheld until both scans finish; Cancel and closing the
dialog also cancel allocation. A stopped lower bound is explicitly labelled
and requires Rescan to continue. Inode and extent deduplication uses numeric
keys rather than per-file strings to reduce memory on large trees.

A complete UID-1000 allocation scan of `/home/tankred` completed in 77290 ms:
1058753 unique regular files, 66016 helper requests, 386 progress updates,
66468 KiB peak client RSS. The result was an explicitly partial lower bound
of 47559116954 bytes with 23 skipped paths. File counts need not match the
content scanner because allocation deduplicates inodes and excludes links and
other filesystems. These are live-directory observations, not an atomic
snapshot or a comparison against the previous full-home scan. Raw evidence:
`/tmp/fm-space-home-batch.json` and `/tmp/fm-space-home-progress.log`.

An allocation total is not a deletion forecast. Other links, snapshots, open
descriptors, metadata changes and filesystem activity can affect reclamation.
Copy, move, delete and filesystem configuration are unchanged.

## Evidence from the local Btrfs experiment

The controlled experiment in `scripts/btrfs-space-proof.py` was run on the
home Btrfs filesystem with `compress=zstd:3`. Test files were removed afterward.

| Fixture | Content size | stat blocks | Data released after deletion |
|---|---:|---:|---:|
| Compressible data | 256 MiB | 256 MiB | 8 MiB |
| Random data | 128 MiB | 128 MiB | approximately 128 MiB |
| Sparse file | 128 MiB | 4 KiB | 4 KiB |
| First compressed reflink | 128 MiB | 128 MiB | 0 |
| Last compressed reflink | 128 MiB | 128 MiB | approximately 4 MiB |
| Both compressed reflinks together | 256 MiB | 256 MiB summed | 4 MiB |
| First hardlink | 128 MiB | 128 MiB | 0 |
| Last hardlink | 128 MiB | 128 MiB | 128 MiB |

An additional privileged *test-only* run read Btrfs extent sizes/backreferences
and compared predictions with observed filesystem counters. Nine of eleven
observations matched exactly; two differed by 8 KiB and 16 KiB. The counters
cover a live filesystem, so these differences do not establish a cause or a
per-file measurement error. Reports from that session are
`/tmp/fm-btrfs-space-proof.json` and `/tmp/fm-btrfs-extent-proof.json`.
This privileged prototype is not used by the application.

## Verification

```sh
cmake --build build --target fm fm-space-helper properties_layout_test file_space_usage_test -j 4
ctest --test-dir build -R '^(file_space_usage_test|properties_layout_test|disk_usage_treemap_layout_test|space_presentation_smoke)$' --output-on-failure
```

`file_space_usage_test` uses disposable fixtures in its working directory to
check compressed, random, sparse, empty, missing and linked files, overlapping
selections, cancellation, and the distinction between unavailable and zero.
With the installed helper it checks actual compressed/sparse sizes and reflink
deduplication, identical batch totals, request counts and progress cancellation.
Selections spanning different devices are checked in both path orders; a
failure on one device must preserve measurements from the other.
`--require-helper` makes missing service access fail explicitly.
`--inspect-progress PATH` prints bounded progress and a final JSON result for
real-directory measurements; it does not alter the directory.
Permission regression cases check a blocked directory, a blocked file, partial
zero, an entirely unreadable selection, and preservation of readable totals.
`properties_layout_test` checks that FS type, allocation and the explanation
stay in the General statistics block for both single and multiple selections.

`space_presentation_smoke` opens the production Properties and Disk Usage UI
in isolated config/data/cache/state directories with the offscreen software
renderer. It checks single-file, folder and multi-selection fields, shared
calculation results, cached reopening and rapid selection changes. It captures dark/light Properties and
all three Disk Usage views as `/tmp/fm-space-properties-*.png` and
`/tmp/fm-space-analyze-*.png`. These renders verify layout; they do not establish
the appearance of the live compositor's translucent surfaces or user acceptance.
With a working helper it also checks and captures partial totals caused by a
closed directory in both Properties and Disk Usage.
It also holds the allocation worker in a test queue until the content scan
finishes, checks that the dialog remains busy with Cancel visible, and clicks
Cancel. Reopening a completed cached scan must not start allocation again.
Additional checks hold the content worker during Rescan, finish allocation
first, then cancel and reopen: both counts and allocation must come from the
original cache. Cancelling both phases before content completion must also
preserve it. Map and Chart navigation is checked while allocation is queued.
Fresh unsynced files must show Pending allocation or a measured size, rather
than Unavailable. The test then explicitly fsyncs its own fixture and checks
that Properties updates without reloading, and that obsolete retries cannot
overwrite a new selection. `FM_SPACE_TEST_ROOT` selects a writable filesystem
for these disposable UI fixtures, allowing the same check on Btrfs and ext4.
On `/mnt/bigSSD` (ext4), a fresh copy of `2150737685.jpg` reproduced DELALLOC
and the former unavailable result for at least 4.2 seconds. After test-only
fsync it measured 1839104 bytes. The updated production UI test on that volume
observed Pending allocation, automatic transition to a measured size without
reload, and rejection of an obsolete retry after selection change. The test
fixtures were removed; the user's image and copied image were not modified.

The real-directory permission regression was reproduced and checked as UID 1000:

| Path | Measured lower bound | Skipped paths | Time |
|---|---:|---:|---:|
| `/home/tankred/s98investigation` (Btrfs) | 656489012 bytes | 2 | 1552 ms |
| `/mnt/mediumSSD/SteamLibrary` (ext4) | 429506560 bytes | 1 | 13 ms |

The first directory has two root-owned private probe directories. SteamLibrary
has `steamapps/.compatdata-users/1001` inaccessible to UID 1000. Their permissions
and contents were not modified. Values are observations of the accessible data
at the time of the scan, not full directory sizes or atomic filesystem snapshots.

`scripts/test-space-helper.py --user USER --build build` is a root-only
integration harness on a private bus. Only the service is privileged; every
client drops to the specified user's UID. It checks a 64 MiB file occupying
2 MiB, two reflinks occupying the same 2 MiB together, a partial compressed
overwrite adding 4 KiB, and an inline payload. It also verifies rejection of
O_PATH, write-only and directory descriptors, rejection of a Delete method,
the root-only maintenance stop, and honest failure when the helper is absent. Fixtures and private processes
are removed. The report is `/tmp/fm-space-helper-proof.json` for this session.

The UI tests must run with system-bus access to exercise installed measurements;
inside a sandbox that blocks the bus they exercise the unavailable fallback.
No estimate of bytes freed is added, and no privilege prompt is issued by this
feature. Exclusive/reclaimable-byte analysis remains a separate operation.
