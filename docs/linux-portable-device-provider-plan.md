# Linux MTP/PTP Portable Device Provider Plan

## Scope

Add Linux support for portable media devices with the same product shape as the
current Windows portable-device provider:

- support MTP phones and PTP/camera devices;
- expose them through the existing `portable://` scheme;
- keep the provider read-only for the first Linux release;
- support browsing, metadata, preview staging, and copying from device to local
  storage;
- integrate with existing Places and Storage UI without adding new QML concepts.

Out of scope for the first pass:

- writing to MTP/PTP devices;
- deleting, renaming, or creating folders on devices;
- direct raw USB ownership through `libmtp`/`libgphoto2`;
- custom mounting UI for devices that the desktop stack cannot see.

## Current Windows Implementation

The Windows implementation lives in
`src/plugins/portable_device/PortableDeviceFileProviderPlugin.cpp` and is built
only under `WIN32`.

It already defines the stable app-level contract we should keep on Linux:

- root path: `portable://`;
- device path: `portable://device/<encoded-device-id>`;
- object path: `portable://device/<encoded-device-id>/object/<encoded-object-id>`;
- plugin id: `fm.portable-device-provider`;
- scheme: `portable`;
- Places section: `portable`;
- capabilities: `Browse | ReadMetadata | Transfer`;
- mutations fail with `portable:// is read-only`.

Windows uses WPD:

- `IPortableDeviceManager` enumerates devices;
- drive-backed devices are filtered out to avoid duplicating normal removable
  storage;
- WPD object ids are encoded into `portable://.../object/...`;
- metadata comes from WPD object properties;
- directories are WPD folders or functional objects;
- file copy uses `IPortableDeviceResources::GetStream`;
- preview opens a cleanup-managed temporary file and copies the remote object
  into it;
- root and object listings populate provider caches for `entryInfo`,
  `childPaths`, `parentPath`, and `childPath`.

The UI and core already know how to consume this:

- `PlacesModel` groups provider places with section `portable`;
- `Sidebar.qml` labels that section as `Portable media`;
- `StorageView.qml` already has portable-device cards;
- `FilePanelController` treats `portable://` failures that indicate removal as
  device-removal cases.

This means Linux should not introduce `mtp://` or `gphoto2://` as public app
paths. Those backend URIs should be implementation details behind `portable://`.

## Backend Choice

Use GIO/GVfs for the Linux MVP.

Reasons:

- GVfs already has backends for both `mtp://` and `gphoto2://`;
- it avoids raw USB locking conflicts with desktop file managers;
- it gives one API for device roots, folders, metadata, and read streams;
- it lets PTP/camera support arrive in the same provider instead of building
  separate `libmtp` and `libgphoto2` paths;
- the provider remains read-only even if the backend exposes write operations.

Direct `libmtp`/`libgphoto2` support should stay a later fallback option only if
GVfs proves insufficient. It would add more device-claiming edge cases, more
dependencies, and separate MTP/PTP code paths.

Expected runtime dependencies on Linux:

- `glib-2.0`;
- `gobject-2.0`;
- `gio-2.0`;
- GVfs MTP backend, commonly packaged as `gvfs-mtp`;
- GVfs gphoto backend, commonly packaged as `gvfs-gphoto2`.

The app should build without the Linux portable plugin when GIO development
files are unavailable. Missing GVfs runtime backends should produce an empty
portable-device list, not break startup.

## Architecture

Keep the existing Windows plugin intact and add a Linux implementation behind
the same provider contract.

Recommended file layout:

- `src/plugins/portable_device/PortableDevicePaths.h`
- `src/plugins/portable_device/PortableDevicePaths.cpp`
- `src/plugins/portable_device/PortableDeviceTypes.h`
- `src/plugins/portable_device/PortableDeviceFileProviderPlugin.h`
- `src/plugins/portable_device/PortableDeviceFileProviderPlugin_win.cpp`
- `src/plugins/portable_device/PortableDeviceFileProviderPlugin_linux.cpp`

The first refactor should extract only platform-neutral helpers:

- `PortableRoot`;
- `PortableDevicePrefix`;
- `PortableObjectSegment`;
- `PortablePath`;
- `PortableDeviceInfo`;
- `encodedSegment`;
- `decodedSegment`;
- `devicePath`;
- `objectPath`;
- `parsePortablePath`;
- `normalizedPortablePath`;
- `fallbackDeviceName`;
- suffix/previewable-media helpers if both platforms need them.

Do not refactor unrelated WPD logic. Keep extraction mechanical so Windows
behavior stays unchanged.

Only one plugin target should be built per platform:

- Windows: current WPD implementation;
- Linux: new GIO/GVfs implementation;
- both expose plugin id `fm.portable-device-provider` and scheme `portable`.

## Linux Device Model

Linux provider device discovery should use `GVolumeMonitor`:

- get current mounts with `g_volume_monitor_get_mounts`;
- inspect each mount root with `g_mount_get_root`;
- accept roots whose URI scheme is `mtp` or `gphoto2`;
- ignore normal local paths and other schemes for the MVP;
- create one `PortableDeviceInfo` per accepted mount.

Device id selection:

1. Prefer a stable mount UUID if GIO exposes one.
2. Otherwise use the root URI.
3. As a fallback, combine mount name and root URI.

The id only needs to be stable within the current connection. It is acceptable
for it to change after unplug/replug.

Device kind:

- `gphoto2://` roots become `photo`;
- `mtp://` roots become `portable`;
- names/descriptions containing camera/PTP can also become `photo`.

Places mapping:

- `ProviderPlaceItem::path`: `portable://device/<encoded-device-id>`;
- `section`: `portable`;
- `driveType`: `camera` for photo devices, `portable` otherwise;
- `icon`: keep `drive` unless the icon pipeline gains a better portable icon;
- `subtitle`: `Read-only camera/photo device` or
  `Read-only portable media device`;
- `isReady`: `true`;
- `canEject`: `false` for the MVP.

## Linux Object Model

The provider should map every browseable backend object to a portable object
path. The simplest robust MVP mapping is:

- device root maps to `portable://device/<device-id>`;
- child object id is the backend `GFile` URI;
- child app path is `objectPath(deviceId, childGFileUri)`.

This makes `entryInfo`, `parentPath`, and copy operations resolvable even when
the object was not reached through the current in-memory listing. The URI is
percent-encoded inside the portable path, so it remains a valid app path.

Provider caches should still be kept for speed:

- `m_entryByPath`: app path to `FileEntry`;
- `m_childrenByPath`: parent app path to child app paths;
- `m_parentByPath`: child app path to parent app path;
- `m_backendUriByPath`: app path to `GFile` URI, optional if object id is URI;
- `m_deviceRootUriById`: device id to root `GFile` URI.

Root/device cache entries must be refreshed on every root scan because devices
can appear and disappear.

## Metadata Mapping

Use `g_file_enumerate_children` and request at least:

- `standard::name`;
- `standard::display-name`;
- `standard::type`;
- `standard::size`;
- `standard::content-type`;
- `standard::icon`;
- `standard::is-hidden`;
- `time::modified`;
- `time::created`;
- `access::can-read`.

Map to `FileEntry`:

- `path`: portable object path;
- `name`: display name, then standard name, then URI tail fallback;
- `isDirectory`: `G_FILE_TYPE_DIRECTORY`;
- `size`: `standard::size` for files;
- `suffix`: current suffix helper;
- `isHidden`: `standard::is-hidden`;
- `isReadOnly`: always `true`;
- `modified` / `created`: GIO time attributes converted to `QDateTime`;
- `sizeText`: `QLocale().formattedDataSize(size)` for files;
- `attributesText`: `Portable folder` for directories, `Read-only` for files;
- `mimeType`: `standard::content-type` converted through
  `g_content_type_get_mime_type`;
- `isImage` / `hasThumbnail`: same suffix and MIME heuristics as Windows.

When GIO returns unknown type, treat it as a file unless metadata clearly marks
it as a directory. Do not expose write capability based on GIO access flags.

## Provider Behavior

Linux provider should match the Windows provider methods:

- `scheme()`: `portable`;
- `canHandle(path)`: valid `portable://` path;
- `capabilities()`: `Browse | ReadMetadata | Transfer`;
- `normalizedPath(path)`: shared `normalizedPortablePath`;
- `absolutePath(path)`: normalized portable path;
- `pathExists(path)`: root, current device id, or resolvable `GFile`;
- `isDirectory(path)`: root/device roots are directories; otherwise metadata;
- `isSymLink(path)`: `false`;
- `fileName(path)`: root label, device display name, or object entry name;
- `parentPath(path)`: root for devices, cache or backend parent for objects;
- `childPath(parent, name)`: scan/list children and match by display name;
- `entryInfo(path)`: cache first, then resolve backend `GFile` info;
- `childPaths(path, includeHidden)`: cache first, otherwise blocking list;
- `openRead(path, stagingParentPath)`: materialize to cleanup-managed temp file;
- `copyToLocalFile(...)`: stream from GIO to a local `QFile`;
- all mutating methods: fail read-only.

Also override these for clearer UI behavior:

- `isReadOnlyContainer(path)`: `true` for valid portable paths;
- `canCreateChildren(path)`: `false`;
- `canRemovePath(path)`: `false`;
- `canCopyPath(path)`: `true` for files, `false` for directories if recursive
  device-to-local folder copy is not implemented in the first pass.

## Scanning

Scanning should remain asynchronous like Windows:

- increment provider generation before each scan;
- set `m_running`;
- run blocking GIO enumeration in `QtConcurrent::run`;
- emit `batchReady(entries, generation)` on the provider thread;
- emit `finished(path, success, generation, error)`;
- ignore stale results if generation changed.

Root scan:

1. Enumerate GIO mounts.
2. Filter to `mtp` and `gphoto2`.
3. Build virtual directory entries for each device.
4. Update root children cache.

Device/object scan:

1. Resolve device id to root URI.
2. Resolve object URI from object id or use root URI for device path.
3. Create `GFile`.
4. Enumerate children with requested metadata attributes.
5. Convert each child into `FileEntry`.
6. Cache entries, parent links, and children.
7. Apply hidden filtering only to emitted entries; keep full cache.

Large folders must not block the UI thread. Enumeration can be batch-emitted
later if needed, but the MVP can mirror Windows and emit one batch when the
worker finishes.

## Copy, Preview, And Cancel

Copy from device to local:

- create `GFile` from object URI;
- open with `g_file_read`;
- copy in chunks to `QFile`;
- report progress after every chunk;
- remove destination on read/write/cancel failure;
- clear error on success.

Use a buffer in the same range as Windows:

- minimum 16 KiB;
- default 64 KiB;
- maximum 1 MiB.

Cancellation:

- keep a provider-level generation counter for scans;
- keep an active `GCancellable` for blocking GIO operations;
- `cancel()` increments generation, flips running to false, and calls
  `g_cancellable_cancel`;
- copy also respects the existing progress callback: if it returns false,
  abort and remove the partial destination;
- canceled operations should return a clear `Portable device transfer canceled`
  or `Portable device scan canceled` message.

Preview:

- keep the existing cleanup-managed temporary-file behavior;
- use caller-provided staging parent when present;
- otherwise use `StagingLocationPolicy::defaultCleanupRoot()/portable-preview`;
- fallback to `QDir::tempPath`;
- register the artifact with `CleanupSubsystem` as `RemotePreview`;
- copy the remote file into the temp file, then reopen it for reading.

## Error Handling

Normalize common GIO failures into useful app messages:

- `G_IO_ERROR_CANCELLED`: operation canceled;
- `G_IO_ERROR_NOT_FOUND`, `G_IO_ERROR_NOT_MOUNTED`,
  `G_IO_ERROR_FAILED_HANDLED`: portable device was removed or is unavailable;
- permission/locked-device errors: ask the user to unlock the phone and allow
  file transfer;
- missing backend/no mounts: no places, no startup error.

Keep the existing `FilePanelController` portable-removal handling useful by
including wording such as `Portable device was removed` for disconnect cases.

## CMake Plan

Add Linux plugin build logic without making GIO mandatory for the whole app:

```cmake
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(GIO gio-2.0 gobject-2.0 glib-2.0)
    endif()

    if(GIO_FOUND)
        qt_add_plugin(fm_portable_device_provider
            CLASS_NAME PortableDeviceFileProviderPlugin
            src/plugins/portable_device/PortableDeviceFileProviderPlugin.h
            src/plugins/portable_device/PortableDevicePaths.cpp
            src/plugins/portable_device/PortableDeviceFileProviderPlugin_linux.cpp
        )
        target_include_directories(fm_portable_device_provider PRIVATE
            src src/core ${GIO_INCLUDE_DIRS})
        target_link_libraries(fm_portable_device_provider PRIVATE
            Qt6::Core Qt6::Concurrent fm ${GIO_LIBRARIES})
        target_compile_options(fm_portable_device_provider PRIVATE
            ${GIO_CFLAGS_OTHER})
    else()
        message(STATUS "Linux portable-device provider disabled: GIO not found")
    endif()
endif()
```

The existing Windows target should keep linking `Ole32` and
`PortableDeviceGuids`.

If duplicate target names are awkward while both platform files coexist, use
platform-specific internal target names but keep plugin id and class metadata
stable.

## Tests

Automated tests that do not require hardware:

- shared path parser accepts `portable:`, `portable:/`, `portable://`;
- shared path parser rejects malformed device/object paths;
- shared encoder/decoder preserves device ids and backend URIs;
- provider reports read-only capabilities;
- provider mutation methods fail with the read-only error;
- Linux backend URI filtering accepts `mtp://` and `gphoto2://`, rejects local
  files and unrelated schemes.

Useful test seam:

- isolate GIO operations behind a small internal backend interface;
- production backend uses GIO;
- tests use a fake backend with devices, directories, files, hidden entries,
  metadata, and stream content.

Fake-backend tests should cover:

- root listing produces portable device entries;
- device scan produces child `FileEntry` values;
- hidden filtering affects emitted entries but not full child cache;
- `parentPath` and `childPath` work from cache;
- `copyToLocalFile` writes expected bytes and deletes partial files on cancel.

Manual acceptance checks:

- with `gvfs-mtp` installed, connect an Android phone in file-transfer mode;
- phone appears under `Portable media`;
- phone disappears cleanly when unplugged;
- locked/not-authorized phone gives a useful error;
- browsing internal storage/DCIM works;
- copying a large file to local storage shows progress and can be canceled;
- cancel leaves no final destination file or stale `.part` file from the app;
- image/video preview opens through cleanup staging;
- with `gvfs-gphoto2` installed, connect a PTP camera or phone in PTP mode;
- camera appears as read-only photo device;
- DCIM browsing and copy work;
- create/rename/delete/paste actions are unavailable or fail read-only.

## Implementation Phases

1. Extract portable path helpers.
   Verify: Windows build still succeeds and path-helper tests pass.

2. Add Linux GIO discovery and Places integration.
   Verify: app starts on Linux with and without GIO/GVfs backends; devices appear
   in `Portable media` when mounted by GVfs.

3. Add Linux browse and metadata mapping.
   Verify: root, device root, folders, hidden entries, `entryInfo`,
   `parentPath`, and `childPath` work against a fake backend and a real device.

4. Add read/copy/preview staging.
   Verify: file copy works, previews open, cleanup artifacts are registered, and
   canceled copy removes partial output.

5. Harden cancellation and disconnect handling.
   Verify: cancel during scan/copy stops promptly; unplug during scan/copy
   surfaces `Portable device was removed`.

6. Update docs and package notes.
   Verify: Linux roadmap points to this plan and runtime package expectations
   are documented.

## Risks

- Some distributions do not install `gvfs-mtp` or `gvfs-gphoto2` by default.
  The provider should degrade to no devices and docs should name the packages.
- MTP devices can be slow or return incomplete metadata. Keep all enumeration
  off the UI thread.
- GVfs URI/device ids may change after reconnect. Treat portable paths as live
  session paths, not permanent bookmarks.
- Locked phones often expose the mount but fail enumeration. Error messages
  should mention unlocking and allowing file transfer.
- GIO objects need careful ownership: unref `GObject` values and free `GError`
  consistently.
- PTP devices can expose a narrower tree than MTP. That is acceptable for the
  read-only MVP if browsing and copy work.
