# Linux Device Integration Plan

## Goal

Replace the current Linux `QStorageInfo`-only device lifecycle with a UDisks2
backed integration for physical storage. The result should make drives in FMQml
behave as intentional devices rather than incidental mounted paths:

- stable identity and friendly labels;
- correct mounted/unmounted state;
- mount, unmount, and eject actions where the device supports them;
- immediate model updates after an external mount/unmount or a device removal;
- a conservative fallback when UDisks2 is unavailable.

This is not an ISO rewrite. `IsoMountManager` remains the owner of FM-managed
ISO loop mounts and their cleanup policy.

## Current State

- Linux `VolumeMonitor` enumerates `QStorageInfo::mountedVolumes()` and filters
  pseudo and non-user-facing mounts.
- `DriveUtils` obtains useful Linux SSD/HDD/USB/optical/network hints from
  sysfs and filesystem data.
- Physical-device `requestEject()` returns unsupported on Linux, while the
  Windows path performs native eject.
- Managed ISO mount/unmount works through its separate manager.
- Places, Tree, Storage view, context menus, and local mount badges consume
  `VolumeMonitor` data and must retain their model contracts.

## Product Scope

### In scope

- UDisks2 `Drive`, `Block`, and `Filesystem` objects.
- Mounted removable USB storage, optical media, and user-facing local/network
  mounts.
- Mount, unmount, and safe eject/power-off actions when UDisks2 advertises
  them.
- Friendly labels, filesystem type, capacity, mounted paths, drive type,
  removable/ejectable state, and stable device keys.
- Object-manager change signals plus debounced model refreshes.
- `QStorageInfo` fallback for capacity and environments without UDisks2.
- Explicit coordination with managed ISO roots so a loop mount is not displayed
  twice or unmounted by the wrong owner.

### Out of scope

- Partition creation, formatting, filesystem repair, encryption unlock/lock,
  RAID/LVM management, SMART management, and privileged raw block I/O.
- Replacing the Linux Admin Mode helper with UDisks2 calls.
- Arbitrary `udisksctl` shell invocation.
- Network-provider authentication or mounting arbitrary remote URLs.
- Reworking the visual layout of Places or Storage view.

## Architecture

### 1. Linux device backend

Add a Linux-only backend, for example `LinuxDeviceMonitor`, underneath
`VolumeMonitor`. It owns the system-bus connection and is the authoritative
source when `org.freedesktop.UDisks2` is present.

Use the ObjectManager API and cache only the required interfaces:

- `org.freedesktop.UDisks2.Drive` for model, vendor, serial, removable,
  ejectable, media availability, and optical hints;
- `org.freedesktop.UDisks2.Block` for device identity, label, UUID, filesystem
  type, drive association, partition relation, and preferred device name;
- `org.freedesktop.UDisks2.Filesystem` for mount points and `Mount`/`Unmount`;
- optionally `org.freedesktop.UDisks2.Loop` only to recognize externally
  mounted loop devices without taking ownership from `IsoMountManager`.

Do not query all properties from QML. Convert them once into the existing
`VolumeInfo` shape plus a Linux-only opaque action identity.

### 2. Stable identity and de-duplication

The current root-path key is insufficient for an unmounted device. Add fields
or an internal mapping for:

- stable UDisks object path for the block/filesystem object;
- associated drive object path;
- canonical block device path;
- current mount roots;
- a display key stable across mount-path changes.

Use the filesystem/block object as the action target. A drive can have multiple
partitions; render each mounted filesystem as one navigable entry while keeping
their shared physical-drive metadata available for UI grouping later.

Deduplicate in this order:

1. FM-managed ISO mount root remains owned by `IsoMountManager`.
2. Same UDisks filesystem object appears once even with multiple observations.
3. Bind mounts and duplicate `QStorageInfo` records collapse to the chosen
   user-facing root.
4. Root filesystem remains visible exactly once.

### 3. Data merge and fallback

When UDisks2 is available:

- use UDisks2 for identity, label, state, and action capabilities;
- use `QStorageInfo` matched by mount root for bytes/free space when useful;
- retain `DriveUtils` classification only as a fallback or supplemental hint.

When UDisks2 is unavailable or disconnected:

- retain the present filtered `QStorageInfo` listing;
- hide mount/unmount/eject actions that cannot be carried out safely;
- expose a concise diagnostics reason, not a misleading unsupported action.

### 4. Actions

All actions are asynchronous C++ calls and return structured success/error
results to the existing `VolumeMonitor` signals.

#### Mount

Call `Filesystem.Mount` only for an unmounted, mountable filesystem object.
Use empty/default option maps initially and let UDisks2/Polkit own the auth
prompt. On success, refresh from ObjectManager signals rather than assuming a
specific mount path.

#### Unmount

Call `Filesystem.Unmount` for a mounted filesystem object. It is the normal
Linux action and should be exposed even for devices that are not power-off
ejectable. Report busy/in-use authorization failures directly.

#### Eject / safe removal

For an optical drive call the advertised eject operation where UDisks2 exposes
one. For removable storage, unmount all FM-visible filesystems belonging to the
same drive, then call `Drive.PowerOff` only when supported. Never power off a
drive if a sibling filesystem cannot be unmounted; report the exact blocker.

The UI wording must distinguish:

- **Unmount** — detach one filesystem;
- **Eject** — optical media;
- **Safely Remove** — unmount then power down a removable drive.

### 5. UI integration

Keep the existing views and route actions through the same `VolumeMonitor`
surface:

- Places and Tree receive refreshed mount roots;
- Storage view receives mounted/unmounted state and labels;
- drive context menu chooses Mount, Unmount, Eject, or Safely Remove from
  capability flags rather than platform checks;
- local mount badge index refreshes from the same snapshot;
- panel navigation away from a root being unmounted follows the existing
  invalid-path recovery path.

No QML should construct D-Bus requests or infer whether a root is ejectable.

### 6. Change notifications

Subscribe to ObjectManager `InterfacesAdded`/`InterfacesRemoved` and property
changes. Coalesce bursts into one short debounce refresh so a single plug or
unmount does not produce flicker or repeated tree resets.

Keep the current periodic `QStorageInfo` refresh as a fallback health check,
not as the primary source when UDisks2 is healthy.

## Implementation Phases

### Phase 1 — data-only UDisks2 discovery

1. Add a thin D-Bus adapter with an injectable interface for tests.
2. Enumerate managed objects and map mounted filesystems to `VolumeInfo`.
3. Add stable action identity internally without exposing D-Bus paths to QML.
4. Merge UDisks2 records with capacity data and the existing ISO snapshot.
5. Preserve the exact current fallback when the service is absent.

Verify: root, USB, optical, network mount, and ISO appear once with sensible
labels and no pseudo-filesystems.

### Phase 2 — live updates and UI state

1. Subscribe to object/property changes.
2. Refresh Places, Tree, Storage, and `LocalMountPointIndex` from one snapshot.
3. Add mounted/unmounted/capability roles without changing existing layout.
4. Handle device removal and unmounted-current-panel recovery.

Verify: external mount/unmount and unplug update the UI promptly without a
restart or duplicate entries.

### Phase 3 — mount and unmount

1. Add async mount/unmount APIs to `VolumeMonitor` with structured errors.
2. Wire the existing drive context/menu actions to capability-based labels.
3. Ensure Polkit prompts are system-owned and the GUI never handles passwords.
4. Disable conflicting actions while a request for that device is pending.

Verify: a regular USB filesystem mounts, opens in a panel, unmounts cleanly,
and removes its user-facing root from Places.

### Phase 4 — eject and safely remove

1. Resolve all mounted filesystem siblings for a removable drive.
2. Unmount siblings in a deterministic order.
3. Call the UDisks2 eject/power-off method only when capability data permits.
4. Surface busy, authorization, and partial-unmount errors without falsely
   reporting success.

Verify: USB safe removal, optical eject, busy-device failure, and a drive with
multiple mounted partitions.

### Phase 5 — polish and regression hardening

1. Improve labels/icons from UDisks2 vendor/model/media information.
2. Validate ISO coexistence and externally-created loop mounts.
3. Add trace mode with sanitized object/action identifiers.
4. Refresh the Linux parity roadmap and remove obsolete fallback claims.

## Test Matrix

### Automated

- fake ObjectManager snapshot: root, USB partition, optical media, network
  mount, bind mount, loop mount, and pseudo filesystem;
- de-duplication and stable-key tests;
- mount/unmount/eject request construction and capability gating;
- action completion/error mapping including busy and not-authorized replies;
- object add/remove/property-change debounce;
- managed ISO root is not duplicated or sent through physical-device actions;
- UDisks2-unavailable fallback preserves current `QStorageInfo` behaviour.

### Manual

- plug in a USB drive; check label, capacity, mount badge, and Places entry;
- unmount from FM, then remount externally and confirm FM updates;
- safely remove a removable drive after all partitions are unmounted;
- eject optical media;
- leave a file open from the device and verify the busy error is useful;
- mount/unmount an ISO during the same session and verify it remains on the
  managed ISO path;
- test KDE/Wayland and a session without UDisks2.

## Success Criteria

- Linux physical-device actions are capability-driven rather than a permanent
  unsupported branch.
- Unmount is first-class; Eject/Safely Remove are offered only when correct.
- UDisks2 absence degrades to the existing safe read-only device listing.
- ISO ownership and all existing Places/Storage/tree contracts remain intact.
