# Provider Transfer Performance Research

## Scope

This document reviews expected transfer performance for these scenarios:

- Google Drive to MEGA;
- portable phone storage to MEGA;
- portable phone storage to Google Drive.

The review covers two file-shape classes:

- many small/medium files;
- a few large files.

This document combines code-path research and the first manual runtime notes.
The findings below are based on the current transfer implementation in:

- `src/core/OperationQueue.cpp`;
- `src/plugins/gdrive/GDriveFileProviderPlugin.cpp`;
- `src/plugins/mega/MegaFileProviderPlugin.cpp`;
- `src/plugins/portable_device/PortableDeviceFileProviderPlugin.cpp`;
- `src/core/FileProvider.h`.

## Current Transfer Model

The app has three relevant transfer paths.

### Local file to provider

When the source provider is `file` and the destination provider is not `file`,
`OperationQueue::copyPath()` calls destination `copyFromLocalFile()` directly.

For many local files up to 16 MiB, `OperationQueue` can batch them through
`copyFromLocalFiles()` when the destination provider supports it.

Current provider support:

- Google Drive supports batch local-file upload, including mixed multipart and
  resumable files.
- MEGA supports batch local-file upload through the MEGA SDK transfer queue.
  Default upload concurrency is 4 and can be overridden with
  `FMQML_MEGA_UPLOAD_CONCURRENCY=1..4`.

### Provider to provider

When both source and destination are non-local providers,
`OperationQueue::copyPath()` stages files through cleanup-managed local
temporary batches:

1. create destination directories;
2. materialize a bounded wave through source `copyToLocalFiles()` when
   supported, otherwise fall back to per-file `copyToLocalFile()`;
3. upload the wave through destination `copyFromLocalFiles()` when supported,
   otherwise fall back to per-file `copyFromLocalFile()`;
4. delete the staging artifacts.

There is no streaming pipeline from source network/device directly into
destination upload.

### Portable phone to provider

Portable device paths are non-local provider paths. For a portable directory,
or for multiple selected portable files, copied to a destination provider that
supports `copyFromLocalFiles()`, `OperationQueue` now uses bounded local
staging batches:

1. create destination directories;
2. copy up to 64 files or 128 MiB into one cleanup-managed staging directory;
3. call destination `copyFromLocalFiles()` for the staged local files;
4. schedule the staging directory for cleanup;
5. keep larger files in the staged-batch scheduler as their own bounded waves
   when needed. Unsupported destination providers still fall back to the
   one-file staging path.

Non-local provider directory copies and multi-selected non-local provider file
copies to batch-capable cloud destinations use staged batches. Single-file
provider-to-provider copies still use the one-file staging path.

The Linux portable provider is read-only and copies out through KIO
`file_copy`.

## Scenario Results

| Scenario | Current path | Many small/medium files | Few large files | Main bottleneck |
| --- | --- | --- | --- | --- |
| Google Drive -> MEGA | GDrive download to bounded local staging batches, then MEGA batch upload | Staged batch waves up to 64 files / 128 MiB | Large files stay in staged batch waves; files above 128 MiB become single-file waves | Sum of GDrive download time + MEGA upload time, plus temp disk IO |
| MEGA -> Google Drive | MEGA download to bounded local staging batches, then GDrive batch upload | Staged batch waves up to 64 files / 128 MiB | Large files stay in staged batch waves; files above 128 MiB become single-file waves | Sum of MEGA download time + GDrive upload time, plus Drive API throttling risk |
| Phone -> MEGA | Phone/KIO copy to bounded local staging batches, then MEGA batch upload | Staged batch waves up to 64 files / 128 MiB | Large files stay in staged batch waves; files above 128 MiB become single-file waves | MEGA upload; staging is consistently faster than upload |
| Phone -> Google Drive | Phone/KIO copy to bounded local staging batches, then GDrive batch upload | Staged batch waves up to 64 files / 128 MiB | Large files stay in staged batch waves; files above 128 MiB become single-file waves | GDrive upload and Drive API throttling risk |

## Verified Cross-Provider Work

The transfer stack has now been validated for both sides of the cloud pipeline:

- source-side parallel download/materialization:
  - MEGA implements `copyToLocalFiles()` with SDK download concurrency 4 by
    default;
  - Google Drive implements `copyToLocalFiles()` with HTTP download concurrency
    4 by default;
- destination-side parallel upload:
  - MEGA implements `copyFromLocalFiles()` with SDK upload concurrency 4 by
    default;
  - Google Drive implements `copyFromLocalFiles()` with bounded parallel upload
    scheduling.

Verified runtime results:

- Google Drive -> local: batch download path exercised successfully, including
  a 5 GiB folder. Observed waves usually landed around 16-25 MiB/s on the test
  machine.
- MEGA -> local: batch download path exercised successfully; local download is
  no longer strictly one file at a time.
- Google Drive -> MEGA: completed successfully with 118/118 files, 160,409,755
  bytes, no failures. Staging used Google Drive batch download; destination used
  MEGA batch upload.
- MEGA -> Google Drive: completed successfully with 316/316 files,
  1,116,819,920 bytes, no failures. Staging used MEGA batch download;
  destination used Google Drive batch upload.

This confirms that the major cloud-to-cloud bottleneck fixes are in place:
download/materialization and upload are both parallelized per wave for MEGA and
Google Drive. The current design still stages a full wave before uploading it;
staging and upload are intentionally not pipelined yet.

## Google Drive Details

Google Drive upload has two modes:

- files up to 5 MiB use multipart upload;
- files larger than 5 MiB use resumable upload.

For local small-file batches, Google Drive supports `copyFromLocalFiles()`.
The current default concurrency is 6, configurable by
`FMQML_GDRIVE_UPLOAD_CONCURRENCY`, clamped to 1..12. Logging can be enabled
with `FMQML_GDRIVE_UPLOAD_LOG=1`.

For source materialization, Google Drive supports `copyToLocalFiles()` using
bounded parallel HTTP downloads. The default download concurrency is 4,
configurable by `FMQML_GDRIVE_DOWNLOAD_CONCURRENCY`, clamped to 1..8. Summary
logging can be enabled with `FMQML_GDRIVE_DOWNLOAD_LOG=1`; range-level queue
logging is behind `FMQML_GDRIVE_DOWNLOAD_RANGE_LOG=1`.

Important behavior for the requested scenarios:

- `copyFromLocalFiles()` is reached for local sources and for non-local staged
  batch waves after bounded local materialization.
- `copyToLocalFiles()` is reached for Google Drive sources copied to local
  folders or to other providers through staged waves.
- Phone -> Google Drive and MEGA -> Google Drive directory copies upload staged
  waves through `copyFromLocalFiles()`.
- Google Drive upload is no longer the only optimized path; Google Drive source
  downloads are also batched.

Expected effect:

- local folder -> Google Drive can be much faster for many small files;
- phone -> Google Drive now gets the batch upload advantage after files are
  materialized into bounded local staging waves.

## MEGA Details

MEGA uses SDK-backed blocking provider calls:

- `copyToLocalFile()` starts SDK download to a `.part` path and waits;
- `copyToLocalFiles()` queues SDK downloads with bounded concurrency;
- `copyFromLocalFile()` starts SDK upload and waits;
- `copyFromLocalFiles()` queues local files through SDK uploads with bounded
  concurrency.

Default MEGA download and upload concurrency is 4, configurable by
`FMQML_MEGA_DOWNLOAD_CONCURRENCY=1..4` and
`FMQML_MEGA_UPLOAD_CONCURRENCY=1..4`.

There is summary timing logging behind `FM_MEGA_TIMING`. Per-file download and
upload rows are intentionally separate behind `FM_MEGA_DOWNLOAD_ITEM_TIMING=1`
and `FM_MEGA_UPLOAD_ITEM_TIMING=1`.

Current limitation:

- provider-to-provider and phone-to-MEGA staged batch transfers still run in
  phases: materialize a bounded local wave, then upload that wave.

## Materialization / Download Bottleneck

The generalized staged batch path fixed the upload-side sequential behavior for
MEGA -> Google Drive and Google Drive -> MEGA. The remaining ceiling is now the
source materialization phase for cloud -> cloud copies.

Current staged wave model:

1. scan source tree and create destination folders;
2. download/copy every file in the wave into one local staging directory;
3. upload the whole staged wave through destination `copyFromLocalFiles()`;
4. delete the staging directory;
5. continue with the next wave.

This means staging and upload are not overlapped. For phone -> cloud this is
acceptable because phone/KIO staging has been observed around 25-35 MiB/s. For
MEGA -> Google Drive, staging is a MEGA network download, and recent logs show
it can be substantially slower than the Google Drive batch upload.

Observed MEGA -> Google Drive wave examples:

```text
[ProviderStagedBatchWave] sourceScheme= mega destinationScheme= gdrive
files=64 bytes=106171571 stagingMs=53693 uploadMs=20504
stagingMiBs=1.88578 uploadMiBs=4.93821

[ProviderStagedBatchWave] sourceScheme= mega destinationScheme= gdrive
files=64 bytes=72224404 stagingMs=53290 uploadMs=18032
stagingMiBs=1.29252 uploadMiBs=3.8198

[ProviderStagedBatchWave] sourceScheme= mega destinationScheme= gdrive
files=17 bytes=8576237 stagingMs=11740 uploadMs=5138
stagingMiBs=0.696673 uploadMiBs=1.59185
```

Observed MEGA -> local download-only baseline:

```text
[ProviderMaterializeFile] sourceScheme= mega destinationScheme= file
source= IMG20251226193858.jpg bytes=4624088 elapsedMs=386 throughputMiBs=11.4245

[ProviderMaterializeFile] sourceScheme= mega destinationScheme= file
source= VID20260307203015.mp4 bytes=168874374 elapsedMs=3010 throughputMiBs=53.5054

[ProviderMaterializeFile] sourceScheme= mega destinationScheme= file
source= x_1ba251bb.jpg bytes=33597 elapsedMs=385 throughputMiBs=0.0832

[ProviderMaterializeFile] sourceScheme= mega destinationScheme= file
source= Фото009.jpg bytes=96223 elapsedMs=475 throughputMiBs=0.193
```

Direct MEGA -> local confirms that materialization is currently strictly
one-file-at-a-time in the generic `copyPath()` loop. Large files can reach high
throughput, but small files are dominated by fixed per-file SDK/request overhead.
The same sequential source-materialization shape is currently used by staged
waves before upload: the wave is filled by calling `copyToLocalFile()` for each
file in order.

Interpretation:

- Google Drive upload is correctly batched and parallelized in these tests.
- The visible pause at `Provider staged batch wave ... stagingDir ...` is the
  full MEGA download/materialization of the wave, not an idle hang.
- Google Drive upload starts only after the full wave is staged, so upload logs
  appear late even when Drive is ready.
- Smaller waves reduce perceived stalls but may increase per-wave overhead.
- Larger waves improve upload batching but can make the UI look stuck while
  source materialization runs.
- Source materialization needs its own scheduler. Upload batching alone cannot
  fix many-small-file cloud downloads because per-file materialization overhead
  is paid serially.

Candidate optimizations:

- Pipeline wave staging and upload: stage wave N+1 while uploading wave N.
- Add adaptive wave sizing for cloud -> cloud, with smaller byte targets when
  source materialization is the observed bottleneck.
- Add source-side batch download/materialization support where providers can
  safely expose it. This should be a capability-gated API rather than blindly
  calling arbitrary provider `copyToLocalFile()` implementations from worker
  threads.
- Improve progress labels during staging so the drawer explicitly says it is
  downloading/materializing the current wave.

Materialization diagnostics:

- `FMQML_PROVIDER_MATERIALIZE_LOG=1` logs one `[ProviderMaterializeFile]` row
  for each file materialized during staged waves.
- The log is emitted from `OperationQueue`, so it covers MEGA, Google Drive,
  and portable phone sources uniformly.
- Each row includes source/destination scheme, wave index, file name, bytes,
  elapsed materialization time, and per-file MiB/s.
- Use it with `FMQML_PROVIDER_TRANSFER_TIMING=1` to compare per-file
  materialization with aggregate `[ProviderStagedBatchWave]` staging totals.

Non-goals for the next pass:

- Do not add a streaming provider-to-provider API yet.
- Do not increase Google Drive upload concurrency based on these logs; upload is
  already faster than materialization in the failing case.
- Do not change MEGA upload concurrency for this issue; the slow phase is MEGA
  download, not MEGA upload.

Expected effect:

- large files should mostly run at the slower side of download/upload, but
  sequential staging makes elapsed time closer to `download/read + upload`
  instead of `max(download/read, upload)`;
- many small files pay per-file SDK/API overhead.

## Portable Phone Details

Portable device provider behavior:

- Linux backend uses KDE/KIO;
- `copyToLocalFile()` copies out through blocking KIO file copy;
- `openRead()` also materializes a temporary local file;
- provider is read-only.

Expected effect:

- many small files are likely dominated by KIO/MTP per-object overhead;
- a few large files are likely dominated by USB/MTP read speed and then cloud
  upload speed;
- because upload starts after staging completes, large phone-to-cloud transfers
  do not pipeline USB read and network upload.

## Performance Risk Summary

### Many small/medium files

Highest risk:

- phone -> MEGA;
- phone -> Google Drive;
- Google Drive -> MEGA with many files.

Reasons:

- Google Drive -> MEGA still handles every file as a separate provider-to-provider
  staging operation;
- phone directory transfers now have staged batching for Google Drive and MEGA;
- no per-file pipeline;
- cloud APIs/SDKs and KIO/MTP each add per-file latency;
- progress updates are per operation and can look uneven because each staged
  file has download/read and upload phases.

### Few large files

Highest risk:

- any provider-to-provider large file transfer when temp storage is slow or
  low on space.

Reasons:

- each file is written fully to local staging before upload starts;
- transfer requires enough local temp space for the current large file;
- elapsed time is roughly source materialization plus destination upload;
- cancellation and cleanup are supported, but partial work can still consume
  time before cancellation is observed by the provider.

## Expected User-Visible Behavior

For the requested scenarios, users should expect:

- progress advances through two phases for one file or one staged batch:
  source-to-temp, then temp-to-destination;
- no true simultaneous download/upload for a single file;
- small-file sets can feel slower than byte throughput suggests, especially
  when the destination provider lacks batch upload support;
- large files can saturate network during upload, but only after staging has
  completed;
- local temp disk activity will be visible during provider-to-provider copies.

## Benchmark Plan

Use the same datasets for all scenarios. Dataset contents must be generated
once, then copied unchanged to the relevant source provider before each test.

General dataset rules:

- use ASCII-only names;
- no symlinks;
- no hidden files;
- no empty files;
- no duplicate relative paths;
- deterministic pseudo-random file contents, not sparse files and not all-zero
  files;
- keep a manifest with relative path, size, and SHA-256 for every file;
- preserve the same directory layout across local disk, phone storage, Google
  Drive, and MEGA.

Root folder naming:

- `fm-transfer-dataset-a-small`;
- `fm-transfer-dataset-b-medium`;
- `fm-transfer-dataset-c-large`.

Content generation:

- file bytes should be deterministic from the relative path and byte offset;
- use a stable seed string, for example
  `FMQML_PROVIDER_TRANSFER_DATASET_V1`;
- generate data in blocks from `SHA-256(seed + relative_path + block_index)`
  repeated until the requested size is reached;
- do not use sparse allocation tools as the primary generator;
- do not use all-zero files, because providers or filesystems may optimize them
  differently from ordinary media/user data.

### Dataset A: many files, 5 GiB total

Measurement expectations:

- exposes per-file overhead across a realistically large transfer;
- includes files below and above the Google Drive 5 MiB multipart/resumable
  boundary;
- keeps the file count high enough to expose KIO/MTP and cloud API latency.

Shape:

- 1,000 files total;
- 10 subdirectories;
- 100 files per subdirectory;
- total size: 5,120 MiB, or 5 GiB.

Directory layout:

- `group-00` through `group-09`;
- each group contains the same file mix;
- file names use this pattern:
  `a-{group}-{index}-{size}.txt`.

Per-directory file mix:

| Index range | Count | Size each | Extension |
| --- | ---: | ---: | --- |
| `000`..`055` | 56 | 1 MiB | `.txt` |
| `056`..`069` | 14 | 4 MiB | `.txt` |
| `070`..`079` | 10 | 8 MiB | `.txt` |
| `080`..`099` | 20 | 16 MiB | `.txt` |

Dataset totals by type:

| Count | Size each | Total |
| ---: | ---: | ---: |
| 560 | 1 MiB | 560 MiB |
| 140 | 4 MiB | 560 MiB |
| 100 | 8 MiB | 800 MiB |
| 200 | 16 MiB | 3,200 MiB |

Expected use:

- this is the primary many-file benchmark;
- all Dataset A files are candidates for bounded local/provider staged batch
  upload because the current per-file batch limit is 16 MiB;
- after optimization, compare total wall-clock and per-size-group behavior.

### Dataset B: medium files

Expected use:

- crosses the 5 MiB Google Drive multipart/resumable threshold;
- checks mixed multipart/resumable upload behavior inside one batch and whether
  staging overhead dominates medium files.

Shape:

- 100 files total;
- 4 subdirectories;
- total size: 400 MiB.

Directory layout:

- `below-threshold-2m`;
- `below-threshold-4m`;
- `above-threshold-6m`;
- `above-threshold-8m`.

File mix:

| Directory | Count | Size each | Extension | Total |
| --- | ---: | ---: | --- | ---: |
| `below-threshold-2m` | 40 | 2 MiB | `.txt` | 80 MiB |
| `below-threshold-4m` | 30 | 4 MiB | `.txt` | 120 MiB |
| `above-threshold-6m` | 20 | 6 MiB | `.txt` | 120 MiB |
| `above-threshold-8m` | 10 | 8 MiB | `.txt` | 80 MiB |

File names use this pattern:

- `b-2m-{index}.txt`, index `000`..`039`;
- `b-4m-{index}.txt`, index `000`..`029`;
- `b-6m-{index}.txt`, index `000`..`019`;
- `b-8m-{index}.txt`, index `000`..`009`.

Measurement expectations:

- all Dataset B files are candidates for bounded local/provider staged batch
  upload because the current per-file batch limit is 16 MiB;
- compare upload behavior around the Google Drive 5 MiB boundary.

### Dataset C: large files

Expected use:

- exposes staging-space pressure;
- measures sequential source materialization plus destination upload;
- verifies that large files remain on the existing one-file staging path.

Shape:

- 4 files total;
- 1 subdirectory;
- total size: 3,840 MiB, or 3.75 GiB.

Directory layout:

- `large`.

File mix:

| File | Size | Extension |
| --- | ---: | --- |
| `c-large-512m.txt` | 512 MiB | `.txt` |
| `c-large-768m.txt` | 768 MiB | `.txt` |
| `c-large-1024m.txt` | 1,024 MiB | `.txt` |
| `c-large-1536m.txt` | 1,536 MiB | `.txt` |

If available temp space or account limits make Dataset C too expensive, use
Dataset C-lite for cancellation smoke tests only:

| File | Size | Extension |
| --- | ---: | --- |
| `c-lite-256m.txt` | 256 MiB | `.txt` |
| `c-lite-512m.txt` | 512 MiB | `.txt` |

Dataset C-lite is not a replacement for final large-file numbers. Mark C-lite
results separately.

Measurement expectations:

- checks large-file throughput and cleanup behavior;
- checks temp-space requirements;
- checks app responsiveness during long blocking provider calls;
- should not be used to evaluate small-file batch changes.

### Dataset manifest

For each dataset, create a manifest file outside the dataset root:

- `fm-transfer-dataset-a-small.manifest.tsv`;
- `fm-transfer-dataset-b-medium.manifest.tsv`;
- `fm-transfer-dataset-c-large.manifest.tsv`;
- `fm-transfer-dataset-c-lite.manifest.tsv`, only when C-lite is used.

Manifest columns:

```text
relative_path<TAB>size_bytes<TAB>sha256
```

Use the manifest to verify:

- local generated dataset before uploading/copying to a provider;
- downloaded/staged destination copies when practical;
- final file count and total bytes on the destination provider.

### Measurements to Capture

For each run:

- wall-clock elapsed time;
- total bytes;
- file count;
- mean throughput;
- temp filesystem used/free before and during transfer;
- whether transfer was canceled successfully;
- final destination file count and total size;
- app responsiveness during transfer.

### Useful Existing Logs

Enable provider batch logging:

```bash
FMQML_PROVIDER_BATCH_LOG=1 build/fm
```

Enable Google Drive upload logs:

```bash
FMQML_GDRIVE_UPLOAD_LOG=1 build/fm
```

When Google Drive appears to stall, keep `FMQML_GDRIVE_UPLOAD_LOG=1` enabled.
It now emits `GDrive upload no progress` lines while an individual multipart or
resumable upload has not reported progress. The default idle log interval is 15
seconds. Override it with:

```bash
FMQML_GDRIVE_UPLOAD_STALL_LOG_MS=5000 build/fm
FMQML_GDRIVE_UPLOAD_STALL_LOG_MS=0 build/fm
```

Adjust Google Drive small upload concurrency:

```bash
FMQML_GDRIVE_UPLOAD_CONCURRENCY=1 build/fm
FMQML_GDRIVE_UPLOAD_CONCURRENCY=6 build/fm
FMQML_GDRIVE_UPLOAD_CONCURRENCY=12 build/fm
```

Enable MEGA timing logs:

```bash
FM_MEGA_TIMING=1 build/fm
```

For provider-to-provider scenarios, `FMQML_PROVIDER_BATCH_LOG=1` is useful to
verify whether the new staged batch path was selected. Expected markers:

- `Provider staged directory batch upload`;
- `Provider staged batch wave`;
- destination-specific batch logs such as `GDrive batch upload started`.

## Implementation Rule

Do not add streaming provider-to-provider transfer APIs for this work.

All optimization work must keep the current local-file contract:

- sources materialize data through `copyToLocalFile()`;
- destinations consume data through `copyFromLocalFile()` or
  `copyFromLocalFiles()`;
- temporary files are allocated through `CleanupSubsystem`;
- every new optimization must have a fallback to the current one-file staging
  path.

Rationale:

- local-file staging already matches all current providers;
- Google Drive already has a local-file batch upload path;
- MEGA SDK upload currently takes a local path;
- KIO/MTP copy-out is already local-file oriented;
- streaming would add new lifecycle, cancellation, and retry failure modes
  before we have timing data proving it is worth the risk.

## Logging Plan

Add one new log gate:

```bash
FMQML_PROVIDER_TRANSFER_TIMING=1 build/fm
```

Add logs in the provider-to-provider branch of `OperationQueue::copyPath()`.
Keep them summary-level only; do not log every progress callback.

### Per-file fields

Log one `[ProviderTransferFile]` line after each provider-to-provider file
finishes, fails, or is canceled:

- operation id;
- source provider scheme;
- destination provider scheme;
- source path basename;
- destination path basename;
- file size;
- staging path parent, not the full temporary filename;
- staging allocation elapsed ms;
- source `copyToLocalFile()` elapsed ms;
- destination `copyFromLocalFile()` elapsed ms;
- cleanup scheduling elapsed ms;
- total file elapsed ms;
- result: success, failed, canceled;
- error category or short error text when failed.

### Aggregate fields

Log one `[ProviderTransferSummary]` line when the operation finishes:

- operation id;
- file count;
- total bytes;
- successful files;
- failed files;
- canceled flag;
- total staging ms;
- total upload ms;
- total cleanup ms;
- wall-clock elapsed ms;
- effective throughput based on wall-clock;
- staging throughput based on source materialization time;
- upload throughput based on destination upload time.

### Measurement checklist

For every test run, write down:

- command line and enabled env vars;
- dataset id;
- source location;
- destination location;
- expected file count;
- expected byte count;
- manual wall-clock elapsed time if the run is not provider-to-provider.

For provider-to-provider runs, also write down:

- source provider from `[ProviderTransferFile] sourceScheme`;
- destination provider from `[ProviderTransferFile] destinationScheme`;
- `[ProviderTransferSummary] files`;
- `[ProviderTransferSummary] bytes`;
- `[ProviderTransferSummary] wallMs`;
- `[ProviderTransferSummary] stagingMs`;
- `[ProviderTransferSummary] uploadMs`;
- `[ProviderTransferSummary] cleanupMs`;
- `[ProviderTransferSummary] effectiveMiBs`;
- `[ProviderTransferSummary] stagingMiBs`;
- `[ProviderTransferSummary] uploadMiBs`;
- `[ProviderTransferSummary] success`, `failed`, and `canceled`;
- peak temp filesystem usage observed externally;
- final destination file count and byte count.

For the local folder -> Google Drive control run, use
`FMQML_PROVIDER_BATCH_LOG=1` and `FMQML_GDRIVE_UPLOAD_LOG=1`; the
`[ProviderTransfer...]` logs are not expected because that path does not stage
through the provider-to-provider branch.

### Provider-specific logs to keep

- `FMQML_GDRIVE_UPLOAD_LOG=1` for Google Drive upload internals.
- `FM_MEGA_TIMING=1` for MEGA SDK transfer timing and batch summaries.
- `FM_MEGA_DOWNLOAD_ITEM_TIMING=1` for per-file MEGA batch download start/finish
  rows.
- `FMQML_PROVIDER_BATCH_LOG=1` to verify when batch upload paths are used.

## Manual Baseline Notes

These results were captured manually with Dataset A
`/home/tankred/fm-transfer-dataset-a-small`.

| Case | Source | Destination | Result |
| --- | --- | --- | --- |
| 1 | local folder | Google Drive | Completed in about 20 minutes. UI stayed responsive. Existing batch path reported 700 files / 1,120 MiB in batch and 300 large files before the 16 MiB threshold change. |
| 2 | phone / MTP | Google Drive | Initial directory copy was blocked by portable provider copy rules. After allowing portable directory sources, the copy ran but used one-file staging/upload; user stopped at about 10% after about 3 minutes. Projected wall time: about 30 minutes. |
| 3 | phone / MTP | MEGA | User stopped at about 10% after about 1 minute 20 seconds. Projected wall time: about 13 minutes 20 seconds. |

Optimized retest notes:

- local folder -> Google Drive initially used the new full batch path, but the
  first source tree was duplicated: `smallFiles 2000`, `smallBytes
  10737418240`, `largeFiles 0`. At 31% after 6 minutes, the projected 10 GiB
  wall time was about 19 minutes 20 seconds. This confirms the 16 MiB batch
  selection but is not a valid Dataset A timing.
- a clean local -> Google Drive retest then hit Google Drive quota/throttling
  (`user exceeds...`). Treat this run as invalid for elapsed-time comparison.
- clean local folder -> Google Drive retest checkpoint: 12% after 1 minute 10
  seconds. Projected full Dataset A wall time: about 9 minutes 43 seconds,
  assuming the rate holds and no further Google Drive throttling occurs.
- phone / MTP -> Google Drive optimized retest entered the staged batch path:
  `files 1000`, `bytes 5368709120`, `largeFiles 0`, max 64 files / 128 MiB
  per wave. First waves uploaded 24 files / 126,877,696 bytes in 6,646 ms,
  27 files / 134,217,728 bytes in 9,970 ms, 22 files / 134,217,728 bytes in
  7,747 ms, 26 files / 124,780,544 bytes in 13,316 ms, and 28 files /
  133,169,152 bytes in 12,539 ms, all with 0 retries. Checkpoint: 13% after
  about 1 minute. Projected full Dataset A wall time: about 7 minutes 42
  seconds if the rate holds.
- phone / MTP -> MEGA retest was canceled at about 10% after about 1 minute 20
  seconds. `[ProviderTransferSummary]` reported 99 files, 535,822,336 bytes,
  wallMs 80,544, stagingMs 15,897, uploadMs 60,630, cleanupMs 469,
  effectiveMiBs 6.34436, stagingMiBs 32.1444, uploadMiBs 8.17661, success 98,
  canceled 1, failed 0.
- Google Drive -> MEGA ad-hoc small-image folder test: source was not Dataset
  A, because Dataset A was not present on Google Drive; source folder contained
  about 300 images. Checkpoint: 10% after about 1 minute 30 seconds. Per-file
  logs show very small files, roughly 4 KiB to 60 KiB, with staging usually
  around 0.6-0.9 seconds and MEGA upload often around 0.7-3.4 seconds per file.
  Treat this as evidence of high per-file overhead for tiny GDrive -> MEGA
  transfers, not as a Dataset A benchmark.
- Google Drive -> MEGA ad-hoc image folder on the old build completed
  successfully according to `[ProviderTransferSummary]`: files 78, bytes
  98,285,732, wallMs 140,915, stagingMs 65,215, uploadMs 74,193, cleanupMs
  378, effectiveMiBs 0.665171, stagingMiBs 1.43729, uploadMiBs 1.26336,
  success 78, failed 0, canceled 0. User-facing count was reported as 87
  files; keep this discrepancy in mind when comparing provider summary counts
  with visible folder item counts.
- local folder -> MEGA Dataset A retests after adding MEGA batch upload:
  concurrency 2 completed in about 8 minutes 14 seconds, concurrency 3 completed
  in about 6 minutes 12 seconds, and concurrency 4 completed in about 5 minutes
  18 seconds with no observed errors. Default MEGA upload concurrency is now 4.
- phone / MTP -> MEGA full Dataset A completed in about 7 minutes 40 seconds.
  Earlier wave logs showed staging around 30-32 MiB/s and upload as the
  dominant phase. This is the current MEGA baseline and should be left unchanged
  for now.
- phone / MTP -> Google Drive full Dataset A completed in about 11 minutes 24
  seconds. `[ProviderTransferSummary]` reported files 1000, bytes
  5,368,709,120, wallMs 683,154, stagingMs 159,220, uploadMs 511,684,
  cleanupMs 190, effectiveMiBs 7.49, stagingMiBs 32.16, uploadMiBs 10.01,
  success 1000, failed 0, canceled 0.
- Google Drive staged waves had no retries in the captured full run, but upload
  throughput varied substantially by wave. Some waves uploaded around
  17.4 MiB/s, while slow waves dropped to about 3.9-5.4 MiB/s. Multipart uploads
  can still sit after the request body is sent, waiting for a Drive response.
- After large Google Drive write/delete bursts, Drive can become hard to use for
  several minutes, including through the web UI. Treat this as a Drive API
  rate/throttle/cooldown problem rather than a phone staging problem.
- A later phone / MTP -> Google Drive Dataset A run reached about 95%, then hit
  repeated `User rate limit exceeded` responses. The first cooldown layer
  detected the throttle, but in-flight uploads still retried inside the same
  wave and caused a retry storm with cooldowns growing from about 16 seconds to
  about 64 seconds. That run did not complete.
- After changing upload rate limits to hard-stop the current batch instead of
  retrying inside it, phone / MTP -> Google Drive Dataset A completed
  successfully again. `[ProviderTransferSummary]` reported files 1000, success
  1000, bytes 5,368,709,120, wallMs 636,205, stagingMs 158,915, uploadMs
  465,093, cleanupMs 174, effectiveMiBs 8.05, stagingMiBs 32.22, uploadMiBs
  11.01. No upload retries were reported in the captured log. Individual waves
  still varied widely, with upload throughput from about 8.4 MiB/s to
  21.4 MiB/s in the sample.
- A Ctrl+A phone / MTP -> MEGA test with 35 media files originally used the
  one-file staging path because the selected set contained videos above the
  old 16 MiB staged-batch file limit. The multi-selected portable-file path was
  first changed to batch eligible small files, then changed again to keep all
  selected files in the staged-batch scheduler with 64 file / 128 MiB waves.

Observed console marker for case 1:

```text
Provider mixed directory batch upload source "/home/tankred/fm-transfer-dataset-a-small" destination "gdrive://new/root/fm-transfer-dataset-a-small" smallFiles 700 smallBytes 1174405120 largeFiles 300
GDrive batch upload started files 700 bytes 1174405120 concurrency 6
```

## Implemented Optimization Status

Current code changes:

- local/provider batch candidate limit raised from 5 MiB to 16 MiB;
- Google Drive `copyFromLocalFiles()` no longer rejects files above the
  multipart threshold, so a batch can contain both multipart and resumable
  uploads;
- portable provider now allows copying files and directories below a concrete
  device path, while keeping the provider root blocked;
- `OperationQueue` now has narrow `non-local provider directory ->
  batch-capable provider` and `multi-selected non-local provider files ->
  batch-capable provider` staged batch paths with 64 files / 128 MiB waves.
  Mixed selections keep large files in the batch scheduler; files above the
  wave byte target become single-file waves;
- staged batch diagnostics log `[ProviderStagedBatchWave]` with staging,
  upload, cleanup, and wall timings;
- staged batch preflight skips per-file destination existence checks for fresh
  cloud folders, avoiding one class of Google Drive metadata pressure;
- MEGA exposes `supportsLocalFileBatchCopy()` and `copyFromLocalFiles()` using
  SDK uploads with default concurrency 4;
- Google Drive now has a process-wide API cooldown gate shared by metadata
  create/list/trash/restore and upload requests. Retryable/rate-limit responses
  set a cooldown before later Drive API calls continue;
- Google Drive upload now treats `User rate limit exceeded` as a hard stop for
  the current upload batch. Multipart and resumable uploads record the cooldown,
  suppress same-batch retries, and allow queued upload workers to cancel instead
  of resuming together after the cooldown;
- provider-to-provider one-file staging has optional timing logs behind
  `FMQML_PROVIDER_TRANSFER_TIMING=1`.

Expected impact:

- case 1 should improve because all 1,000 Dataset A files are now batch
  candidates for Google Drive;
- case 2 should improve because the phone source is staged in bounded waves and
  uploaded through Google Drive batch upload;
- case 3 now uses the same staged directory batch path because MEGA implements
  `copyFromLocalFiles()` and uses SDK upload concurrency.

Current decision:

- keep MEGA unchanged for now; current local and phone benchmarks are acceptable
  and MEGA remains responsive after large operations;
- focus the next investigation on Google Drive API pressure, throttle recovery,
  and avoiding post-operation unavailability.

## Next Test Order

Run tests in this order after the current optimization build.

### Phase 1: optimized Dataset A retest

Run all tests with:

```bash
FMQML_PROVIDER_TRANSFER_TIMING=1 \
FMQML_PROVIDER_MATERIALIZE_LOG=1 \
FMQML_PROVIDER_BATCH_LOG=1 \
FMQML_GDRIVE_UPLOAD_LOG=1 \
FM_MEGA_TIMING=1 \
build/fm
```

Test cases:

1. Local folder -> Google Drive, Dataset A.
2. Phone -> Google Drive, Dataset A.
3. Phone -> MEGA, Dataset A.

Purpose:

- local folder -> Google Drive verifies the 16 MiB batch limit and mixed
  multipart/resumable batch behavior;
- phone -> Google Drive verifies `Provider staged directory batch upload` and
  compares directly against the stopped 10% / 3 minute run;
- phone -> MEGA verifies the same staged batch path against MEGA's SDK upload
  queue.

### Phase 2: cancellation and cleanup tests

Run these after the Dataset A retest, because batching increases cleanup
surface and needs explicit cancellation coverage.

Test cases:

1. Cancel Dataset A phone -> Google Drive during source staging.
2. Cancel Dataset A phone -> Google Drive during upload.
3. Cancel Dataset C phone -> Google Drive during source staging.
4. Cancel Dataset C phone -> Google Drive during upload.
5. Repeat the same four tests for phone -> MEGA.

Capture:

- whether UI returns to idle;
- whether temp staging directories are scheduled for cleanup;
- whether partial destination files remain;
- whether rerunning the same transfer hits conflicts or stale temp files;
- whether closing the app during an active transfer exits cleanly.

### Phase 3: Google Drive throttle and recovery

Use the optimized retest results to design Drive-specific rate limiting. The
current problem is not phone staging speed; it is Google Drive becoming
unavailable or very slow after large write/delete bursts.

Capture:

- whether the previous operation included mass trash/delete, upload, or both;
- whether the next operation stalls before upload (`makePath`, `files.list`, or
  folder creation) or during upload;
- whether Google Drive web UI is also unavailable during the cooldown;
- any 403/429/rate limit text;
- wave-level upload throughput and `GDrive upload no progress` lines.

## Counting Rules

For every benchmark run, record:

- dataset id;
- source provider;
- destination provider;
- file count;
- total bytes;
- elapsed wall-clock ms;
- staging ms;
- upload ms;
- cleanup ms;
- effective throughput MiB/s;
- staging throughput MiB/s;
- upload throughput MiB/s;
- peak temp bytes used;
- final destination file count;
- final destination total bytes;
- canceled flag;
- failure count.

Derived formulas:

- effective throughput = total bytes / wall-clock elapsed;
- staging throughput = total bytes staged / staging elapsed;
- upload throughput = total bytes uploaded / upload elapsed;
- per-file overhead estimate = wall-clock elapsed minus measured staging,
  upload, and cleanup time.

## Bottom Line

The requested cloud/phone scenarios stay local-file based. The only approved
optimization direction is better staging and batching of local temporary files.

- For many small files, per-file overhead is the main performance risk.
- For large files, sequential staging plus upload is the main performance risk.
- Google Drive now gets local-file batches for files up to 16 MiB and can use
  that path from portable directory staged batches.
- MEGA now has local-file batch upload through the SDK queue, with upload
  concurrency controlled by `FMQML_MEGA_UPLOAD_CONCURRENCY`.
- MEGA is currently the stable baseline: phone -> MEGA Dataset A completed in
  about 7 minutes 40 seconds and did not make the provider unusable afterward.
- Google Drive is the current risk area: phone -> Google Drive Dataset A
  completed successfully in about 11 minutes 24 seconds, but large write/delete
  bursts can push Drive into a cooldown where both app and web UI are difficult
  to use.

## Next Work Plan

Use this as the starting point for the next optimization pass.

### 1. MEGA local-file batch upload: implemented, do not change for now

Implemented goal:

- make MEGA expose `supportsLocalFileBatchCopy()` and `copyFromLocalFiles()`;
- keep the implementation local-file based;
- start with conservative upload concurrency.

Implemented behavior:

- default MEGA batch upload concurrency: 4;
- configurable with `FMQML_MEGA_UPLOAD_CONCURRENCY`, clamped to 1..4;
- current `copyFromLocalFile()` behavior is preserved for single files;
- cancellation is routed through existing MEGA cancellation behavior;
- batch start/finish and SDK upload connection logs are behind
  `FM_MEGA_TIMING=1`.

Why this was first:

- phone -> MEGA showed upload-dominated timing;
- Google Drive -> MEGA also spends a large share of wall time in MEGA upload;
- this improves the destination side without adding streaming APIs.

Verification status:

- local -> MEGA Dataset A: 5 minutes 18 seconds with concurrency 4;
- phone / MTP -> MEGA Dataset A: 7 minutes 40 seconds;
- no observed MEGA post-operation unavailability.

Remaining MEGA checks:

- cancellation during staged batch upload;
- temp staging cleanup after cancellation.

### 2. Generalized staged batch to provider -> batch-capable provider

Implemented:

- staged batch now accepts non-local source providers, not only portable
  devices, when the destination supports `copyFromLocalFiles()`;
- target paths include Google Drive -> MEGA, MEGA -> Google Drive, and phone /
  MTP -> both cloud providers.

Rules:

- still materialize files locally with `copyToLocalFile()`;
- still upload through `copyFromLocalFiles()`;
- keep the current 64 files / 128 MiB wave limits;
- fall back to the existing one-file provider-to-provider path on conflict,
  unsupported files, unsupported destination provider, or batch failure.

Verification:

- Google Drive -> MEGA and MEGA -> Google Drive should emit staged batch logs
  instead of only `[ProviderTransferFile]` one-file logs;
- phone / MTP -> MEGA improved compared with the 10% in 1 minute 20 seconds
  checkpoint;
- Google Drive -> MEGA small-image folder should improve over the old-build
  summary of 78 files / 98,285,732 bytes / 140,915 ms.

### 3. Materialization pipeline for cloud -> cloud

Goal:

- overlap source materialization for wave N+1 with destination upload for wave
  N;
- keep all transfer data in local temporary files;
- do not add a streaming provider API.

Why this is next:

- MEGA -> Google Drive now reaches staged batch upload, but observed waves spend
  more wall time downloading from MEGA into staging than uploading to Google
  Drive;
- the current phase order leaves the destination upload idle while source
  materialization completes;
- pipeline should improve wall time when both staging and upload are
  significant, and should improve perceived progress by reducing long staging
  stalls.

Guardrails:

- keep cleanup ownership explicit for each wave's staging directory;
- keep cancellation semantics simple: cancel current staging, current upload,
  and any queued next-wave staging;
- bound disk usage by limiting how many staged waves can exist at once;
- keep the current non-pipelined path available as a fallback if a provider does
  not tolerate concurrent source and destination operations.

Open questions:

- whether MEGA download throughput improves, regresses, or rate-limits when a
  second MEGA download wave overlaps with Google Drive upload;
- whether Google Drive source downloads show the same materialization bottleneck
  in Google Drive -> MEGA;
- whether adaptive wave byte limits are enough to improve UX before full
  pipelining.

Initial design constraint:

- use at most one staging wave in progress and one upload wave in progress;
- keep total staged bytes bounded;
- never start a new wave if cancellation is already requested.

### 4. Google Drive rate limiting and cooldown behavior

Goal:

- prevent large Google Drive write/delete bursts from making Drive unusable for
  subsequent operations;
- keep normal-case upload fast, but back off when Drive shows rate-limit or
  slow-response signals.

Capture on the next Google Drive stall:

- operation type: local -> Google Drive or phone / MTP -> Google Drive;
- current wave file count and bytes;
- `GDrive upload no progress` mode, name, bytes, sent, total, idleMs,
  elapsedMs, count;
- any 429/500/503 or `user exceeds` error text;
- whether progress resumes without user action.

Possible follow-up:

- validate the hard-stop rate-limit behavior with a mass trash/delete run;
- add an adaptive GDrive limiter that lowers active upload pressure after 403,
  429, `User rate limit exceeded`, or many simultaneous post-body response
  stalls;
- separate multipart and resumable pressure so many tiny files do not starve or
  throttle larger resumable uploads;
- defer Drive refresh/list/quota calls after large write bursts and rely on
  local cache first;
- make mass trash/delete use a Drive-aware queue with backoff and cooldown;
- expose a clear cooling-down UI/status instead of leaving operations at
  `Starting...`.

### 5. Clean up provider transfer logs

Goal:

- make transfer logs easier to read during manual testing.

Changes:

- log source and destination basenames by default;
- keep full encoded provider paths behind a verbose flag;
- keep operation id, provider schemes, byte counts, and timing fields;
- avoid printing long `portable://...mtp%3A...` source paths in normal timing
  logs.

Success criteria:

- tester can identify the current file/wave without decoding provider URLs;
- logs still contain enough data to compare staging, upload, cleanup, and wall
  time.

### 6. Source-side batch materialization

Finding:

- MEGA -> local showed strictly serial `[ProviderMaterializeFile]` rows;
- MEGA -> Google Drive staged batches uploaded in parallel, but each wave spent
  most of its time materializing files from MEGA first;
- the same bottleneck is expected for any source provider whose
  `copyToLocalFile()` is called one file at a time.

Implemented first:

- `FileProvider` now has a capability-gated `copyToLocalFiles()` API for
  provider -> local materialization;
- MEGA implements the API with bounded SDK download concurrency;
- `FMQML_MEGA_DOWNLOAD_CONCURRENCY` controls MEGA download concurrency
  from 1 to 4, default 4;
- provider -> provider staged waves use source batch materialization when the
  source provider supports it;
- MEGA directory/file copies to local fresh destinations use the same batch
  materialization path, so local download tests isolate the download phase.

Diagnostics:

- `FM_MEGA_TIMING=1` logs:
  - `[MegaTiming] provider batch download start`;
  - `[MegaTiming] provider batch download finish`;
- `FM_MEGA_DOWNLOAD_ITEM_TIMING=1` additionally logs:
  - `[MegaTiming] provider batch download item start`;
  - `[MegaTiming] provider batch download item finish`;
- `FMQML_PROVIDER_MATERIALIZE_LOG=1` logs aggregate
  `[ProviderMaterializeWave]` rows for batch waves;
- sequential fallback still logs per-file `[ProviderMaterializeFile]` rows.

Validation scenarios:

- MEGA folder with many small photos -> local empty folder;
- MEGA folder with mixed photos and videos -> local empty folder;
- MEGA folder -> Google Drive folder;
- flat multi-select MEGA files -> local empty folder;
- flat multi-select MEGA files -> Google Drive folder.

Expected result:

- for MEGA sources, logs should show up to four concurrent download item starts;
- local MEGA download should no longer show only `waveFiles=1` per file;
- staged MEGA -> Google Drive should reduce `stagingMs` before upload waves.

Still intentionally not parallelized:

- phone / MTP source materialization: current implementation goes through
  blocking device/KIO copy; keep it serial unless dedicated device testing shows
  safe parallel behavior.

Added next:

- Google Drive implements the same source-side `copyToLocalFiles()` API with
  bounded parallel HTTP downloads;
- `FMQML_GDRIVE_DOWNLOAD_CONCURRENCY` controls Google Drive source download
  concurrency from 1 to 8, default 4;
- `FMQML_GDRIVE_DOWNLOAD_LOG=1` logs Google Drive batch download scheduler
  start/finish summaries;
- `FMQML_GDRIVE_DOWNLOAD_RANGE_LOG=1` additionally logs queued download ranges.
