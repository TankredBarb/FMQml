# MEGA.nz Plugin Implementation Plan

Goal: add a `mega://` provider that feels native in FMQml for the workflows that make sense for MEGA: browsing account and public-link trees, copying files and folders to/from the cloud, deleting account items, previews, native file-type icons, Pathbar integration, and dedicated support for opening public MEGA links without requiring authentication.


## Current Implementation Status (June 2026)

The repository is no longer at a pure planning stage. The current codebase has reached a stable account baseline, and practical account storage mutations are present in the MEGA provider implementation. The intended product scope is narrower than the provider's internal API surface: MEGA should behave as a cloud storage source/target, not as a manual document-creation workspace.

- **Build gating is present.** `FM_ENABLE_MEGA_PLUGIN` is enabled by default, but the plugin target is built only when `SDKlib` from the MEGA SDK is found. Builds without the SDK continue without the provider.
- **Plugin skeleton exists.** `MegaFileProviderPlugin` registers the `mega` scheme, exposes `pluginId = "mega"`, and constructs a read-oriented `FileProvider` implementation in the committed source tree.
- **Path parsing exists.** `MegaPath` normalizes `mega:///` and `mega://link/<id>` paths and parses modern and legacy public MEGA file/folder links into internal `mega://link/<linkId>` paths without keeping the key in `FileEntry::path`.
- **Basic cache exists.** `MegaCache` stores public-link keys in memory, link load state, cached `FileEntry` metadata, MEGA handles, and parent-to-children relationships.
- **MEGA SDK bridge exists for public reads.** `MegaClient` creates one SDK session per public link, opens public file links through `getPublicNode()`, opens public folder links through `loginToFolder()` + `fetchNodes()`, traverses nodes into the cache, and maps SDK download callbacks back to provider requests.
- **Public and account scan/download paths are implemented in-tree.** The provider supports scanning cached/loaded public-link trees and signed-in `mega:///` account trees, exposes transfer capabilities, downloads via `.part` files, atomically renames completed downloads into place, removes partial files on failure, supports account upload/delete, and keeps public-link paths read-only.
- **`openRead` staging is implemented through the cleanup subsystem.** Public-link previews/materialization create a temporary staging file, register it as a `RemotePreview` cleanup lease, and schedule deletion when the returned device is destroyed or when materialization fails.
- **Account authorization and read-only access exist.** The action layer exposes MEGA sign-in/sign-out/status, Settings provides a credential dialog, saved sessions are stored through the platform credential store and resumed, `mega:///` appears in Places, and account storage usage is reported from the cached account tree.
- **Account mutation support covers the practical storage scope.** The provider supports copying files into the account, copying/downloads from the account, deleting account files and folders, and previews of uploaded files. Fresh upload cache entries retain MEGA handles so immediate preview and delete can work without requiring a rescan.
- **Manual create/rename is intentionally out of scope for the MEGA UX.** The UI should treat MEGA as a cloud storage target/source rather than a document-creation workspace: users copy data there and delete data there, but direct "New File", direct "New Folder", direct move, and rename should not be exposed as user-facing MEGA actions. Provider/backend methods may remain available for future product decisions or internal transfer helpers, but QML affordances and command-palette actions must not offer them for `mega://` paths.
- **Unit coverage covers the current path.** `MegaPathTest` covers path normalization and public-link parsing; `MegaProviderPublicLinkTest` covers public scan/download errors/cancellation, account scan, sign-in/sign-out actions, cached account storage usage, account upload/delete, enriched uploaded-file metadata, and public-link read-only guards. Direct create/rename/move methods are not product requirements and should not be required by product-scope regression tests.

This means Phase 3 should be treated as complete for the committed read/account baseline, except that exact account quota limits depend on the SDK account-details bridge. Phase 4 should be treated as the practical MEGA storage workflow: upload/copy into the account, download/copy out, delete account items, and keep public links read-only. Phase 5 polish has started with native presentation metadata and a dedicated execution plan. Phase 6 has a safe reliability baseline for dirty marking and diagnostics; automatic full-account refresh remains intentionally out of scope.

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

The MEGA plugin should cover the classes of operations expected from a cloud storage provider in FMQml:

- navigating account roots, folders, the rubbish bin, and published links;
- retrieving metadata: name, size, dates, MIME/suffix, folder flag, read-only flag, and hidden/system flags where applicable;
- downloading files for local operations, Quick Look, previews, and external opening through existing materialization paths;
- uploading local files and folders to MEGA with progress, cancellation, and correct name-conflict handling;
- creating account folders internally only when recursive folder copy/upload needs them;
- deleting account files and folders;
- keeping manual "New File", manual "New Folder", direct move, and rename out of the user-facing MEGA UI unless a future product decision explicitly changes that scope;
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

## 2. Scanning and Metadata Flow

For an account path:

1. normalize the `mega://` path;
2. use cached children when they are available and still valid;
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

For a public link, store the link key only in the in-memory cache and expose only
the internal `mega://link/<linkId>` path to the rest of the application.

## 4. Account Writes

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

- MVP: use a `CleanupSubsystem`-managed staging file that uploads to MEGA on close;
- explicitly define where upload-on-close errors surface so the upper layer does not treat the operation as successful too early;
- if the current `openWrite` contract cannot express an async commit, temporarily avoid advertising workflows that require direct writes through `QIODevice` and rely on `copyFromLocalFile` instead.

### 4.5. Account mutations

- `copyFromLocalFile(sourceFilePath, destinationPath, progress, error)` is the primary write path for account files.
- recursive folder copy/upload may call an internal folder-create path; successful folder creates must cache the MEGA handle immediately so nested uploads and later folder deletion work without a full rescan.
- successful file uploads must cache the MEGA handle immediately so previews and downloads work right after upload.
- `removePath(path)` should support account files and folders; for public links, return false/read-only.
- `renamePath(oldPath, newName)`, direct move, and direct user-facing `createFile/createFolder` are intentionally not part of the MEGA UI scope. Backend support can remain for future work or transfer helpers, but UI menus, toolbar buttons, shortcuts, and command-palette commands must not expose them for `mega://` paths.
- delete semantics should be checked against MEGA SDK behavior: prefer Rubbish Bin semantics when the SDK operation allows it, and expose hard delete only as a clearly separate future action.

## 5. Previews and Native Icons

### 5.1. Icons

MEGA should not introduce a separate file-type icon system. Rules:

- folders use the shared folder icon;
- files use `FileTypeIconResolver` by suffix/MIME;
- MEGA-specific entities get separate overlays/assets only when needed: public-link root, rubbish bin, shared folder;
- `FileEntry::iconName` should be populated with the same values the UI expects for local/GDrive files.

### 5.2. Thumbnail pipeline

Priorities:

1. prefer SDK-provided thumbnails/previews when the bridge is available;
2. use bounded full-file fallback materialization only for files under the safety limit;
3. register every fallback staging file with `CleanupSubsystem`.

## 9. Phase Roadmap

### Phase 2. Public links

Output: the requirement to paste and open MEGA links from the Pathbar is satisfied in read-only mode.

### Phase 3. Account authorization and account reads

- Login/session resume/logout/status.
- `mega:///` scan for Cloud Drive.
- Metadata cache.
- Storage quota.
- Read/download/openRead for account files.

Output: private account data can be browsed and downloaded.

### Phase 4. Account storage writes and deletes

- Copy/upload local files -> MEGA.
- Copy/upload local folders -> MEGA, including internal folder creation and nested file uploads.
- Download/copy MEGA account files/folders -> local.
- Delete account files and folders.
- Keep public links read-only.
- Cache uploaded files and created folders with MEGA handles immediately so preview, nested upload, and delete work without requiring a rescan.
- Conflict policy for same-name copy targets.
- Batch upload with limited concurrency.
- Cache invalidation/rescan after external changes.
- Do not expose manual New File, New Folder, direct move, or rename in the MEGA UI.

Output: MEGA behaves as a reliable cloud storage source/target in the file panel. Manual document creation and rename remain intentional non-goals for this provider.

### Phase 4 follow-up checklist

- Add/keep regression tests for uploading a folder containing files into an internally created MEGA folder.
- Add/keep regression tests for previewing an image immediately after upload.
- Add/keep regression tests for deleting newly uploaded folders, not only individual files.
- Add conflict-policy coverage for copy/upload into a folder that already contains the target name.
- Verify cancellation cleanup for large uploads and recursive folder uploads.
- Verify that public-link paths reject all write/delete operations.
- Verify UI affordances: context menus, empty-folder menus, toolbar actions, shortcuts, and command-palette entries must offer upload/paste/delete where appropriate but must not offer manual create, direct move, or rename for `mega://` paths.

### Phase 5. Previews, icons, and polish

Detailed plan: `docs/mega-phase5-polish-plan.md`.

- Native MEGA presentation metadata baseline for scanned and freshly mutated entries.
- SDK thumbnails/previews before full-file fallback downloads.
- Every fallback materialization registered in `CleanupSubsystem`.
- Thumbnail/preview cache keyed by provider path, MEGA handle, size, and modified timestamp where available.
- Icons/overlays for MEGA-specific roots, public links, shared folders, and rubbish-bin-like nodes when SDK metadata supports them.
- Quick Look UX for large files with clear progress/size feedback.
- Safe link actions such as `copyLink`, `openInBrowser`, and possibly `importLink`, without leaking public-link keys into paths/logs/history.

Output: MEGA cloud UX looks native in the file panel without expanding into manual document creation or rename workflows.

### Phase 6. Watch/events and reliability

Detailed plan: `docs/mega-phase6-reliability-plan.md`.

- SDK event bridge -> dirty marking and explicit refresh.
- Retry/backoff for transfers only when it can be implemented asynchronously without blocking provider calls.
- Resume interrupted transfers remains deferred until the SDK and operation contract can persist and validate transfer state safely.
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
| Exposing create/rename/move in MEGA UI | Users treat MEGA as an editor workspace and hit unsupported semantics | Keep direct New File/New Folder/Rename/direct move hidden for provider paths; use internal folder creation only for recursive copy/upload |

## 11. MVP Definition of Done

The MVP is ready when all of the following are true:

- `mega:///` opens the authenticated account after login/session resume;
- a public `https://mega.nz/folder/...` link opens from the Pathbar without authentication;
- a public `https://mega.nz/file/...` link opens from the Pathbar without authentication and the file can be downloaded;
- a file can be downloaded from the account to a local disk;
- a local file can be uploaded into the account;
- a local folder can be copied/uploaded into the account;
- account files and folders can be deleted;
- `FileEntry` correctly populates size, dates, MIME/suffix, directory/read-only flags;
- images get thumbnails either through SDK preview or through the existing fallback;
- file icons match local files of the corresponding types;
- auth/quota/not-found/network errors are visible to the user;
- public-link secrets and sessions are not written to logs;
- there are unit tests for paths and error mapping, plus fake-client integration tests for transfer/scan.
