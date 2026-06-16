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
- Linux panel and tree directory watching use an `inotify` watcher.
- Linux local panel enumeration, search traversal, disk usage, and folder size
  use the shared `LinuxFileEnumerator` native path.
- Linux properties show Unix owner/group/mode and effective access through
  POSIX calls.
- Linux volume display has initial mount filtering and storage type hints, but
  it is still based on `QStorageInfo::mountedVolumes()` rather than a dedicated
  mountinfo provider.

## Priority Summary

1. Replace the remaining `QStorageInfo`-driven Linux Places construction with a
   mountinfo-backed, de-duplicated, user-facing mount provider.
2. Finish native Linux enumeration coverage for the remaining tree paths and
   add focused tests around symlinks, permissions, and hidden files.
3. Add a Linux-aware application launching path for native executables,
   `.desktop` launchers, AppImage files, scripts, and Windows `.exe` files via
   Wine when available.
4. Wire Linux storage detection into copy/move strategy and add local fast
   paths.
5. Finish Linux metadata polish: properties are no longer placeholder-only, but
   system info, ACL detection, editing, and some desktop integration remain.
6. Improve Linux icons/thumbnails/MIME using freedesktop APIs and caches.
7. Add Linux ISO/mount/eject/device behavior separately from Windows VirtualDisk
   and Configuration Manager code.

## 1. Places And Mounts

Current code:

- `src/models/PlacesModel.cpp` appends standard folders, provider places, then
  drives from `VolumeMonitor` or `QStorageInfo::mountedVolumes()`.
- On Linux, `VolumeMonitor` enumerates `QStorageInfo::mountedVolumes()` but now
  filters pseudo/internal filesystems and non-user-facing mount paths.
- `DriveUtils::detectDriveType()` detects Linux network, optical, USB,
  removable, SSD, and HDD hints through filesystem type and `/sys/class/block`.
- `TreeModel` prefers `VolumeMonitor` roots when available, with a filtered
  `QStorageInfo::mountedVolumes()` fallback.

Problem:

The first cleanup pass filters many implementation mounts, but the source is
still `QStorageInfo::mountedVolumes()`. That limits de-duplication and friendly
device metadata because the app does not yet own a Linux mount model based on
`/proc/self/mountinfo` plus `stat()` device identity.

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
  - Linux uses `LinuxFileEnumerator` for panel scans and child path listing,
    preserving async 512-entry batches.
- `src/core/FileSearchScanner.cpp`
  - Windows has native recursive enumeration.
  - Linux uses `LinuxFileEnumerator` and root-device boundary handling for `/`.
- `src/core/DiskUsageScanner.cpp` and `src/core/FolderSizeCalculator.cpp`
  - Windows use native enumeration.
  - Linux use `LinuxFileEnumerator` and count recursive sizes with `du -sb`
    semantics: apparent size, symlink as link, and hard-link de-duplication.
- `src/models/TreeModel.cpp`
  - Windows has native `FindFirstFileExW` path.
  - Linux root selection is filtered, but child loading still needs a dedicated
    audit to confirm full native enumerator coverage.

Problem:

The main Linux hot paths now share `LinuxFileEnumerator`. Remaining work is
mostly coverage hardening: tree traversal audit, richer metadata from `statx`
where useful, and tests that protect symlink, permission-denied, dotfile, and
large-directory behavior.

Desired Linux behavior:

- Native Linux enumerator based on `opendir()` / `readdir()` plus `fstatat()`
  exists. `statx()` remains a possible enhancement for richer timestamps and
  attributes.
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

- Continue using the shared `LinuxFileEnumerator`.
- Expose reusable functions for:
  - single entry info from `statx`/`fstatat`;
  - direct child enumeration;
  - recursive traversal with cancellation.
- Current covered users:
  - `LocalFileProvider`;
  - `FileSearchScanner`;
  - `DiskUsageScanner`;
  - `FolderSizeCalculator`.
- Remaining audit target:
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
  - Linux creates `LinuxDirectoryChangeWatcher`.
- `src/core/LinuxDirectoryChangeWatcher.cpp`
  - uses `inotify` through `QSocketNotifier`;
  - maps create/delete/modify/rename events to precise `DirectoryChangeEvent`
    values where possible;
  - emits `Overflow` for queue overflow and structural watch invalidation.
- `src/core/QtDirectoryChangeWatcher.cpp`
  - remains the non-Windows, non-Linux fallback;
  - maps every directory change to `DirectoryChangeEvent::Overflow`.

Problem:

The first Linux watcher pass now avoids refresh-on-any-change for common
create/delete/modify/rename events. Remaining work is hardening overflow,
unmount/delete, and high-churn behavior against real desktop sessions.

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

## 4. Application Launching

Current code:

- `FilePanelController::openSelected()` routes ordinary files through
  `QDesktopServices::openUrl()`.
- Provider URI paths are blocked from direct file launch.
- Archives and ISO images have special handling before the generic launch path.

Problem:

Linux needs deliberate executable handling instead of treating every file as a
document. Native executables, scripts, AppImage files, `.desktop` launchers, and
Windows `.exe` files have different expectations and failure modes. The app
also needs clear user feedback when a launch cannot happen.

Desired Linux behavior:

- Keep non-executable documents on the desktop/default-app path.
- Launch executable local files with the parent folder as working directory.
- Detect executable types:
  - executable bit plus ELF magic;
  - shebang scripts with executable bit;
  - AppImage files;
  - `.desktop` launchers that are trusted/executable enough to run;
  - Windows `.exe` files.
- For Windows `.exe` files:
  - detect `wine`/`wine64` in `PATH`;
  - launch through Wine when available;
  - show a clear unavailable message when Wine is missing.
- Do not pass provider, archive, or virtual paths directly to POSIX launch APIs.
- Surface permission denied, missing interpreter, missing Wine, and start
  failure errors in the file panel status or a dialog.

Acceptance checks:

- ELF executable launches from the file manager.
- Executable shell script launches; non-executable script reports a clear
  failure instead of guessing an interpreter.
- AppImage launch works when executable.
- `.desktop` launchers do not bypass trust/executable checks.
- `.exe` launches through Wine when Wine exists and reports a clear message
  when it does not.
- Non-executable documents still open in the default desktop app.

## 5. Copy, Move, And Storage Performance

Current code:

- `src/core/OperationQueue.cpp` uses `getDriveTypeByPath()` to choose copy
  buffer size.
- Windows detects USB/fixed/NVMe/HDD via WinAPI and storage IOCTLs.
- `DriveUtils::detectDriveType()` already has Linux sysfs-based classification
  for volume display.
- `OperationQueue::getDriveTypeByPath()` still returns
  `DriveStorageType::Unknown` on non-Windows, so Linux copy performance still
  uses the unknown/default buffer path.
- Copy loops are provider-based and preserve progress/cancellation.

Problem:

Linux volume display can classify storage, but copy/move does not consume that
logic yet and does not use kernel fast paths. Large local copies should not be
limited to a generic buffered loop when both endpoints are local filesystem
paths.

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

## 6. Permissions And Attributes

Current code:

- `src/core/FileAccessResolver.cpp`
  - Windows has ACL/AuthZ-based effective access logic.
  - Linux uses `lstat`, `getpwuid_r`, `getgrgid_r`, `faccessat(AT_EACCESS)`,
    sticky-parent delete checks, and mode-bit rendering for Unix properties and
    effective access.
- `LocalFileProvider` Linux attributes are dotfile hidden, writable-derived
  read-only, and symlink info from `QFileInfo`.
- `tests/core/LinuxFileAccessResolverTest.cpp` covers owner/group/mode rows and
  executable/read/traverse/create access basics.

Problem:

Linux permissions are no longer placeholder-only. Remaining gaps are editing,
ACL presence, immutable/append-only flags, and broader edge-case coverage. The
UI can show owner/group/mode/sticky/setuid/setgid, but it cannot yet modify
mode/ownership or represent ACLs.

Desired Linux behavior:

- Continue using POSIX metadata, with `statx` as a future enhancement for:
  - uid/gid;
  - mode bits;
  - file type;
  - symlink;
  - timestamps;
  - immutable/append-only when available.
- Owner/group names are already resolved through `getpwuid_r` and
  `getgrgid_r`.
- Mode is already rendered as symbolic and octal, for example `rwxr-xr-x` and
  `755`.
- Effective access already uses `faccessat`; consider `faccessat2` only where it
  improves correctness.
- Later: detect POSIX ACL presence, but do not build full ACL editing first.
- Editing:
  - `chmod` should be supported before `chown`;
  - `chown`/`chgrp` need clear permission errors.

Acceptance checks:

- Properties for Linux files show real owner/group/mode.
- Executable files and read-only files are identified correctly.
- Symlink metadata is shown without accidentally following loops.

## 7. Icons, MIME, Thumbnails, And Desktop Identity

Current code:

- `IconProvider` has extensive Windows Shell extraction paths and a fallback
  internal SVG resolver.
- Linux native icons use desktop-configured freedesktop icon themes through
  `QIcon::fromTheme`.
- Linux file icons use Qt MIME detection plus MIME and generic icon names.
- Linux special folders including Home, Desktop, Downloads, Documents, Pictures,
  Music and Videos use XDG location matching and theme-specific folder icon
  candidates before falling back to the generic folder icon.
- Linux `.desktop` entries can provide their own theme icon.
- `ThumbnailProvider` supports images, SVG, PDF, fonts, audio cover art, and
  Windows shell thumbnails.

Problem:

Linux desktop identity is not Windows Shell identity. The baseline Linux native
icon path is now good enough for normal file-manager use, but display/launch
handling for `.desktop` files and freedesktop thumbnail cache integration still
need polish.

Desired Linux behavior:

- MIME:
  - use Qt MIME database as baseline;
  - consider libmagic only if Qt MIME is insufficient for specific cases.
- Icons:
  - keep using freedesktop icon themes through `QIcon::fromTheme`;
  - keep mapping MIME names to theme icon names;
  - preserve XDG special-folder theme icon candidates;
  - parse `.desktop` files for display name/icon/exec enough for listing and
    preview;
  - preserve the app setting for native icons vs internal icons.
- Thumbnails:
  - read freedesktop thumbnail cache under `~/.cache/thumbnails`;
  - generate async thumbnails for images/PDF/video where supported;
  - never block selection, hover, navigation, or context menu opening.

Acceptance checks:

- Common MIME types show desktop-theme icons when native icons are enabled.
- XDG special folders such as Downloads show theme-specific folder icons when
  the active icon theme provides them.
- `.desktop` files display meaningful names/icons.
- Thumbnail work remains asynchronous and cancellable.

## 8. System Info

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

## 9. ISO, Devices, Eject, And Mount Management

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

## 10. Archive Runtime On Linux

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

## 11. Suggested Implementation Order

Phase 1: Places cleanup

- Replace the current filtered `QStorageInfo` scan with a mountinfo-backed
  Linux mount provider.
- Add device-id de-duplication and better labels where available.
- Keep UI model shape unchanged.
- Verify with current machine, USB drive, network mount if available.

Phase 2: Native local enumeration hardening

- Keep `LinuxFileEnumerator` as the shared helper for panel/search/disk
  usage/folder size.
- Audit and finish tree child-loading coverage.
- Add regression tests for dotfiles, symlinks, permission denied folders, hard
  links, and `/` mount-boundary traversal.

Phase 3: Watchers

- Harden `LinuxDirectoryChangeWatcher` with real desktop churn tests.
- Keep `QtDirectoryChangeWatcher` as non-Linux fallback.

Phase 4: Application launching

- Add Linux executable classification.
- Add Wine-backed `.exe` launch when Wine is available.
- Keep document open behavior on desktop/default-app paths.

Phase 5: Storage/copy

- Reuse existing `DriveUtils` Linux storage classification in `OperationQueue`.
- Add local-only reflink/copy_file_range path.

Phase 6: Metadata/desktop polish

- Permissions/properties editing and ACL detection.
- System info.
- `.desktop` identity, icons/MIME/thumbnails.
- UDisks2/device/eject work.

## 12. Verification Matrix

Minimum recurring checks for Linux parity work:

- Release build from `build`.
- Automated watcher coverage: `ctest --test-dir build --output-on-failure`.
- Open Home, `/`, a large source tree, and a directory with many dotfiles.
- Toggle show-hidden and verify dotfile behavior.
- Create/delete/rename files and verify watcher updates.
- Copy/move files within same filesystem and across filesystems.
- Launch ELF executable, shell script, AppImage, `.desktop` launcher, and
  Windows `.exe` with/without Wine.
- Open archive, nested archive, and extract to local path.
- Sign in to Google Drive, restart app, verify persisted auth through libsecret.
- Check Places after connecting/removing a USB drive.
- Check Places does not show pseudo/system mounts as primary drives.
- Open Properties on regular file, executable, symlink, directory, and
  permission-denied path.
