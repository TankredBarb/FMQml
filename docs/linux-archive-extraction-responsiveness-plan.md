# Linux archive extraction responsiveness plan

This document records the planned fix for severe UI lag during large archive
extraction on Linux, especially when extracting from NTFS to Btrfs. Extraction
currently uses external `7z`, so it does not benefit from the Linux
cross-filesystem copy cache policy already used by `OperationQueue`.

## Current findings

- Full archive extraction is routed through `ArchiveFileProvider` and external
  `7z` via `QProcess`.
- The `7z` process currently runs with normal CPU and I/O priority.
- The existing Linux copy fix applies only to in-process copy. It lowers the
  operation thread I/O priority and uses `sync_file_range` plus
  `posix_fadvise` windows to avoid dirty-page-cache pressure.
- That copy policy cannot directly control `7z` reads and writes because they
  happen in a child process.
- `7z` progress reporting is already throttled, so progress updates are not the
  likely primary UI lag source.
- Linux directory watching can still add pressure: extracting thousands of
  files into or near the visible folder can produce large `inotify` batches,
  repeated `upsertPath()`/`stat` work, and overflow-triggered refreshes while
  the archive is still being written.

## Preferred solution

Use two Linux-only layers before considering a deeper extraction rewrite.

1. Run `7z` as a background-quality process.
   - Lower child process CPU priority with `nice`.
   - Lower child process I/O priority with Linux `ioprio`.
   - Prefer Qt child-process setup APIs where available instead of shelling out
     through wrapper commands.
   - Consider a conservative `-mmt=N` thread limit only after measuring the
     first priority-only pass.

2. Suppress live directory-model churn during large extract operations.
   - When `OperationQueue::Type::Extract` writes to a local folder currently
     visible in a panel, temporarily treat the destination as a bulk mutation.
   - Do not apply every `inotify` batch during the operation.
   - Mark the affected model dirty and perform one normal refresh after
     `operationFinished`.
   - Keep unrelated external edits and navigation responsive.

This keeps the existing `7z` compatibility and destination-near staging model,
while addressing the two likely causes of UI stalls: Linux I/O/writeback
contention and high-churn model refresh work.

## Implementation plan

1. Add diagnostics before changing behavior.
   - Log source filesystem, destination filesystem, archive size, destination
     path, `7z` elapsed time, and operation elapsed time.
   - Count directory-watch batches, overflow refreshes, and applied events
     during extraction.
   - Use this to compare baseline against each mitigation.

2. Add a small Linux process-QoS helper for archive subprocesses.
   - Apply it to `extractArchiveWithSevenZip()` first.
   - Reuse it for compression later if measurements show the same UI pressure.
   - Keep Windows/macOS behavior unchanged.
   - Preserve cancellation: abort must still kill the child process promptly.

3. Add extraction bulk-refresh suppression.
   - Scope it to local filesystem destinations.
   - Activate only for panels whose current path is the extraction destination
     or its parent.
   - Queue a single refresh after extraction completes, fails, or is cancelled.
   - Avoid hiding explicit navigation or manual refresh requests.

4. Benchmark representative cases.
   - One large file inside an archive.
   - Many small files inside an archive.
   - NTFS source to Btrfs destination.
   - Same-filesystem extraction as a control case.
   - Measure extraction time and subjective UI responsiveness while navigating,
     scrolling, and opening folders.

5. Decide whether a second pass is needed.
   - If UI is responsive and extraction speed remains acceptable, stop.
   - If CPU contention remains high, tune `-mmt=N`.
   - If Btrfs writeback still dominates, investigate optional Btrfs-specific
     policy separately. Do not make CoW-disabling behavior implicit.

## Acceptance checks

- Extracting a large archive from NTFS to Btrfs does not freeze panel
  navigation, scrolling, or basic UI interaction.
- Total extraction time does not regress unacceptably compared with the current
  direct `7z` path.
- Cancelling extraction kills `7z` and cleans staging artifacts.
- The destination panel shows correct final contents after the operation.
- External changes outside the active extraction destination still appear
  normally.
- Windows behavior is unchanged.
