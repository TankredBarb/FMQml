# Linux Parity Roadmap

This document records the current Linux gap against the Windows build. The goal
is not only "works on Linux", but the same product class: fast navigation,
precise watchers, useful places/devices, correct permissions, and operation
performance that matches the platform instead of relying on broad Qt fallbacks.

Current status as of the Linux bring-up:

- The Release build works from `build`.
- Archives are required through `bit7z`.
- Qt Multimedia preview is enabled.
- Google Drive OAuth persistence works through `libsecret`.
- MTP/portable-device provider is intentionally Windows-only for now.
- Most local filesystem features work, but several hot paths are still Qt
  fallback implementations.

## Priority Summary

1. Replace noisy Linux Places construction with mount-aware, user-facing places.
2. Add native Linux local enumeration for panels, tree, search, disk usage, and
   folder-size paths.
3. Add `inotify` watcher support instead of refresh-on-any-change fallback.
4. Implement Linux storage detection and copy/move fast paths.
5. Replace placeholder Linux permission/system-info behavior with real `/proc`,
   `/sys`, POSIX, and desktop integration.
6. Improve Linux icons/thumbnails/MIME using freedesktop APIs and caches.
7. Add Linux ISO/mount/eject/device behavior separately from Windows VirtualDisk
   and Configuration Manager code.

## 1. Places And Mounts

Current code:

- `src/models/PlacesModel.cpp` appends standard folders, provider places, then
  drives from `VolumeMonitor` or `QStorageInfo::mountedVolumes()`.
- On Linux, `VolumeMonitor` falls back to `QStorageInfo::mountedVolumes()`.
- `DriveUtils::detectDriveType()` returns `"hdd"` for every non-Windows volume.
- `TreeModel` also uses `QStorageInfo::mountedVolumes()` for roots.

Problem:

`QStorageInfo::mountedVolumes()` exposes many implementation mounts that are not
useful file-manager places: pseudo filesystems, container/runtime mounts,
snapshots, cgroup/proc/sys/dev internals, bind mounts, temp mounts, and desktop
service internals. This creates a noisy Linux Places list with many folders the
user should never see as primary places.

Desired Linux behavior:

- Show a small, intentional Places section:
  - Favorites.
  - Google Drive.
  - Home.
  - Desktop, Downloads, Documents, Pictures, Music, Videos only when they exist
    and are not duplicates of Home.
- Show a storage/devices section based on mount relevance:
  - `/` root filesystem.
  - user-mounted removable drives under `/run/media/$USER`, `/media/$USER`,
    `/mnt` when appropriate.
  - network mounts such as `nfs`, `cifs`, `smb3`, `sshfs`, `davfs`, `fuse.sshfs`
    when user-facing.
  - mounted ISO images when the Linux ISO implementation exists.
- Hide pseudo/internal filesystems:
  - `proc`, `sysfs`, `devtmpfs`, `devpts`, `cgroup`, `cgroup2`, `pstore`,
    `securityfs`, `debugfs`, `tracefs`, `configfs`, `fusectl`, `binfmt_misc`,
    `mqueue`, `hugetlbfs`, `autofs`, `overlay` unless it is the actual root.
  - runtime paths under `/proc`, `/sys`, `/dev`, `/run/user`, `/var/lib`,
    `/snap`, package manager internals, and duplicate bind mounts by default.
- De-duplicate by device id plus mount root. Prefer the shortest/user-facing
  mount path when multiple mounts point to the same backing device.

Recommended implementation:

- Add a Linux mount provider used by `VolumeMonitor`/`PlacesModel`.
- Parse `/proc/self/mountinfo`, not only `/proc/mounts`.
- Use `stat()` on mount roots to get `st_dev`.
- Resolve `/sys/dev/block/<major>:<minor>` to classify block devices.
- Read `/sys/block/<device>/queue/rotational` for HDD vs SSD.
- Detect NVMe from device path/name (`nvme*`) and USB from sysfs transport/path.
- Keep `QStorageInfo` only for byte counts and as a fallback.
- Optionally integrate UDisks2 later for labels, icons, removable state, eject,
  and friendly desktop names.

Acceptance checks:

- Places on a normal KDE/GNOME session should show Home/user folders plus a
  small set of real volumes, not dozens of system mounts.
- Mounting a USB drive adds one clear device entry with label, capacity, and
  eject capability.
- Network mounts are visible when they are user-facing.
- The root filesystem is visible exactly once.

## 2. Local Directory Enumeration

Current code:

- `src/core/LocalFileProvider.cpp`
  - Windows uses `FindFirstFileExW` with `FIND_FIRST_EX_LARGE_FETCH`.
  - Linux uses `QDirIterator`/`QFileInfo` fallback through `entryFromInfo()`.
- `src/core/FileSearchScanner.cpp`
  - Windows has native recursive enumeration.
  - Linux uses `QDirIterator`.
- `src/core/DiskUsageScanner.cpp` and `src/core/FolderSizeCalculator.cpp`
  - Windows use native enumeration.
  - Linux use `QDirIterator`.
- `src/models/TreeModel.cpp`
  - Windows has native `FindFirstFileExW` path.
  - Linux needs an equivalent native path.

Problem:

Qt enumeration is acceptable for correctness bring-up, but it is not the final
performance target for a file manager. It performs repeated abstraction work,
has less control over symlink handling and metadata calls, and cannot share a
single optimized Linux metadata path across panels/search/disk usage/tree.

Desired Linux behavior:

- Native Linux enumerator based on `opendir()` / `readdir()` plus `statx()` or
  `fstatat()`.
- Do not follow symlinks by default: use `AT_SYMLINK_NOFOLLOW`.
- Use `d_type` when available, but fall back to `statx`/`fstatat` for
  `DT_UNKNOWN`.
- Produce the same `FileEntry` shape as Windows:
  - name, absolute path, suffix, size, modified time, created/birth fallback,
    directory, symlink, hidden, read-only, executable, attributes text.
- Dotfiles are hidden.
- Batch panel model updates in the existing 512-entry shape unless benchmarks
  prove a better number.
- Keep async execution and generation/cancellation guards.

Recommended API layer:

- Add a small Linux enumeration helper under `src/core`, for example
  `LinuxFileEnumerator`.
- Expose reusable functions for:
  - single entry info from `statx`/`fstatat`;
  - direct child enumeration;
  - recursive traversal with cancellation.
- Use it from:
  - `LocalFileProvider`;
  - `FileSearchScanner`;
  - `DiskUsageScanner`;
  - `FolderSizeCalculator`;
  - `TreeModel`.

Acceptance checks:

- Large Linux folders remain scrollable while loading.
- Counts match Qt fallback on sample folders.
- Dotfiles honor the show-hidden setting.
- Symlink loops do not recurse forever.
- Permission-denied folders report errors without moving the panel into a fake
  state.

## 3. Directory Watchers

Current code:

- `src/core/DirectoryChangeWatcher.cpp`
  - Windows creates `WinDirectoryChangeWatcher`.
  - Linux creates `QtDirectoryChangeWatcher`.
- `src/core/QtDirectoryChangeWatcher.cpp`
  - uses `QFileSystemWatcher`;
  - maps every directory change to `DirectoryChangeEvent::Overflow`.

Problem:

The Linux panel currently refreshes broadly on any change. This is safe, but it
does not match the Windows watcher quality. It loses add/remove/rename detail,
causes unnecessary rescans, and makes high-churn folders less stable.

Desired Linux behavior:

- Add `LinuxDirectoryChangeWatcher` based on `inotify`.
- Use `inotify_init1(IN_NONBLOCK | IN_CLOEXEC)`.
- Watch the current folder with `inotify_add_watch`.
- Read events through `QSocketNotifier` or a worker thread without blocking the
  GUI thread.
- Map events:
  - `IN_CREATE` -> Added.
  - `IN_DELETE` -> Removed.
  - `IN_MODIFY`, `IN_ATTRIB`, `IN_CLOSE_WRITE` -> Modified.
  - `IN_MOVED_FROM` + `IN_MOVED_TO` paired by cookie -> Renamed.
  - unmatched move events -> Removed/Added.
  - `IN_Q_OVERFLOW` -> Overflow.
  - `IN_IGNORED`, unmount/delete -> watch failure or Overflow.
- Preserve generation/path guards so stale events cannot mutate a new folder.

Acceptance checks:

- Creating, deleting, renaming, and modifying files updates the current panel
  without a full refresh when possible.
- Queue overflow forces a full refresh.
- Deleting/unmounting the watched folder fails cleanly.

## 4. Copy, Move, And Storage Performance

Current code:

- `src/core/OperationQueue.cpp` uses `getDriveTypeByPath()` to choose copy
  buffer size.
- Windows detects USB/fixed/NVMe/HDD via WinAPI and storage IOCTLs.
- Non-Windows returns `DriveStorageType::Unknown`, so Linux effectively uses
  the unknown/default buffer path.
- Copy loops are provider-based and preserve progress/cancellation.

Problem:

Linux copy performance is not tuned by storage type and does not use kernel
fast paths. Large local copies should not be limited to a generic buffered loop
when both endpoints are local filesystem paths.

Desired Linux behavior:

- Classify destination storage:
  - map path to mount using `/proc/self/mountinfo`;
  - get device major/minor from `stat`;
  - inspect `/sys/dev/block/<major>:<minor>`;
  - read `queue/rotational`;
  - detect USB/NVMe where possible;
  - fall back to Unknown only when classification fails.
- Use layered local-copy strategy only when both endpoints are local paths:
  1. Reflink clone with `ioctl(FICLONE)` for same-filesystem CoW filesystems.
  2. `copy_file_range`.
  3. Buffered loop with storage-tuned buffer.
- Move:
  - use `rename`/`renameat2` for same-filesystem moves;
  - handle `EXDEV` explicitly with copy+delete fallback.
- Preserve:
  - temp-file/rollback semantics;
  - progress updates;
  - cancellation;
  - provider compatibility;
  - archive and Google Drive behavior.

Acceptance checks:

- Same-filesystem moves are instant.
- Cross-device moves fall back correctly.
- Large local copies are faster than the current buffered-only path.
- Provider/archive transfers still use provider semantics.

## 5. Permissions And Attributes

Current code:

- `src/core/FileAccessResolver.cpp`
  - Windows has ACL/AuthZ-based effective access logic.
  - Linux currently uses a `QFileInfo` fallback for readable/writable/executable
    and simple hidden/read-only attributes.
- `LocalFileProvider` Linux attributes are dotfile hidden, writable-derived
  read-only, and symlink info from `QFileInfo`.

Problem:

Linux permissions are not represented at the same quality level as Windows. The
UI cannot yet show owner/group/mode/sticky/setuid/setgid/ACL presence, and
effective access is approximate.

Desired Linux behavior:

- Use `statx` or `fstatat` to read:
  - uid/gid;
  - mode bits;
  - file type;
  - symlink;
  - timestamps;
  - immutable/append-only when available.
- Resolve owner/group names through `getpwuid_r` and `getgrgid_r`.
- Render mode as both symbolic and octal, for example `rwxr-xr-x` and `755`.
- Use `faccessat`/`faccessat2` for effective access where appropriate.
- Later: detect POSIX ACL presence, but do not build full ACL editing first.
- Editing:
  - `chmod` should be supported before `chown`;
  - `chown`/`chgrp` need clear permission errors.

Acceptance checks:

- Properties for Linux files show real owner/group/mode.
- Executable files and read-only files are identified correctly.
- Symlink metadata is shown without accidentally following loops.

## 6. Icons, MIME, Thumbnails, And Desktop Identity

Current code:

- `IconProvider` has extensive Windows Shell extraction paths and a fallback
  internal SVG resolver.
- Linux mostly receives fallback icons and Qt MIME behavior.
- `ThumbnailProvider` supports images, SVG, PDF, fonts, audio cover art, and
  Windows shell thumbnails.

Problem:

Linux desktop identity is not Windows Shell identity. The current fallback is
usable, but not equal to native Linux file-manager behavior.

Desired Linux behavior:

- MIME:
  - use Qt MIME database as baseline;
  - consider libmagic only if Qt MIME is insufficient for specific cases.
- Icons:
  - use freedesktop icon themes through `QIcon::fromTheme`;
  - map MIME names to theme icon names;
  - parse `.desktop` files for display name/icon/exec enough for listing and
    preview;
  - preserve the app setting for native icons vs internal icons.
- Thumbnails:
  - read freedesktop thumbnail cache under `~/.cache/thumbnails`;
  - generate async thumbnails for images/PDF/video where supported;
  - never block selection, hover, navigation, or context menu opening.

Acceptance checks:

- Common MIME types show desktop-theme icons when native icons are enabled.
- `.desktop` files display meaningful names/icons.
- Thumbnail work remains asynchronous and cancellable.

## 7. System Info

Current code:

- `SystemInfoProvider.cpp` uses WinAPI for CPU/RAM on Windows.
- Linux currently uses placeholder values:
  - CPU name: `"Generic Processor"`;
  - total RAM: `16.0`;
  - CPU/RAM usage fixed mock-like values.

Desired Linux behavior:

- CPU name: parse `/proc/cpuinfo` or use `/sys/devices/system/cpu`.
- RAM totals/usage: parse `/proc/meminfo`.
- CPU usage: sample `/proc/stat` over time.
- Uptime: use `/proc/uptime` or keep app uptime separately if that is intended.

Acceptance checks:

- System info reflects actual machine CPU/RAM.
- Values update without blocking the UI.

## 8. ISO, Devices, Eject, And Mount Management

Current code:

- `IsoMountManager` is Windows VirtualDisk-focused.
- `VolumeMonitor` has Windows device notification/eject code and Linux fallback
  to periodic `QStorageInfo` scanning.
- MTP provider is Windows-only after Linux bring-up.

Desired Linux behavior:

- ISO:
  - choose strategy: `udisksctl loop-setup`/UDisks2, `gio mount`, or a small
    helper policy;
  - require clear privilege/error behavior;
  - list mounted ISO images in Places cleanly.
- Devices:
  - use UDisks2 for removable device labels, icons, mount/unmount/eject when
    available;
  - keep `/proc`/`/sys` fallback for environments without UDisks2.
- MTP:
  - separate future provider, likely based on `libmtp` or GIO/GVfs depending on
    desired behavior;
  - keep read-only behavior first, mirroring the current Windows portable-device
    provider scope.

Acceptance checks:

- USB mount/unmount/eject updates Places.
- Eject failures surface a useful error.
- ISO mount/unmount does not require Windows-only assumptions.

## 9. Archive Runtime On Linux

Current code:

- `bit7z` is required by CMake.
- `ArchiveSupport::sevenZipExecutablePath()` searches for `7z` plus Windows
  install paths.
- `archiveLibraryPath()` currently searches for `7z.dll`.

Gap:

Linux archive browsing works through `bit7z` in the current environment, but
runtime discovery should be made explicitly platform-aware.

Desired Linux behavior:

- Search command-line helpers in this order: bundled executable next to `fm`,
  `7z`, `7zz`, `7za` in `PATH`.
- For shared library fallback, search Linux names only if `bit7z` actually
  needs an explicit library path in the deployment model.
- Error messages should say exactly which archive runtime is missing.

Acceptance checks:

- Archive browsing and extraction work after clean install of documented Linux
  dependencies.
- Compression/extraction helpers do not rely on Windows paths.

## 10. Suggested Implementation Order

Phase 1: Places cleanup

- Add Linux mount filtering and de-duplication.
- Keep UI model shape unchanged.
- Verify with current machine, USB drive, network mount if available.

Phase 2: Native local enumeration

- Add shared Linux enumeration helper.
- Wire it into `LocalFileProvider`.
- Then reuse for search, disk usage, folder-size, and tree.

Phase 3: Watchers

- Add `LinuxDirectoryChangeWatcher`.
- Keep `QtDirectoryChangeWatcher` as fallback.

Phase 4: Storage/copy

- Add Linux storage classification.
- Add local-only reflink/copy_file_range path.

Phase 5: Metadata/desktop polish

- Permissions/properties.
- System info.
- Icons/MIME/thumbnails.
- UDisks2/device/eject work.

## 11. Verification Matrix

Minimum recurring checks for Linux parity work:

- Release build from `build`.
- Open Home, `/`, a large source tree, and a directory with many dotfiles.
- Toggle show-hidden and verify dotfile behavior.
- Create/delete/rename files and verify watcher updates.
- Copy/move files within same filesystem and across filesystems.
- Open archive, nested archive, and extract to local path.
- Sign in to Google Drive, restart app, verify persisted auth through libsecret.
- Check Places after connecting/removing a USB drive.
- Check Places does not show pseudo/system mounts as primary drives.
- Open Properties on regular file, executable, symlink, directory, and
  permission-denied path.

