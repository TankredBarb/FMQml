# Provider-Native Thumbnails Plan

Goal: add thumbnails for provider-backed files where cheap provider-native
thumbnail data exists, without spending full-file bandwidth or API quota. Target
providers: MTP/portable devices, Google Drive, and MEGA.

## Current State

- QML delegates already request thumbnails lazily through
  `image://thumbnail/<encoded path>`.
- `FileEntry::hasThumbnail` is already used by local and provider entries to
  decide whether a delegate should try thumbnail loading.
- `ThumbnailProvider::requestImage()` has a provider fast path:
  `FileProviderPluginRegistry::thumbnailUrlForPath(path)`.
- Today that fast path is only useful for Instagram HTTP thumbnail URLs and
  local-file thumbnail URLs.
- Generic provider fallback materialization is intentionally disabled for
  provider paths without a local thumbnail URL. That is correct: downloading an
  entire cloud/MTP file in the QQuickPixmapReader path is too expensive and has
  already caused lifecycle crashes.
- Some providers currently mark files as thumbnail-capable by suffix even though
  they do not expose a cheap thumbnail source:
  - GDrive: `entry.hasThumbnail = false`.
  - MEGA: images/videos are marked thumbnail-capable, but only full download
    paths exist.
  - Portable/MTP: images/videos are marked thumbnail-capable, but only full
    object copy paths exist.

## Non-Goals

- Do not re-enable full-file provider materialization for thumbnails.
- Do not download full remote videos to extract frames.
- Do not increase `kProviderThumbnailMaterializeLimit`.
- Do not fetch provider metadata per visible delegate.
- Do not turn thumbnail failures into user-facing errors during browsing.

## Policy

Provider thumbnails are allowed only when all of these are true:

- The provider can resolve a thumbnail source from already-known metadata or a
  bounded provider-native thumbnail API.
- The request is size-limited and timeout-limited.
- The request has provider-level concurrency limits.
- Failures are negative-cached.
- Cache identity includes enough provider revision data to avoid obvious stale
  thumbnails: provider path, requested bucket size, and at least one of handle,
  modified timestamp, file size, thumbnail URL, or provider revision.

If no cheap source exists, `hasThumbnail` must be false and the UI should show
the normal file-type icon.

## Shared Architecture

### 1. Add A Provider Thumbnail Contract

The existing `thumbnailUrlForPath()` string is too narrow:

- GDrive thumbnail URLs need authorization headers.
- MEGA thumbnails are SDK requests that write to a local file.
- MTP thumbnails may be platform resource streams, not URLs.

Add a provider thumbnail contract rather than overloading the URL string:

```cpp
struct ProviderThumbnailResult {
    enum class Kind { None, LocalFile, EncodedBytes, TemporaryUnavailable };
    Kind kind = Kind::None;
    QString cacheIdentity;
    QString localFilePath;
    QByteArray encodedBytes;
    QString mimeType;
};
```

Expose it either on `FileProviderPlugin` or `FileProvider`. Prefer
`FileProvider` if the implementation needs auth/session state, caches, or
provider APIs. The registry can still route by path and instantiate the provider
like existing metadata/materialization code.

Suggested API:

```cpp
virtual ProviderThumbnailResult thumbnailForPath(
    const QString &path,
    const QSize &requestedSize,
    QString *error) const;
```

The method may block briefly, but must enforce its own timeout and size limits.
It must not call `copyToLocalFile()` or `copyToLocalFileForPreview()` unless the
provider implementation can prove the provider-native thumbnail API is being
used.

### 2. Unify ThumbnailProvider Handling

In `ThumbnailProvider::requestImage()`:

1. Keep the current local-file and archive behavior.
2. For provider paths, call the new provider thumbnail contract before returning
   empty.
3. Decode `EncodedBytes` with `QBuffer` + `QImageReader`.
4. Decode `LocalFile` with `QImageReader`.
5. Scale to the existing bucket size.
6. Insert into the existing in-memory cache.
7. Negative-cache permanent misses.
8. Do not negative-cache `TemporaryUnavailable`; let later scroll/refresh retry.

Keep the Instagram branch initially, then migrate it onto the shared contract
once GDrive/MEGA/MTP prove the shape.

### 3. Concurrency And Budget Defaults

Initial defaults:

- Max provider thumbnail response: 2 MB.
- Timeout: 5 seconds for HTTP/native resource requests, 8 seconds for MEGA SDK
  thumbnail/preview requests.
- Global provider thumbnail concurrency: 2.
- Per-provider concurrency: 1 for GDrive and MEGA, 1 for MTP.

These are intentionally conservative. Thumbnails are decoration; browsing must
remain responsive and quota-safe.

## Google Drive Plan

### Current Code

- `DriveListFields` and `DriveFileFields` do not request `thumbnailLink`.
- `entryFromDriveFileObject()` always sets `entry.hasThumbnail = false`.
- GDrive already has OAuth/authenticated network infrastructure and shared
  metadata caches.

### Cheap Source

Use Drive metadata `thumbnailLink` when it is already returned by list/file
metadata. Do not call `files.get` just to discover thumbnails for visible rows.

Implementation steps:

1. Extend `DriveListFields` and `DriveFileFields` with `thumbnailLink`.
2. Add a thumbnail metadata cache keyed by normalized GDrive item path.
3. In `entryFromDriveFileObject()`:
   - set `hasThumbnail = true` only when `thumbnailLink` is non-empty;
   - preserve `false` for folders, shortcuts without target metadata, Google
     app files without thumbnail metadata, and unknown rows.
4. Cache `thumbnailLink` alongside existing `m_mimeTypes`/shared metadata when
   list results are processed.
5. Implement provider thumbnail fetch:
   - resolve normalized path and cached `thumbnailLink`;
   - if missing, return `None`;
   - issue authenticated GET using the existing Drive access token;
   - enforce response size and timeout;
   - decode from bytes in `ThumbnailProvider`.

Cache identity:

- `gdrive:<fileId>:<modifiedTime>:<size>:<thumbnailLink hash>`.

Quota policy:

- Listing may include `thumbnailLink` because listing already happens for the
  panel.
- Thumbnail loading must not trigger additional metadata calls.
- Thumbnail bytes should be fetched only for visible delegates and should be
  negative-cached on 404/403.
- 401 should be `TemporaryUnavailable` after auth refresh failure, not a
  permanent negative cache.

Acceptance:

- Images/videos with `thumbnailLink` show thumbnails in grid, brief, details,
  and hover preview.
- A folder with 100 image files performs one list request plus lazy thumbnail
  byte requests only for visible rows.
- Offline/expired links fall back to icons without repeated request storms.

## MEGA Plan

### Current Code

- MEGA marks images/videos as `hasThumbnail` by suffix in
  `MegaPresentation::enrichEntryPresentation()` and `MegaClient::traverse...`.
- Only full `startDownload()` materialization exists today.
- The local MEGA SDK header exposes `MegaApi::getThumbnail()` and
  `MegaApi::getPreview()`.

### Cheap Source

Use MEGA SDK node attributes:

- Prefer `getThumbnail()` for small file-panel thumbnails.
- Use `getPreview()` only when thumbnail is unavailable and only if the SDK
  request remains bounded and cheap.
- Never use `startDownload()` for thumbnails.

Implementation steps:

1. Extend `MegaClientInterface` with a thumbnail request:
   `getNodeThumbnail(path, destinationFilePath, preferPreviewFallback)`.
2. Implement it in `MegaClient` using the right SDK session:
   - account paths use `accountApiSession()`;
   - public-link paths use `sessionForLink(linkId)`;
   - resolve the cached node handle from `MegaCache`;
   - call `MegaApi::getThumbnail(node, dst)` first;
   - optionally call `MegaApi::getPreview(node, dst)` if thumbnail is missing
     and the file type benefits from preview fallback.
3. Add dedicated request tracking for thumbnail/preview SDK requests. Do not
   mix them with transfer tracking.
4. Store SDK output in a cleanup-managed thumbnail staging directory.
5. Return `LocalFile` from the provider thumbnail contract.
6. Add a small persistent provider thumbnail cache later if needed, but start
   with cleanup-managed temp files plus in-memory `ThumbnailProvider` cache.

Cache identity:

- `mega:<nodeHandle>:<mtime>:<size>:thumb`.

Quota/bandwidth policy:

- SDK thumbnail/preview requests are allowed.
- Full node download is forbidden for thumbnail generation.
- MEGA transfer quota errors should return `TemporaryUnavailable` and should
  disable more MEGA thumbnail attempts for a short cooldown window.

Acceptance:

- MEGA image/video entries no longer start full downloads for thumbnails.
- Public link and account paths both work after their node trees are cached.
- If SDK thumbnail is absent, the entry either uses preview fallback or icon
  fallback; no full file download occurs.

## MTP / Portable Device Plan

### Current Code

- Portable provider marks image/video suffixes as thumbnail-capable.
- Windows implementation uses WPD metadata and full `WPD_RESOURCE_DEFAULT`
  object streams for copies.
- Linux implementation routes through KIO MTP and full `KIO::file_copy`.
- No provider-native thumbnail resource is currently used.

### Cheap Source

Only use platform-native thumbnail resources:

- Windows WPD: investigate `IPortableDeviceResources` thumbnail/icon resources
  for each object and request only those resources.
- Linux/KIO MTP: use a KIO-provided thumbnail/local preview only if it is
  exposed without copying the full object. If KIO does not expose this, leave
  thumbnails disabled.

Implementation steps:

1. Stop setting `hasThumbnail = true` solely because the file suffix is image or
   video for portable paths.
2. During metadata listing, mark `hasThumbnail = true` only if the platform
   reports an actual thumbnail-capable resource or cheap thumbnail URL/local
   path.
3. Windows:
   - extend metadata/resource probing to ask for available object resources;
   - if a thumbnail resource exists, stream only that resource with size and
     timeout limits;
   - return `EncodedBytes` or a cleanup-managed `LocalFile`.
4. Linux:
   - inspect KIO MTP `UDSEntry` for any local thumbnail/icon/preview field;
   - if unavailable, do not implement thumbnail fetching;
   - keep full `KIO::file_copy` only for explicit preview/open/copy actions.
5. Keep device removal/unlock errors silent for thumbnail requests and return
   `TemporaryUnavailable`.

Cache identity:

- `portable:<deviceId>:<objectId>:<modifiedTime>:<size>:thumb`.

Acceptance:

- Devices that expose native thumbnails show them lazily.
- Devices that do not expose native thumbnails show icons.
- Browsing a phone folder never copies original photos/videos just to populate
  thumbnails.

## Rollout Order

1. Shared provider thumbnail contract and `ThumbnailProvider` integration.
2. GDrive metadata/cached `thumbnailLink` implementation.
3. MEGA SDK `getThumbnail/getPreview` implementation.
4. Portable/MTP capability detection; implement only platforms with proven
   native thumbnail resources.
5. Migrate Instagram onto the shared contract if the new shape works well.

## Tests And Instrumentation

Add logging under an env flag, for example `FM_PROVIDER_THUMBNAIL_TRACE=1`:

- provider name;
- path hash or redacted path;
- cache hit/miss/negative/temporary-unavailable;
- source kind;
- bytes downloaded/read;
- elapsed ms;
- reason for fallback to icon.

Add tests where practical:

- `ThumbnailProvider` provider path with no thumbnail returns empty and does not
  call full materialization.
- GDrive entry parsing stores `thumbnailLink` and sets `hasThumbnail`.
- GDrive missing `thumbnailLink` keeps `hasThumbnail = false`.
- MEGA thumbnail request uses SDK thumbnail path, not `startDownload`.
- Portable entries do not mark suffix-only thumbnails unless native capability
  is detected.

Manual QA:

- Grid, brief, details, and hover preview show provider thumbnails consistently.
- Scrolling large provider folders stays responsive.
- Disconnect network/device during thumbnail loading; UI falls back to icons.
- Provider quota/rate-limit errors do not loop.

## Open Questions

- Should provider thumbnails get a small persistent disk cache, or is the
  existing in-memory cache enough for the first pass?
- Should GDrive thumbnail fetches share the main Drive API cooldown state, or
  have a separate lower-priority cooldown?
- For MEGA, should `getPreview()` fallback be enabled for videos, images only,
  or only hover/preview-pane surfaces?
- For Linux MTP, does the target KIO stack expose any thumbnail resource without
  full file copy? If not, Linux MTP thumbnails should remain disabled.
