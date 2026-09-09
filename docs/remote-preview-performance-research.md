# Remote preview downloads and reuse

Code review and implementation: 2026-09-09.

## Implemented for Google Drive

- `GDriveThumbnailLoader::NetworkWorker` creates its network manager on its own
  thread on the first request and retains it. Four existing workers remain; no
  extra concurrency was added. The loopback regression verifies eight requests
  use four TCP connections, rather than creating a connection for each request.
- Full preview downloads use a thread-local manager through
  `GDriveTransferClient::previewNetwork()`. It survives the provider instance and
  expires with the Quick Look pool thread. Ordinary copy/batch transfers retain
  their existing managers and scheduling.
- Preview downloads poll cancellation every 100 ms, independently of incoming
  progress. The callback retains the last byte count and does not restart the
  network inactivity timeout. This covers a silent server before the first byte
  and a transfer stalled after partial progress. Authentication refresh and
  shortcut metadata requests are still separate blocking operations; this change
  does not give them independent cancellation.
- The common materializer can reuse validated full files through
  `FileProvider::previewCacheIdentity()`. Google is the first provider to opt in.
  Its identity combines a non-secret authorization-session nonce, normalized path,
  modification timestamp, and size. Logout/token replacement invalidates the
  session identity. Missing authorization, missing version metadata, denied
  download capability, shortcuts, and native Google document exports bypass reuse.

## Full-file cache contract

`RemotePreviewCache` is an in-process index of cleanup-managed files, not a second
persistent thumbnail cache. It retains at most eight entries / 128 MiB of primary
file bytes. Reuse expires five minutes after insertion; expiry is checked on cache
access/insertion. Active previews hold shared ownership, so eviction cannot remove
a file still in use. Active files can therefore outlive the cache budget/TTL.
Derived cover files are not included in the primary-file byte budget.

Only successful, validated downloads enter the cache. A changed session/version
causes a miss; explicit reload bypasses lookup. A missing or truncated cached file
also causes a miss. Freshness relies on the provider's current metadata snapshot,
with the five-minute limit bounding reuse when that snapshot has not refreshed.
There is no extra metadata request on each cache hit.

Each artifact keeps its existing `CleanupSubsystem` lease until the last cache or
preview owner releases it. Controller teardown clears cache-only ownership while
cleanup services are still alive. Crash recovery remains owned by startup cleanup.
Concurrent misses for the same identity can still download independently; this
change does not introduce a shared transfer with multiple cancellation owners.

Adding the optional virtual method changes `FileProvider` ABI. Provider API version
is now 3; all bundled provider plugins are rebuilt together, and old provider
binaries are rejected by the existing version check.

## Other providers: should the same work be applied?

| Provider | Transport lifetime today | Full-file reuse | Cancellation today | Recommended next work |
| --- | --- | --- | --- | --- |
| Instagram | `InstagramNetwork::httpGetBytes()` creates a manager for every call; it also buffers the complete body before writing it | No opt-in to the new materializer cache | Callback is checked on `downloadProgress` | Strong candidate for connection reuse and independent cancellation. Keep authenticated and cookie-free requests isolated when retaining managers. Add a session-scoped media-version identity before opting into file reuse; signed URL text alone is not a stable content version. Stream large bodies to staging as a separate change. |
| MEGA | Shared SDK client already owns transport | No opt-in; repeated materialization starts another SDK download | `MegaFileProvider::copyToLocalFile()` waits for SDK completion and invokes `cancelAll()` when progress rejects a transfer or timeout expires | Do not add a Qt network manager. First implement per-request cancellation so abandoning a preview cannot cancel unrelated transfers. Then opt into reuse with account/session scope and node fingerprint/version. |
| Telegram | Shared TDLib client; `downloadFile()` can return an already downloaded local file | TDLib already retains downloads; the materializer still copies them to its own staging directory | Cancellation is checked when file updates report progress; the receive loop polls every 100 ms but does not independently check the preview callback | Prefer cancellation checks during silent receive iterations and preserving TDLib reuse. An extra full-file cache is lower priority: it would mainly save local copying. Audit thumbnail-fallback downloads too; some currently receive no progress callback. |
| Portable devices | Windows WPD or Linux KIO, not Qt HTTP; Linux routes jobs to the application thread | No opt-in to the new cache | Linux cancellation follows `processedAmountChanged`; backend behavior differs on Windows | A bounded cache may avoid expensive device reads, but needs device-connection generation plus object identity/version and unplug invalidation. Cancel the individual KIO/WPD job independently of progress; validate separately on both platforms. |
| FTP | `FtpClient` owns raw socket operations; no QNetworkAccessManager | The plugin currently exposes downloads through `openRead()` and does not override `copyToLocalFile()` / `copyToLocalFileForPreview()` | `openRead()` constructs its client with an always-false cancellation predicate | First establish the preview materialization/cancellation contract. Then assess session reuse and a credential-scoped file identity. Applying the HTTP-manager patch would not address this path. |

The common cache deliberately remains disabled for these providers until they
supply a suitable identity. The Google changes do not modify their transport or
cancellation behavior. Existing generation checks still discard obsolete results.

## Evidence and validation

Relevant source paths:

- `src/plugins/gdrive/GDriveThumbnailLoader.cpp`
- `src/plugins/gdrive/GDriveTransferClient.cpp`
- `src/plugins/gdrive/GDriveFileProvider.cpp`
- `src/plugins/gdrive/GDriveAuth.cpp`
- `src/preview/ProviderPreviewMaterializer.cpp`
- `src/preview/RemotePreviewCache.cpp`
- `src/plugins/instagram/InstagramNetwork.cpp`
- `src/plugins/mega/MegaFileProvider.cpp`
- `src/plugins/telegram/TelegramClient.cpp`
- `src/plugins/telegram/TelegramFileProvider.cpp`
- `src/plugins/portable_device/PortableDeviceProvider.cpp`
- `src/plugins/ftp/FtpFileProviderPlugin.cpp`

Deterministic checks:

- `gdrive_thumbnail_network_test`: actual loopback HTTP connection reuse across
  requests handled by the production thumbnail workers; no cloud credentials.
- `preview_download_cancellation_test`: silent reply before first byte and after
  partial progress; cancellation without another network event.
- `remote_preview_cache_test`: entry/byte budgets, LRU access, session/version
  misses, truncated-file rejection, and cleanup after the last owner releases.
- `provider_preview_materializer_test`: production materializer/cache/cleanup with
  a fake provider and format helpers; A-B-A reuse, explicit reload, version/session
  change, cancellation before downloading and before validation, and rejection/retry
  of invalid content.

Final build and all six focused preview/thumbnail/cleanup tests pass.

These tests do not establish live Google latency or subjective UI smoothness.
Manual acceptance should cover repeated A-B-A navigation, a refresh after changing
a remote file, rapid A-B-C navigation on a slow connection, account switch/logout,
and both Preview Pane and Quick Look. Measure request count, first-byte delay,
transfer duration, validation time, and queue wait separately.
