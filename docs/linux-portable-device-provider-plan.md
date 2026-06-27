# Linux MTP Portable Device Provider Plan

## Scope

Linux portable media support keeps the same app-level contract as Windows:

- public scheme: `portable://`;
- plugin id: `fm.portable-device-provider`;
- Places section: `portable`;
- capabilities: `Browse | ReadMetadata | Transfer`;
- first release is read-only.

The Linux target desktop is KDE. The backend should therefore use KDE/KIO for
MTP instead of GVfs or direct raw `libmtp` access.

## Current Linux MVP Status

Implemented in `src/plugins/portable_device/PortableDeviceFileProviderPlugin.cpp`:

- Linux plugin target builds when KF6 KIO is available;
- KIO `mtp:/` device entries are exposed as `portable://device/...`;
- object paths store percent-encoded backend KIO URLs;
- browsing uses `KIO::listDir`;
- metadata uses `KIO::stat`;
- copy and preview staging use `KIO::file_copy`;
- mutations remain read-only.

Why KIO:

- Dolphin already recognizes the phone correctly through KIO;
- direct `libmtp` can report stale model names and can conflict with KDE's MTP
  worker holding the USB interface;
- KIO avoids competing for raw USB ownership and matches KDE user expectations.

## Path Model

The public path model stays compatible with the Windows provider:

- root: `portable://`;
- device: `portable://device/<encoded-device-id>`;
- object: `portable://device/<encoded-device-id>/object/<encoded-object-id>`.

On Linux:

- device id is the encoded KIO device URL from `mtp:/` listing;
- object id is the encoded KIO object URL;
- KIO URLs remain backend details and are not shown as app paths.

## Behavior

Provider behavior should match Windows where possible:

- root scan lists KIO `mtp:/` devices;
- device scan lists the KIO device URL;
- folder scan lists the KIO object URL;
- `entryInfo` uses cache first, then `KIO::stat`;
- `openRead` materializes a cleanup-managed temporary file;
- `copyToLocalFile` copies via KIO and removes partial output on failure/cancel;
- create, rename, delete, paste-to-device, and write APIs fail read-only.

## Dependencies

CMake should build the Linux portable provider only when KF6 KIO is available:

```cmake
find_package(KF6KIO QUIET)
target_link_libraries(fm_portable_device_provider PRIVATE KF6::KIOCore)
```

No GVfs runtime packages are required. `libmtp` is not used by the app's main
backend.

## Acceptance Checks

- phone appears under `Portable media` with the same user-visible name as
  Dolphin;
- opening the device enters the phone root;
- internal storage/DCIM browsing works;
- copy from phone to local storage works;
- preview staging works for images/videos;
- cancel during copy removes partial local output;
- locked/not-authorized phone surfaces a useful KIO error;
- unplug during browse/copy does not crash and surfaces removal/unavailable
  state.

## Follow-Ups

- Split shared portable path helpers and platform backends into separate files.
- Add fake-backend tests for path mapping and read-only behavior.
- Add PTP/camera support separately if KDE exposes a usable backend for it.
- Improve active KIO job cancellation for long listings if real devices need it.
