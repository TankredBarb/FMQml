# Linux transfer performance policy plan

## Purpose

Define one Linux-only performance and responsiveness policy for local file
copying and archive extraction.

The primary goal is to keep FM responsive during heavy transfers without
unnecessarily limiting throughput in ordinary same-storage scenarios. The
known failure case that must not return is severe desktop and FM lag while
extracting a large archive from NTFS to Btrfs.

This plan covers policy and integration boundaries. It does not prescribe a
new archive backend, replace 7-Zip, or change Windows and macOS behavior.

## Current baseline

### Archive extraction

Linux archive extraction currently uses external `7z` processes.

The current working baseline is:

- 7-Zip chooses its own thread count; FM does not pass `-mmt=1`;
- the child uses the normal CPU scheduler;
- the child is assigned `nice=19`;
- the child is assigned idle I/O priority;
- a `60 ms` active / `40 ms` paused duty cycle is enabled when the source and
  destination filesystem type identifiers differ;
- progress, speed, and ETA use the backend `processed/total` contract.

This baseline keeps the interface responsive on the tested same-filesystem
SATA SSD scenario and is intentionally retained until the shared policy is
implemented and measured.

### Local copying

The local buffered copy path already has a conservative Linux cross-filesystem
mode:

- buffer size is capped at `1 MiB`;
- the operation thread receives low best-effort I/O priority;
- source and destination cache pages are released in bounded windows;
- destination writeback is started and awaited in `32 MiB` windows.

However, the current decision is local to `OperationQueueCopy.cpp` and is based
on `QStorageInfo::device()` and mount root differences. Archive extraction uses
a separate filesystem-type comparison. The two routes can therefore classify
the same transfer differently.

Linux copy buffer selection is also incomplete: `getDriveTypeByPath()` returns
`Unknown` outside Windows even though `DriveUtils` already contains Linux
sysfs-based HDD/SSD/USB classification. As a result, Linux commonly uses the
generic `1 MiB` buffer rather than a storage-specific value.

## Design principles

1. Responsiveness has priority over peak benchmark throughput.
2. Conservative controls must be applied for a concrete topology reason, not
   globally.
3. Copying and archive extraction must consume the same classification result.
4. Unknown or ambiguous topology falls back to the safe policy.
5. Change one policy dimension at a time and measure it before further tuning.
6. Preserve cancellation, staging, conflict handling, cleanup, progress, and
   final refresh behavior.
7. Avoid a user-facing performance setting until automatic policy has been
   validated and shown to need an override.

## Storage endpoint classification

For each local source and destination path, build a storage endpoint profile:

- normalized local path;
- mount root;
- filesystem type;
- mounted device identifier;
- block-device name where available;
- parent physical block-device identity where available;
- storage type: HDD, SATA SSD, NVMe SSD, USB/removable, network, optical, or
  unknown;
- whether the filesystem is native/local, foreign/FUSE, network-like, or
  unknown.

Linux implementation should reuse and extend `DriveUtils` rather than adding a
second sysfs parser.

Physical-device identity must resolve partitions to their parent device where
possible. For example, two partitions on `sda` are different filesystems but
share one physical device. Device-mapper, multi-device filesystems, bind
mounts, and unresolved devices must degrade conservatively rather than claim a
false fast path.

## Transfer modes

### Optimized

Use when source and destination are on the same mount and the endpoint is a
known local storage device.

Intent:

- avoid unnecessary cache and scheduler restrictions;
- use a buffer appropriate for the destination storage;
- retain normal progress and cancellation cadence.

Initial copy buffer targets:

| Destination storage | Buffer |
| --- | ---: |
| HDD or USB | `512 KiB` |
| SATA SSD | `1 MiB` |
| NVMe SSD | `8 MiB` |
| Unknown | do not select Optimized |

Initial archive policy:

- automatic 7-Zip threading;
- normal CPU scheduler;
- no duty-cycle pause;
- CPU and I/O priority values must be tuned separately rather than silently
  inherited from Conservative.

### Balanced

Use when source and destination are on different known physical devices but
both endpoints are ordinary local filesystems with no high-risk combination.

Two independent SSDs can read and write in parallel, so this case must not be
treated as automatically equivalent to NTFS-to-Btrfs.

Intent:

- preserve parallel device throughput;
- bound page-cache and writeback pressure;
- keep FM and unrelated desktop I/O ahead of bulk work;
- avoid periodic `SIGSTOP` pauses unless measurements prove they are required.

Initial copy policy:

- choose the buffer using the slower endpoint, with an upper bound to be
  validated by benchmarks;
- enable bounded cache/writeback handling for local cross-mount copies;
- use reduced best-effort I/O priority;
- do not use duty-cycle pausing.

Initial archive policy:

- automatic 7-Zip threading;
- normal CPU scheduler;
- `nice=19` as the initial safe value;
- reduced best-effort I/O priority;
- no duty-cycle pausing.

### Conservative

Use when any of the following is true:

- source and destination filesystem types differ;
- source and destination are different filesystems or partitions on the same
  physical device;
- either endpoint is NTFS, another foreign/FUSE filesystem, network-like,
  removable/slow storage, or unknown;
- physical-device identity is ambiguous and a fast classification cannot be
  proven;
- a specifically measured topology is known to cause severe writeback or UI
  stalls.

`NTFS -> Btrfs` must always select Conservative.

Initial copy policy:

- buffer no larger than `1 MiB`;
- bounded `32 MiB` cache/writeback windows;
- low I/O priority;
- retain the existing cancellation and temp-file semantics.

Initial archive policy:

- automatic 7-Zip threading unless later measurements demonstrate that a
  thread cap is still required for a particular topology;
- normal CPU scheduler;
- `nice=19`;
- idle I/O priority;
- `60 ms` active / `40 ms` paused duty cycle.

The duty cycle is intentionally the strongest control and must remain scoped to
Conservative. It should not be activated merely because two independent native
Linux SSDs are involved.

## Policy decision order

The decision should be deterministic and explainable:

1. If either endpoint cannot be classified safely, choose Conservative.
2. If either endpoint is network-like, foreign/FUSE, removable/slow, or
   otherwise explicitly risky, choose Conservative.
3. If filesystem types differ, choose Conservative.
4. If both paths resolve to different filesystems on the same physical device,
   choose Conservative.
5. If both paths share the same mount, choose Optimized.
6. If both endpoints are known ordinary local filesystems on different
   physical devices, choose Balanced.
7. Otherwise choose Conservative.

Each result should carry a short reason code such as `same-mount`,
`different-filesystem-type`, `same-physical-device-cross-mount`,
`independent-local-devices`, or `unknown-topology`.

## Shared policy boundary

Introduce a small Linux-only policy component with two layers:

1. Endpoint discovery reads the live filesystem and block-device topology.
2. A pure decision function accepts two endpoint profiles and returns:
   - mode: Optimized, Balanced, or Conservative;
   - reason code;
   - copy buffer limit;
   - whether bounded cache/writeback policy is required;
   - CPU priority policy for archive children;
   - I/O priority policy;
   - whether archive duty-cycle throttling is required.

The pure decision layer must not read `/sys`, `/proc`, or the filesystem. This
keeps the topology matrix directly unit-testable.

Consumers:

- `OperationQueueCopy.cpp` uses the selected buffer, I/O priority, and cache
  policy;
- `ArchiveFileProvider.cpp` uses the selected process priorities and duty-cycle
  policy;
- both routes log the same mode and reason when diagnostics are enabled.

## Diagnostics

Add a disabled-by-default Linux transfer-policy trace. Each operation should
log one compact decision record containing:

- source and destination paths in the existing safe path-log format;
- source and destination mount roots;
- filesystem types;
- logical device identifiers;
- parent physical-device identifiers where known;
- detected storage types;
- selected mode and reason;
- selected copy buffer/cache window or archive process QoS;
- whether duty-cycle throttling is active.

Diagnostics must report actual behavior. For example, they must not print
`throttle=60/40ms` when throttling is disabled.

## Test matrix

The pure policy tests must cover at least:

| Source | Destination | Expected mode |
| --- | --- | --- |
| ext4 on `sdb1` | same ext4 mount | Optimized |
| Btrfs on `sda4` | same Btrfs mount | Optimized |
| ext4 on SSD A | ext4 on SSD B | Balanced |
| Btrfs on SSD A | Btrfs on SSD B | Balanced |
| ext4 partition 1 on SSD A | ext4 partition 2 on SSD A | Conservative |
| NTFS on SSD A | Btrfs on SSD B | Conservative |
| NTFS on SSD A | ext4 on SSD B | Conservative |
| ext4 | unknown/FUSE destination | Conservative |
| local filesystem | network filesystem | Conservative |
| unresolved device topology | local destination | Conservative |

Copy integration checks:

- selected buffer matches the policy result;
- cache/writeback policy is enabled only when requested;
- cancellation removes partial files;
- progress and speed remain monotonic;
- same-filesystem move/rename behavior is unchanged.

Archive integration checks:

- both direct Extract and archive-selection copy/paste use the same policy;
- cancellation kills every 7-Zip child, including both sides of the compressed
  tar pipeline;
- progress, speed, ETA, staging, cleanup, and final paths remain correct;
- Windows and non-Linux builds do not consume Linux policy code.

## Benchmark matrix

Measure wall time, sustained throughput after page cache effects, UI
responsiveness, CPU utilization, device utilization, I/O latency, and dirty
memory for:

1. one large file on the same ext4 SATA SSD;
2. many small files on the same filesystem;
3. different native Linux filesystems on independent SSDs;
4. different partitions on one physical drive;
5. NTFS source to Btrfs destination;
6. same scenarios for buffered copy and archive extraction.

UI checks during every long operation:

- navigate both panels;
- scroll large directories;
- open and close preview/hover content;
- open the operations drawer;
- cancel an operation;
- verify the final destination contents and refresh behavior.

Do not compare only the cache-heavy beginning of an operation. Record sustained
speed after the transfer exceeds available page-cache benefit.

### Initial runtime baseline (2026-07-17)

The first integration smoke run used one incompressible `1 GiB` file and a
store-only 7-Zip archive containing that file. The available machine provided:

- `sda2`, ext4, SATA SSD;
- `sda4`, Btrfs, another partition on the same SATA SSD;
- `sdb1`, ext4, an independent SATA SSD.

Copy classification and integrity were verified through the real FM
`Ctrl+C`/`Ctrl+V` route:

| Route | Result |
| --- | --- |
| ext4 `sda2` to the same mount | Optimized, `4 MiB`, no bounded cache policy, normal I/O priority |
| ext4 `sda2` to ext4 `sdb1` | Balanced, `4 MiB`, bounded cache policy, best-effort I/O priority level 7 |
| ext4 `sda2` to Btrfs `sda4` | Conservative, `1 MiB`, bounded cache policy, best-effort I/O priority level 7 |

All copied files matched the source SHA-256. The first integration run also
found and fixed endpoint discovery for a destination file that does not exist
yet; discovery now resolves its nearest existing parent before querying
`QStorageInfo`.

Archive-selection extraction through the real `Ctrl+C`/`Ctrl+V` route selected:

| Route | Result | Single smoke-run wall time |
| --- | --- | ---: |
| ext4 `sda2` to the same mount | Optimized, `nice=0`, normal I/O priority, no duty cycle | `1.739 s` |
| ext4 `sda2` to ext4 `sdb1` | Balanced, `nice=19`, best-effort I/O priority level 7, no duty cycle | `2.142 s` |
| ext4 `sda2` to Btrfs `sda4` | Conservative, `nice=19`, idle I/O priority, `60/40 ms` duty cycle | `1.884 s` |

All extracted files matched the source SHA-256. Actual child-process tracing
found and fixed an inherited `nice=-5` value in Optimized mode; archive children
now receive the selected `nice=0` explicitly.

These timings are smoke results only. They were affected by operation order and
page cache and are not sufficient to tune coefficients. No default policy value
was changed from this run.

An NTFS filesystem was no longer available on the test machine. The explicit
NTFS/FUSE policy tests remain in place and retain the previous Conservative
archive safeguards. NTFS-to-Btrfs defaults must not be relaxed without a new
runtime test on that topology.

A later removable-storage run used an approximately `2 GiB` USB 2.0 flash
device (`sdc1`, VFAT). Local-to-USB copy selected Conservative with a `1 MiB`
buffer, bounded cache/writeback, and best-effort I/O priority level 7. The UI
remained responsive. A direct control write with final `fsync` sustained only
`3.1 MB/s`, confirming that the observed low throughput was device-limited;
the policy coefficients were not relaxed. The copied file matched SHA-256.

The USB archive-selection run exposed a 7-Zip progress limitation for a small
store-only file: its process output contained only `0%` and `100%`, while the
physical device continued writing for about 23 seconds. Logical output size and
per-process `write_bytes` were both rejected as fallbacks because 7-Zip and the
kernel accounted the full file before physical completion. The Linux fallback
therefore uses the destination parent block device's completed-sector counter,
with these narrow guards:

- destination storage must classify as USB;
- the selected non-directory output size must be known;
- progress is monotonic and capped below 100% until successful completion;
- ordinary disks and unknown-size or directory selections continue using
  backend progress only.

The repeated USB run then showed progressive percentage, realistic low speed,
and ETA in the Operation Drawer. The extracted file matched SHA-256 and the UI
remained responsive.

### Full acceptance run (2026-07-17)

The final run used the real FM `Ctrl+C`/`Ctrl+V` routes and four payloads for
each mode: a `20 GiB` file copy, extraction of a store-only `15 GiB` file, a
`1 GiB` tree containing 1024 files, and extraction of the same tree. Every
result matched its reference SHA-256 or relative tree manifest.

| Mode and route | 20 GiB copy | 15 GiB extract | 1 GiB tree copy | 1 GiB tree extract |
| --- | ---: | ---: | ---: | ---: |
| Optimized: same `sda2` ext4 mount | `82.037 s`, `249.6 MiB/s` | `67.858 s`, `226.3 MiB/s` | `17.029 s`, `60.1 MiB/s` | `4.027 s`, `254.3 MiB/s` |
| Balanced: `sda2` ext4 to independent `sdb1` ext4 | `185.867 s`, `110.2 MiB/s` | `220.282 s`, `69.7 MiB/s` | `11.612 s`, `88.2 MiB/s` | `3.459 s`, `296.0 MiB/s` |
| Conservative: `sda2` ext4 to `sda4` Btrfs on the same drive | `92.697 s`, `220.1 MiB/s` | `82.981 s`, `185.1 MiB/s` | `14.793 s`, `69.2 MiB/s` | `5.662 s`, `180.9 MiB/s` |

Balanced sequential throughput was limited by the slower `sdb1` destination,
not by a changed source. Its tree results nevertheless exceeded Optimized,
which is consistent with independent devices overlapping metadata and data
I/O. Conservative retained useful throughput despite its duty cycle and kept
the interface usable on the same-physical-device cross-filesystem route.

The initial Optimized large-file copy caused visible one-to-two-second UI
stalls. Three one-factor reruns isolated the effective control:

| Optimized copy variant | Wall time | Throughput | UI result |
| --- | ---: | ---: | --- |
| `4 MiB`, bounded cache, normal I/O priority | `84.675 s` | `241.9 MiB/s` | severe stalls unchanged |
| `4 MiB`, bounded cache, best-effort level 7 | `84.895 s` | `241.3 MiB/s` | severe stalls unchanged |
| `1 MiB`, bounded cache, best-effort level 7 | `86.787 s` | `236.0 MiB/s` | severe stalls removed; only non-critical micro-stalls remained |

The final Optimized copy policy therefore uses a `1 MiB` buffer, bounded cache
and writeback handling, and reduced best-effort I/O priority. Compared with the
original run, throughput decreased by about `5.5%` while the disruptive stalls
were removed. Optimized archive extraction remains unthrottled with normal I/O
priority because it did not reproduce the UI failure.

## Implementation phases

### Phase 1: classification and pure policy

- expose reusable Linux endpoint discovery through `DriveUtils` or a narrowly
  related component;
- implement physical parent-device resolution;
- add the pure three-mode decision function and table-driven tests;
- add policy trace output;
- do not change transfer behavior yet.

### Phase 2: Linux copy integration

- replace the private cross-filesystem predicate with the shared policy;
- enable Linux storage-specific buffer selection;
- preserve the existing conservative cache/writeback implementation;
- validate copy and cross-device move behavior.

### Phase 3: archive integration

- replace the filesystem-type-only archive predicate with the shared policy;
- apply mode-specific child CPU/I/O priority and duty cycle;
- apply the same decision to regular 7-Zip extraction and compressed-tar pipe
  extraction;
- preserve the current working progress and cancellation contracts.

### Phase 4: measurement and tuning

- benchmark Optimized, Balanced, and Conservative independently;
- tune one dimension at a time: buffer, cache window, `nice`, I/O priority,
  duty-cycle ratio, then optional 7-Zip thread cap;
- keep `NTFS -> Btrfs` protected throughout tuning;
- update this document with measured defaults and remove obsolete assumptions.

## Acceptance criteria

- Large `NTFS -> Btrfs` copy and extraction do not cause severe FM or desktop
  lag.
- Same-mount SATA SSD and NVMe operations do not use duty-cycle pauses; copy
  buffering may remain at the measured responsive `1 MiB` limit.
- Independent native Linux SSDs retain useful read/write parallelism.
- Copy and archive routes classify identical endpoint pairs identically.
- Unknown topology fails safe.
- Progress, speed, ETA, cancellation, cleanup, conflicts, and final refreshes
  remain correct.
- All automated tests pass and representative long-running manual scenarios
  are recorded before changing default coefficients.
