# Provider Batch Operation Label Plan

## Goal

Make operation drawer labels for provider batch transfers predictable and
user-facing. Internal staging filenames such as `transfer-00007.png` must never
appear in the drawer.

The change is presentation-only. It must not alter transfer scheduling,
provider APIs, staging file allocation, conflict handling, retry behavior,
cleanup, or progress byte accounting.

## Current Problem

`OperationQueue` uses cleanup-managed staging files for provider-to-provider
batch transfers. Staged payloads are named with internal temporary names:

- `transfer-00000.ext`;
- `transfer-00001.ext`;
- and so on.

During the upload phase, destination providers receive `LocalFileCopyItem`
values whose `sourceFilePath` is the staging path. The provider progress
callback reports that local staging path back to `OperationQueue`, and
`OperationQueue` currently turns it into a drawer label by taking
`QFileInfo(currentFilePath).fileName()`.

Result: users can see internal names like `transfer-00007.png` while the app is
uploading between providers. That looks like files are being renamed and makes
batch transfer state harder to trust.

## Scope

In scope:

- provider-to-provider staged directory batch labels;
- multi-selected non-local provider files copied to a batch-capable provider;
- mixed non-local provider selections containing both files and directories;
- provider fallback one-file staging labels, but only as a clearly marked
  sequential/fallback mode;
- labels shown through `OperationQueue::currentLabel`.

Out of scope:

- changing `OperationsDrawer.qml` layout or styling;
- changing provider transfer contracts;
- adding streaming provider-to-provider transfers;
- making portable/MTP materialization parallel;
- adding extra full-tree scans only to compute nicer UI counts;
- changing logging formats except where a log references a new label helper
  during debugging.

## Label Principles

1. Do not expose implementation artifacts.
   - Never show cleanup lease ids, staging directories, or `transfer-NNNNN`
     payload names in the drawer.

2. Use real filenames only when they are truthful and stable.
   - Local sequential operations may continue to show the current filename.
   - One-file fallback provider transfers may show the real source filename,
     with an explicit fallback/sequential label.
   - Batch upload/download phases should not show a rapidly changing real
     filename when multiple workers may be active.

3. Prefer phase plus batch context for batch paths.
   - Examples:
     - `Downloading batch 2/8`
     - `Uploading batch 2/8`
     - `Uploading 24 files to Google Drive`
   - The exact copy should be short enough for the compact drawer chip.
   - `N/M` labels are required for provider staged waves when the total can be
     computed from the already-built batch plan.

4. Avoid expensive counting.
   - Use counts already known in the current function: `batchFiles.size()`,
     current wave index, `waveFiles.size()`, and source/destination provider
     names when available cheaply.
   - Do not add a second scan, remote list, or metadata query just to compute
     a drawer label.

5. Preserve existing progress semantics.
   - Staging and upload progress bytes must remain as they are today.
   - The label change must not affect cancellation timing or progress callback
     return values.

## Existing Code Paths

### Local file batches

`copySmallLocalFilesToProviderBatch()` and
`copyNextSmallLocalFilesToProviderBatch()` pass real local paths to
`copyFromLocalFiles()`.

Expected label behavior:

- single-file local uploads may keep showing the real filename;
- multi-file local batch uploads should use stable batch labels such as
  `Uploading batch` or `Uploading batch N` when a batch index is already known;
- do not show rapidly changing filenames while several upload workers may be
  active.

Reason: local upload items are real user files, but parallel batch callbacks can
still make the drawer jump between filenames. The drawer should describe the
operation mode, not worker scheduling noise.

### Local directory to provider batch

`copyLocalDirectoryToProviderBatch()` also passes real local child paths to
`copyFromLocalFiles()`.

Expected label behavior:

- keep real filenames only for one-file/sequential upload fallback;
- use stable `Uploading batch N` labels for multi-file directory batch upload
  where the current loop already tracks the batch index;
- keep existing labels such as `Scanning upload folder...` and
  `Creating upload folder...`;
- do not introduce extra counting scans.

Reason: this path is not the source of `transfer-NNNNN` drawer labels, but it
is still parallel batch upload and should use the same user-facing convention.

### Provider directory to provider staged batch

`copyProviderDirectoryToProviderStagedBatch()` builds waves from `batchFiles`.
Each wave allocates a staging directory and creates internal staged payloads
named `transfer-NNNNN.ext`.

Expected label behavior:

- during preflight:
  - keep or slightly clarify existing phase labels:
    - `Preparing upload folder...`
    - `Scanning upload files...`
- during source materialization:
  - show a batch materialization phase, not staging filenames;
  - if source-side batch materialization is used, show a stable wave label such
    as `Downloading batch 2/8`;
  - if the source provider falls back to one-file materialization inside the
    staged wave, it is acceptable to show a stable phase label such as
    `Reading batch 2/8`;
  - MTP/portable remains sequential internally, but it should not require a new
    provider API or parallelization.
- during destination upload:
  - show `Uploading batch 2/8` or `Uploading 24 files to <provider>`;
  - do not derive the label from the upload callback `currentFilePath`, because
    that path is the staging file.

### Provider multi-select files to provider staged batch

`copyProviderFilesToProviderStagedBatch()` handles selection-level staged
batching before the generic top-level copy loop.

Expected label behavior:

- use the same helper and message format as directory staged batches;
- do not show per-file names during batch upload;
- use already-known `batchFiles.size()`, `waveFiles.size()`, and wave index.

Expected scheduling behavior:

- a mixed provider selection such as one folder plus several files should be
  expanded into one staged selection batch plan;
- selected folders should create their destination folders and contribute their
  child files to the same wave scheduler as selected top-level files;
- selected top-level files must not fall through to the generic sequential
  provider-to-provider path merely because the selection also contains a
  folder;
- empty selected folders may complete through the selection batch path after
  their destination folders are created.

### Provider to local batch materialization

`copyProviderDirectoryToLocalBatch()` and `copyProviderFilesToLocalBatch()`
materialize provider files directly to local destination paths.

Expected label behavior:

- leave existing source-file labels intact for now;
- these paths do not upload temporary staging files to a destination provider.

### One-file provider fallback path

The generic provider-to-provider path inside `copyPath()` stages one file and
then uploads one file.

Expected label behavior:

- keep the real source filename visible;
- make the mode explicit enough that the drawer does not look like optimized
  batch mode;
- candidate labels:
  - `Transferring file in fallback mode: <name>`
  - `Sequential transfer: <name>`
  - `Fallback transfer: <name>`

The final wording should be short because the drawer has compact surfaces.
`Sequential transfer: <name>` is the preferred first candidate.

## Proposed Implementation

### 1. Add small label helpers in `OperationQueue.cpp`

Keep helpers local to `OperationQueue.cpp`; do not expose new QML API.

Candidate helpers:

```cpp
QString providerDisplayName(FileProvider *provider);
QString batchPhaseLabel(QStringView phase, int waveIndex, int waveCount);
QString batchUploadLabel(int waveFileCount, FileProvider *destinationProvider);
void postCurrentLabel(const QString &label);
```

Guidance:

- `providerDisplayName()` should be cheap. Prefer `provider->scheme()` unless a
  safe existing display-name API is already available in this layer.
- `batchPhaseLabel()` should accept counts already known by the caller.
- `postCurrentLabel()` can wrap the existing `QMetaObject::invokeMethod`
  pattern to avoid duplicating lambda boilerplate.

Do not add provider calls that may hit the network.

### 2. Compute wave count without extra scans

The staged batch functions already have `batchFiles` before wave processing.
Calculate wave count from the same loop constraints used for execution:

- max `ProviderStagedBatchMaxFiles`;
- max `ProviderStagedBatchMaxBytes`;
- existing `entryInfo()` values already used while building waves.

Implementation options:

Preferred implementation:

- store file sizes in the staged `batchFiles` plan when metadata is already
  available;
- compute `waveCount` from that plan with the same max-file and max-byte limits
  used by execution;
- do not call provider metadata APIs again just to compute the label total;
- include `waves` in provider batch logs so manual testing can compare logs and
  drawer labels.

### 3. Replace upload-phase staging-name labels

In both staged batch functions, replace this upload callback behavior:

```cpp
const QString fileName = QFileInfo(currentFilePath).fileName();
setCurrentLabel(fileName);
```

with a stable batch label set before and during upload:

- before `destProvider->copyFromLocalFiles(...)`:
  - set `Uploading batch <waveIndex>/<waveCount>`;
  - or `Uploading <waveFiles.size()> files to <scheme>`.
- inside the progress callback:
  - do not inspect `currentFilePath`;
  - keep the stable upload label only if needed;
  - continue to update progress and metrics exactly as today.

This is the key fix that removes `transfer-NNNNN.ext` from the drawer.

### 4. Add phase labels around materialization

In the same staged batch functions:

- before source `copyToLocalFiles(...)`, set
  `Downloading batch <waveIndex>/<waveCount>`;
- before the per-file `copyToLocalFile(...)` fallback loop, set
  `Reading batch <waveIndex>/<waveCount>`;
- inside the per-file materialization callback, do not replace the batch label
  with a staging filename.

Open question for implementation:

- For sequential portable/MTP materialization, showing the real phone filename
  may still be useful. However, this plan intentionally starts conservative:
  staged provider batch phases use stable batch labels, while local and
  one-file fallback paths keep real filenames.

If testing shows `Reading batch N` feels too vague for MTP, we can add
`Reading file X in batch N` later using the already-known loop index, without
new provider work.

### 5. Mark one-file provider fallback mode

In the generic provider-to-provider branch of `copyPath()`:

- keep real source filename;
- change the current label before staging/upload to a short fallback label:
  - preferred: `Sequential transfer: <name>`;
- do not change progress byte logic or staging allocation.

This helps users distinguish:

- optimized staged batch path: phase plus batch label;
- fallback path: explicit sequential file transfer.

### 6. Keep provider implementations unchanged

Do not change:

- `FileProvider.h` callback signatures;
- `GDriveFileProviderPlugin::copyFromLocalFiles()`;
- `MegaFileProviderPlugin::copyFromLocalFiles()`;
- portable provider copy logic;
- cleanup lease allocation.

Reason: this is a drawer-label problem caused by how `OperationQueue` interprets
staged upload callback paths. Provider APIs correctly report local upload
source paths according to their current contract.

## Candidate Label Copy

First-pass labels:

| Path | Phase | Label |
| --- | --- | --- |
| local multi-file provider batch | upload | `Uploading batch` or `Uploading batch N` |
| staged provider batch | preflight | `Scanning upload files...` |
| staged provider batch | materialize, batch-capable source | `Downloading batch N/M` |
| staged provider batch | materialize, one-file source fallback | `Reading batch N/M` |
| staged provider batch | upload | `Uploading batch N/M` |
| staged provider batch | cleanup | no label change |
| generic provider fallback | staging/upload | `Sequential transfer: <name>` |
| local sequential copy/upload | current file | unchanged |

Optional second-pass labels:

| Path | Label |
| --- | --- |
| staged provider upload with destination scheme | `Uploading batch N to Google Drive` |
| large single-file wave in staged batch | `Uploading large file batch N` |

Do not implement the optional labels if they require additional remote metadata
queries or a broad provider display-name abstraction.

## Verification Plan

Manual checks:

1. Phone/MTP directory -> Google Drive.
   - Expected: no `transfer-NNNNN` labels.
   - Expected: staged batch phases show `Reading batch N/M` and
     `Uploading batch N/M`.
   - Expected: upload remains batch-capable; logs still show
     `GDrive parallel upload scheduler started`.

2. Phone/MTP directory -> MEGA.
   - Expected: same label shape as Google Drive.
   - Expected: MEGA batch upload timing remains unchanged.

3. Google Drive directory -> MEGA.
   - Expected: source materialization label uses `Downloading batch N/M`.
   - Expected: upload label uses `Uploading batch N/M`.

4. MEGA directory -> Google Drive.
   - Expected: same stable batch labels.

5. Local folder -> Google Drive.
   - Expected: existing real local filename labels remain acceptable.
   - Expected: no regression in batch upload selection.

6. Single provider file -> provider.
   - Expected: drawer shows explicit sequential/fallback wording with the real
     source filename.

Useful env vars:

```bash
FMQML_PROVIDER_BATCH_LOG=1 \
FMQML_PROVIDER_TRANSFER_TIMING=1 \
FMQML_GDRIVE_UPLOAD_LOG=1 \
FM_MEGA_TIMING=1 \
build/fm
```

The logs are only to confirm that the same transfer paths are selected; the
feature itself should be validated visually in the operation drawer.

## Success Criteria

- Internal staging names never appear in `OperationsDrawer` during provider
  staged batch upload.
- Batch provider transfers use stable phase labels instead of rapidly changing
  worker filenames.
- Local sequential operations still show real filenames.
- Provider one-file fallback is visibly sequential/fallback.
- No new transfer scans, provider network calls, or expensive count collection
  are introduced for labels.
- No changes are made to provider transfer behavior, cleanup behavior, retry
  behavior, or progress accounting.
