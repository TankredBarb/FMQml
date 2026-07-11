# Linux Properties Permissions Plan

## Goal

Add a dedicated **Access & Ownership** tab to the local-file Properties dialog
on Linux. It must let the user inspect and safely change a single local item's
POSIX mode, owner, and group while preserving the existing unprivileged GUI and
narrow Linux Admin Mode helper design.

The user-facing result is not a full ACL editor. It is a reliable replacement
for the missing Windows-style editable attributes, using Linux-native concepts:
`chmod`, `chown`, and `chgrp`.

## Current State

- `FileAccessResolver` already resolves owner, group, symbolic and octal mode,
  raw UID/GID, special bits, and effective access.
- `PropertiesController` exposes the dedicated tab, live pending mode,
  ownership fields, local account/group suggestions, and errors.
- Linux Admin Mode has a typed broker, a request-scoped helper protocol,
  session state, root-side `LinuxAdminPolicy`, and no shell-command escape
  hatch.
- The protocol now has explicit `ChangeMode` and `ChangeOwnership` operations;
  ordinary owned-item changes run through the same helper without elevation.

## Implementation Status — 2026-07-11

Completed:

- Access & Ownership tab and Admin Mode context entry.
- rwx and special-bit editing, including confirmation for newly enabled
  setuid/setgid on an executable regular file.
- Owner/group changes as separate typed helper requests.
- Async local user/group suggestions; ordinary mode suggests only groups of
  the current user and rejects other groups before dispatch.
- Symlink/read-only messaging, numeric UID/GID range checks, helper CLI tests,
  and cache refresh after a successful change.

Remaining before declaring the feature manually verified:

- run the manual matrix below with the installed helper and a real Polkit
  Administrator Mode session;
- inspect narrow-window, light/dark, and keyboard interaction in the running
  application.

## Product Decisions

### In scope

- One local file or directory at a time.
- Read/write/execute bits for owner, group, and others.
- Explicit setuid, setgid, and sticky-bit controls.
- Owner and group changes.
- Mode changes through the typed helper: as the current user where Linux allows
  it, or through active Linux Admin Mode for protected locations.
- An administrator context-menu action that opens this tab directly in edit
  mode.
- Refreshing Properties, file-panel roles, badges, and watcher state after a
  successful mutation.

### Out of scope for the first release

- Recursive permission changes.
- Multi-selection editing.
- POSIX ACL inspection or editing, default ACLs, SELinux/AppArmor labels,
  immutable/append-only flags, capabilities, or extended attributes.
- Editing remote/provider/archive/managed-ISO paths.
- Changing a symlink or silently changing its target.
- Any shell command, `sudo`, or password collection in QML.

### Symlink policy

Phase 1 disables mutation for symbolic links. The UI must say that link
permission/ownership changes are not supported yet rather than following the
target implicitly. A later explicit design may add `lchown` for the link itself
and a separately confirmed target action; it must not be folded into this work.

## UX

### Tab placement and migration

Add **Access & Ownership** beside the existing Properties content for a single
local path. It becomes the sole home for local permission information and
editing. Move the complete current ownership/Unix-mode block out of the
existing **Access** tab into this tab:

- owner and group;
- symbolic and octal mode;
- special bits;

Do not duplicate those rows between tabs. The existing **Access** tab retains
the existing capability and attribute information, including effective access
rows. Keep the current General/metadata presentation unchanged. **Access &
Ownership** is hidden for:

- multi-selection;
- drives;
- provider, archive, virtual, and managed-ISO paths;
- mode/ownership editing for symbolic links in phase 1 (the read-only Unix
  summary may remain visible).

### Read-only summary

At the top show:

- owner name and numeric UID;
- group name and numeric GID;
- symbolic mode and octal mode;
- a concise effective-access summary for the current user;
- whether the pending change can run directly, requires Admin Mode, or is
  unavailable.

This is display state only until the user changes a control.

### Mode editor

Use three rows, Owner / Group / Others, each with Read, Write, Execute toggles.
Show the computed octal value live. Place special bits below in an Advanced
group with clear descriptions:

- setuid: execute with file owner's effective identity;
- setgid: execute with group identity; on directories, inherit group;
- sticky: restrict deletion/rename inside a directory.

The Apply button remains disabled until a real change exists. Applying one mode
change uses the complete resulting mode in one operation, never nine separate
`chmod` calls.

### Ownership editor

Provide searchable owner and group fields populated from local account/group
enumeration. Display both name and numeric id. Invalid or unknown values cannot
be applied. Ownership changes are separate from mode changes, so a failure to
change ownership cannot partially apply a mode edit.

The UI may queue a combined Apply action only after the backend has an atomic,
well-defined transaction policy. For the first release, use independent Apply
actions and reload after each success.

### Privilege messaging

- Ordinary Properties exposes the mode editor only for a local item whose mode
  the current user may change. It still uses the typed helper, launched without
  elevation; it never invokes `chmod` or a shell directly from FM.
- **Edit Access & Ownership as Administrator** in the local-item context menu
  is the protected-location entry point.
- The action is visible and enabled only while Linux Admin Mode is active; it
  is not shown merely because the helper is installed or unlockable.
- The action opens Properties directly on **Access & Ownership** in explicit
  administrator edit mode.
- If the helper/session becomes unavailable before Apply, keep the tab open,
  disable Apply, and state the exact reason. Do not fall back to direct system
  calls or external privilege tools.

## Backend Design

### 1. Immutable permission snapshot

Add a compact `LinuxPermissionInfo` or extend the existing local capability
data with:

- `uid`, `gid`, resolved owner/group names;
- current `mode_t` bits and symbolic/octal text;
- whether the item is a symlink;
- whether mode and ownership are eligible for direct mutation;
- an optional reason when mutation is unavailable.

Use `lstat` for classification. For non-symlink local items, use `stat` data
for the mode that `chmod` would affect. No QML-side filesystem probing.

### 2. Permission request model

Create a small Linux-only permission request model, owned by C++, not QML. It
  uses numeric values rather than command strings and is dispatched only through
  the helper:

```cpp
struct PermissionChange {
    mode_t mode;
};
struct OwnershipChange {
    uid_t uid;
    gid_t gid;
    bool changeOwner;
    bool changeGroup;
};
```

Rules:

- validate an absolute local path and reject provider/archive/managed paths;
- reject symlinks in phase 1;
- preserve unchanged fields with `-1`;
- invalidate `FileAccessResolver` and refresh the model only after helper
  success;
- return errno-derived, path-safe errors.

Do not implement this with `QProcess`, `chmod`/`chown` executables, or shell
syntax.

### 3. Privileged helper extension

Add two operations consistently across all of these components:

- `LinuxAdminBroker::Operation`;
- protocol serialization/deserialization;
- helper request validation and execution;
- `LinuxAdminPolicy::Operation`;
- helper CLI tests and policy tests.

Suggested names:

- `ChangeMode` with `uint32 mode`;
- `ChangeOwnership` with optional numeric `uid` and `gid`.

The policy must reject:

- non-local, relative, archive, pseudo-filesystem, and managed-ISO paths;
- symlinks and symlinked parents under the phase-1 policy;
- missing paths;
- modes outside the allowed `07777` mask;
- UID/GID values outside the platform numeric range.

The helper executes only the corresponding system call. It receives no user or
group name, no shell string, no recursive flag, and no arbitrary operation.

### 4. Context-menu entry and controller state

`PropertiesController` becomes the only QML-facing owner of the Access &
Ownership tab state:

- exposes the permission snapshot, pending values, busy state, and last error;
- accepts an explicit `openAccessOwnershipAsAdmin(path)` request from the
  active panel/controller;
- opens the Properties overlay on the Access & Ownership tab with edit mode
  enabled only for that request;
- dispatches helper operations only;
- calls `AdminController::refreshAdminModeAfterOperation()` only after helper
  success;
- reloads its current path after success;
- tells the active `DirectoryModel` to refresh the changed entry, not the full
  directory unless the watcher already requires it.

The existing local lock/read-only badges must update from fresh access data.

## Implementation Phases

### Phase 1 — data and read-only tab

1. Define the C++ permission snapshot and local eligibility rules.
2. Expose it from `PropertiesController`.
3. Add the Linux-only Permissions tab with no edit controls enabled yet.
4. Add tests for regular files, directories, owner/group names, special bits,
   missing paths, and symlinks.

Verify: the displayed symbolic/octal mode matches `stat`, and no QML path probe
or permission mutation occurs.

### Phase 2 — administrator context-menu entry

1. Add **Edit Access & Ownership as Administrator** to the local-item context
   menu.
2. Gate visibility and enabled state on active Linux Admin Mode, one selected
   local non-symlink path, and eligible local metadata.
3. Route the action through the panel/controller into Properties with requested
   tab and explicit administrator edit mode.
4. Keep ownership controls administrator-only; show the mode editor normally
   only where the current user owns the item.

Verify: the action is absent outside active Admin Mode; it opens the correct
tab when active; ordinary Properties exposes mode changes only for eligible
local items.

### Phase 3 — privileged chmod

1. Extend broker, helper, policy, protocol versioning, and tests with
   `ChangeMode`.
2. Add the rwx mode editor, pending-change comparison, Apply/Cancel, and error
   UI for eligible ordinary items and administrator edit mode.
3. Invalidate access caches and refresh the selected entry after helper
   success.
4. Keep special bits visible; enable their editing only after the basic rwx
   operation is covered by tests.

Verify: an owned file changes mode through the unprivileged helper, and a
protected file or directory changes mode after Admin Mode auth; no other bits
change accidentally.

### Phase 4 — privileged chown/chgrp

1. Extend the same protocol path with `ChangeOwnership`.
2. Enumerate valid users/groups without shelling out.
3. Add owner/group controls and independent Apply actions.
4. Refresh session activity only after successful helper execution.

Verify: protected-file mode/owner/group changes work after Polkit auth, are
rejected while the session is not active, and cannot access forbidden or
symlink paths.

### Phase 5 — special bits and polish

1. Completed: explicit setuid/setgid/sticky changes with danger-oriented copy.
2. Completed: confirmation for setting setuid/setgid on regular executable files.
3. Pending manual verification: dark/light, narrow dialog, keyboard, and
   accessibility checks.
4. Completed: deliberate lack of ACL/recursive/symlink mutation is documented.

## Test Matrix

- regular file: administrator context action is absent outside Admin Mode and
  toggles rwx only after it is active;
- directory: administrator mode edit changes execute/traverse and refreshes
  local badges/access;
- sticky directory and setgid directory: display, edit only after confirmation;
- protected `/etc` test fixture: direct change denied, helper path succeeds;
- unknown user/group and numeric overflow: validation rejects before dispatch;
- symlink to file and dangling symlink: tab disabled, target untouched;
- provider/archive/ISO path: tab absent;
- helper unavailable, locked, active, expired, and failed states; action
  visibility must require Active;
- cancellation/close while operation is pending: UI remains consistent;
- light/dark theme and keyboard-only Apply/Cancel.

## Success Criteria

- Linux Properties exposes a clear native **Access & Ownership** workflow
  rather than Windows-only attributes.
- Permission changes never use shell execution or broaden Admin Mode into a
  generic root service.
- A failed `chmod`/`chown` does not partially mutate unrelated state.
- Successful changes are visible immediately in Properties and the file panel.
