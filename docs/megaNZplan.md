# MEGA.nz Plugin Implementation Plan

Goal: add a `mega://` provider that is comparable to the current Google Drive plugin from a user-workflow perspective: reading and writing files, creating and deleting folders, renaming, copying between the local filesystem and the cloud, previews, native file-type icons, Pathbar integration, and dedicated support for opening public MEGA links without requiring authentication.


## Current Implementation Status (June 2026)

The repository is no longer at a pure planning stage. The current codebase has reached a partial Phase 1 / early Phase 2 state:

- **Build gating is present.** `FM_ENABLE_MEGA_PLUGIN` is enabled by default, but the plugin target is built only when `SDKlib` from the MEGA SDK is found. Builds without the SDK continue without the provider.
- **Plugin skeleton exists.** `MegaFileProviderPlugin` registers the `mega` scheme, exposes `pluginId = "mega"`, and constructs a read-only `FileProvider` implementation.
- **Path parsing exists.** `MegaPath` normalizes `mega:///` and `mega://link/<id>` paths and parses modern and legacy public MEGA file/folder links into internal `mega://link/<linkId>` paths without keeping the key in `FileEntry::path`.
- **Basic cache exists.** `MegaCache` stores public-link keys in memory, link load state, cached `FileEntry` metadata, MEGA handles, and parent-to-children relationships.
- **MEGA SDK bridge exists for public reads.** `MegaClient` creates one SDK session per public link, opens public file links through `getPublicNode()`, opens public folder links through `loginToFolder()` + `fetchNodes()`, traverses nodes into the cache, and maps SDK download callbacks back to provider requests.
- **Read-only public scan/download path is partially implemented.** The provider currently supports scanning cached/loaded public-link trees, exposes read-only transfer capabilities, downloads via `.part` files, atomically renames on success, removes partial files on failure, and returns `false` for create/rename/move/remove/write operations.
- **`openRead` staging is implemented through the cleanup subsystem.** Public-link previews/materialization create a temporary staging file, register it as a `RemotePreview` cleanup lease, and schedule deletion when the returned device is destroyed or when materialization fails.
- **Unit coverage is still narrow.** `MegaPathTest` covers path normalization and public-link parsing. There are not yet fake-client integration tests for scan/download, cache isolation, errors, cancellation, or cleanup behavior.

This means the next documentation and implementation work should treat Phase 1 as mostly complete, Phase 2 as in progress, and Phases 3-6 as not started.

### Mandatory temporary-file policy for MEGA

All MEGA temporary artifacts **must** be allocated, registered, and retired through `CleanupSubsystem`; ad-hoc files in `QDir::tempPath()`, unmanaged `QTemporaryFile` auto-removal, or SDK state/cache files in the process working directory are not acceptable for production code. This rule applies to:

- `openRead` staging files for Quick Look, previews, and external opening;
- download `.part` files when the destination is provider-owned staging rather than a user-selected final path;
- upload-on-close staging files for any future `openWrite` implementation;
- thumbnail/preview fallback materialization;
- provider-to-provider transfer payloads;
- any MEGA SDK state/cache directory that would otherwise be emitted into the current working directory.

When a temporary artifact has to outlive a single stack frame, the provider must keep the cleanup lease id with the owning object and call `scheduleDelete()`, `scheduleDeleteOnFailure()`, or `completeWithoutDelete()` according to the operation result. Tests for new transfer/materialization features should verify that cancellation and failure do not leave unmanaged temporary files behind.

## 1. Scope and Functional Targets

### 1.1. What Google Drive parity means

The MEGA plugin should cover the same classes of operations expected from a cloud file provider in FMQml:

- navigating account roots, folders, the rubbish bin, and published links;
- retrieving metadata: name, size, dates, MIME/suffix, folder flag, read-only flag, and hidden/system flags where applicable;
- downloading files for local operations, Quick Look, previews, and external opening through existing materialization paths;
- uploading local files to MEGA with progress, cancellation, and correct name-conflict handling;
- creating files and folders;
- deleting, moving, and renaming;
- showing used/free account storage;
- caching metadata so the panel does not perform a full network traversal for every small action;
- plugin actions: sign in/out, authorization status, open link, copy MEGA link, and possibly import a public link into the account;
- predictable public-link behavior: links open read-only without authentication, and write/delete operations are unavailable.

### 1.2. Important MEGA-vs-Google Drive difference

MEGA uses client-side encryption, so the provider should not be designed as a thin HTTP wrapper around a REST API. A practical implementation should rely on the official MEGA SDK, or on a small adapter around it, because the plugin needs:

- decrypted names, attributes, previews, and file contents;
- correct handling of public file/folder links and URL keys;
- chunked transfer, retry, resume, and API limit handling;
- account filesystem events;
@@ -166,73 +195,73 @@ For a public link:
3. on cache miss, request children from the SDK;
4. convert each node into `FileEntry`:
   - `name`, `path`, `suffix`, `size`, `sizeText`;
   - `modified`, `created`, and date display text;
   - `isDirectory`, `isReadOnly`, `isImage`, `hasThumbnail`;
   - `mimeType` through `QMimeDatabase` by name and/or SDK attributes;
   - `iconName` through the shared resolver or file-type asset names;
   - `providerCapabilitiesText` for read-only/public/quota states.
5. emit batches rather than one huge list for large folders;
6. finish with `finished(path, success, generation, error)`.

### 4.3. Downloading

`copyToLocalFile(sourcePath, destinationFilePath, progress, error)`:

- use SDK download transfers;
- write to a `.part` file next to the destination and atomically rename after success, unless the upper layer already owns this behavior;
- propagate progress and cancellation;
- map quota, bandwidth, auth, not-found, and permission-denied errors;
- do not leave partial files behind after cancellation or error.

`openRead(path, stagingParentPath)`:

- for small files, downloading to staging and returning a `QFile` is acceptable;
- for large files, prefer a streaming/range adapter, but that can be a later iteration;
- staging must be registered with the cleanup subsystem once it is centralized.
- staging must be registered with `CleanupSubsystem`; this is mandatory for MEGA temporary files, not a later cleanup task.

### 4.4. Uploading

`copyFromLocalFile(sourceFilePath, destinationPath, progress, error)`:

- interpret `destinationPath` according to the current operation contract: either as the full future path or as parent+name;
- verify that the parent exists and is writable;
- use an SDK upload transfer;
- update `MegaCache` after successful upload;
- support overwrite/rename-on-conflict according to the existing FMQml policy;
- show a clear error on quota exceeded.

`copyFromLocalFiles(items, progress, error)`:

- implement after single-file upload;
- group uploads by parent folder;
- limit concurrency so the plugin does not hit SDK/API constraints or make the UI noisy;
- return the current file through the progress callback.

`openWrite(path, truncate)`:

- MVP: use a staging file that uploads to MEGA on close;
- MVP: use a `CleanupSubsystem`-managed staging file that uploads to MEGA on close;
- explicitly define where upload-on-close errors surface so the upper layer does not treat the operation as successful too early;
- if the current `openWrite` contract cannot express an async commit, temporarily avoid advertising workflows that require direct writes through `QIODevice` and rely on `copyFromLocalFile` instead.

### 4.5. Create, delete, rename, and move

- `createFolder(parentPath, name)` — SDK create folder, then cache insert.
- `createFile(parentPath, name)` — create an empty local temp file and upload it, or use an SDK create-empty-file operation if available.
- `renamePath(oldPath, newName)` — SDK rename, then update cache paths for the subtree.
- `movePath(sourcePath, destinationPath)` — SDK move node inside the account; for public links, return false/read-only.
- `removePath(path)` — decide UX semantics: move to Rubbish Bin by default, and expose hard delete only as a separate action or when the MEGA SDK treats remove that way. For Google Drive parity, prefer trash-like behavior first if the SDK allows it.

## 5. Previews and Native Icons

### 5.1. Icons

MEGA should not introduce a separate file-type icon system. Rules:

- folders use the shared folder icon;
- files use `FileTypeIconResolver` by suffix/MIME;
- MEGA-specific entities get separate overlays/assets only when needed: public-link root, rubbish bin, shared folder;
- `FileEntry::iconName` should be populated with the same values the UI expects for local/GDrive files.

### 5.2. Thumbnail pipeline

Priorities:
@@ -374,71 +403,72 @@ Output: the plugin loads but may return a clear `MEGA SDK unavailable/not signed
Output: the requirement to paste and open MEGA links from the Pathbar is satisfied in read-only mode.

### Phase 3. Account authorization and account reads

- Login/session resume/logout/status.
- `mega:///` scan for Cloud Drive.
- Metadata cache.
- Storage quota.
- Read/download/openRead for account files.

Output: private account data can be browsed and downloaded.

### Phase 4. Account writes and mutations

- Upload local -> MEGA.
- Create folder/file.
- Rename/move/remove.
- Conflict policy.
- Batch upload with limited concurrency.
- Cache invalidation after mutations.

Output: core file operations are comparable to Google Drive.

### Phase 5. Previews, icons, and polish

- SDK thumbnails/previews.
- SDK thumbnails/previews, with every fallback materialization registered in `CleanupSubsystem`.
- Thumbnail cache.
- Icons/overlays for MEGA-specific roots.
- Quick Look UX for large files.
- `copyLink` and `importLink` actions.

Output: cloud UX looks native in the file panel.

### Phase 6. Watch/events and reliability

- SDK event bridge -> cache invalidation/rescan.
- Retry/backoff for transfers.
- Resume interrupted transfers if the SDK and operation contract allow it.
- Diagnostics and debug logging without secrets.

Output: the provider is resilient to external changes and network failures.

## 10. Risks and Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| MEGA SDK is hard to build | Plugin breaks portable builds | Optional CMake, feature flag, CI without SDK |
| Client-side encryption | A quick REST-only implementation is not viable | Use the SDK as the required backend |
| Public-link key leaks into history/logs | Access to files can leak | Internal `linkId`, log redaction, no key in `FileEntry::path` |
| `openWrite` async commit does not match the `QIODevice` contract | False successful writes | For MVP, rely on `copyFromLocalFile`; limit direct `openWrite` |
| Large thumbnails/Quick Look downloads gigabytes | Poor UX and bandwidth waste | SDK preview first, size limits, explicit progress |
| MEGA quota/bandwidth limits | Operations fail unclearly | Error mapping and user-visible messages |
| SDK callbacks arrive off the UI thread | Race/crash | Single Qt bridge layer, queued connections, generation checks |
| Unmanaged MEGA temporary files or SDK state files | Disk bloat, secret leakage, cleanup regressions | Mandatory `CleanupSubsystem` leases for staging/materialization plus explicit SDK state root outside the process working directory |
| Move/delete semantics differ from GDrive | Data loss | Prefer trash-like remove first; expose hard delete only as a separate action |

## 11. MVP Definition of Done

The MVP is ready when all of the following are true:

- `mega:///` opens the authenticated account after login/session resume;
- a public `https://mega.nz/folder/...` link opens from the Pathbar without authentication;
- a public `https://mega.nz/file/...` link opens from the Pathbar without authentication and the file can be downloaded;
- a file can be downloaded from the account to a local disk;
- a local file can be uploaded into the account;
- a folder can be created, renamed, and deleted in the account;
- `FileEntry` correctly populates size, dates, MIME/suffix, directory/read-only flags;
- images get thumbnails either through SDK preview or through the existing fallback;
- file icons match local files of the corresponding types;
- auth/quota/not-found/network errors are visible to the user;
- public-link secrets and sessions are not written to logs;
- there are unit tests for paths and error mapping, plus fake-client integration tests for transfer/scan.
