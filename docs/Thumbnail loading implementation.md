# Thumbnail loading implementation plan

## Goal

Make thumbnails faster and more predictable for local files and plugin-backed
providers without reintroducing unsafe full-file remote downloads in the image
provider. The implementation should be incremental: each phase must be useful on
its own and small enough to review independently.

## Non-goals

- Do not download full remote/provider files just to generate thumbnails.
- Do not make provider SDK or network calls from the GUI thread.
- Do not require every provider to support thumbnails immediately.
- Do not replace the existing QML `Image` rendering path in the first phase.
- Do not persist original provider thumbnail bytes unless the provider already
  owns that cache policy.

## Current constraints to preserve

- QML already pauses thumbnail scheduling/loading while scrolling or resizing.
  Keep this behavior because it protects scroll responsiveness.
- `ThumbnailProvider` is registered as an asynchronous image provider, but each
  `requestImage()` call is still synchronous from the worker's perspective.
- Provider thumbnails must come from `thumbnailForPath()`/`thumbnailUrl()` style
  native preview resources, not generic materialization.
- Permanent provider misses should remain negative-cacheable. Temporary busy or
  transient provider failures should be retryable.
- Thumbnail staging files must be cleanup-managed: register a
  `CleanupSubsystem` lease under the approved staging root and schedule deletion
  when the adapter is no longer needed. The decoded disk cache is intentionally
  persistent and is bounded/evicted separately.

## Implementation status (2026-07-10)

Implemented for provider thumbnails:

- `ThumbnailController` is exposed to QML and owns provider scheduling, cache
  lookup, cancellation, retries, state reporting, and queue metrics.
- Provider HTTP, SDK, and device lanes are bounded independently; MEGA uses the
  SDK lane and portable devices use the device lane.
- `ThumbnailProvider` no longer invokes provider network/SDK thumbnail work or
  generic provider-file materialization. It returns a transparent soft miss
  while the controller has pending/retryable work.
- Google Drive/MEGA/portable use provider-native thumbnail results. Instagram
  now exposes its thumbnail fetch through `thumbnailForPath()`; its auth,
  headers, byte limit, and HTTP policy remain in the provider.
- Decoded images use a versioned disk cache with atomic `QSaveFile` commits,
  a 256 MiB LRU budget, and remote-cache opt-out via
  `FM_THUMBNAIL_DISK_CACHE_REMOTE=0`.
- Grid and brief delegates keep the previous valid thumbnail visible while a
  revision/bucket reload is in flight, avoiding fallback flicker when a hover
  preview requests a larger provider thumbnail. `FM_THUMBNAIL_DEBUG_OVERLAY=1`
  enables a small state overlay.

Still deferred before declaring the full architecture complete:

- split local image versus expensive local decoders into scheduler lanes;
- dedicated unit tests for controller retry/coalescing and disk-cache eviction;
- before/after rollout metrics and manual provider smoke testing.

## Target architecture

Introduce a `ThumbnailController` service with a lower-level
`ThumbnailScheduler` worker component:

```text
QML FilePanel
  ├─ reports visible/near-visible thumbnail interest
  ├─ still renders with Image { source: image://thumbnail/... }
  └─ reacts to thumbnailReady(path) by bumping/reloading revision

ThumbnailController (QObject exposed to QML)
  ├─ requestThumbnail(path, size, priority, reason)
  ├─ cancelThumbnail(path/requestId)
  ├─ warmThumbnails(paths, viewport metadata)
  └─ signals thumbnailReady(path, identity, revision), thumbnailUnavailable(...)

ThumbnailScheduler
  ├─ priority queues and duplicate coalescing
  ├─ per-lane concurrency limits
  ├─ retry/backoff for temporary failures
  ├─ cancellation/staleness checks
  └─ writes completed decoded images to memory/disk cache

ThumbnailProvider
  ├─ returns already completed cached image when possible
  ├─ returns soft placeholder for pending/temporary states
  └─ no longer performs provider network/SDK fetches directly
```

## Data model

### Thumbnail request key

Use a normalized key so scheduler, provider, memory cache, and disk cache agree:

- `path`: normalized lookup path after stripping `::thumbrev=`.
- `bucketSize`: rounded requested size bucket.
- `mode`: normal, cover-only, provider avatar, or future variants.
- `identity`: stable file/provider identity when known.

### Thumbnail identity

Identity should be cheap to compute:

- Local files: absolute/native path + mtime + byte size.
- Providers: provider scheme + provider `thumbnailCacheIdentity(path)`.
- Provider local URL thumbnails: URL path + file mtime + byte size.
- Archives: archive path + archive mtime/size + internal path + optional entry
  metadata.

### Thumbnail states

Represent states explicitly instead of mapping everything to image errors:

- `Ready`: decoded image exists in memory/disk cache.
- `Pending`: job is queued or running.
- `TemporaryUnavailable`: provider/network/SDK is busy; retry is allowed.
- `Unavailable`: permanent no-thumbnail; negative-cacheable.
- `DecodeFailed`: fetched thumbnail bytes/file could not be decoded;
  negative-cacheable for that identity.
- `Cancelled`/`Stale`: request is no longer relevant.

## Phase 0: cleanup and instrumentation foundation

### Tasks

1. Remove the unreachable provider full-file materialization branch from
   `ThumbnailProvider`, or guard it behind an explicit diagnostic-only env flag.
2. Add a code comment/invariant that production provider thumbnails must not call
   `copyToLocalFileForPreview()` from `ThumbnailProvider`.
3. Add structured trace categories/counters:
   - memory cache hit/miss;
   - disk cache hit/miss once disk cache exists;
   - provider busy;
   - temporary unavailable;
   - permanent unavailable;
   - decode failed;
   - queue wait ms;
   - fetch ms;
   - decode ms.
4. Keep existing env flags working:
   - `FM_THUMBNAIL_TIMING`;
   - `FM_PROVIDER_THUMBNAIL_TRACE`.

### Acceptance criteria

- Provider paths cannot reach generic full-file materialization in production.
- Existing provider-native thumbnail behavior remains unchanged.
- Debug logs can explain why a thumbnail did or did not load.

### Suggested tests

- Unit-test/logic-test provider paths to ensure no materialization fallback is
  invoked.
- Manual run with `FM_PROVIDER_THUMBNAIL_TRACE=1` over Google Drive, MEGA, and a
  provider without native thumbnails.

## Phase 1: retryable soft-miss behavior

### Tasks

1. Change provider semaphore-busy and `TemporaryUnavailable` handling from hard
   empty image errors to an explicit retryable state.
2. Return a transparent 1x1 placeholder for retryable states so QML does not mark
   the delegate path as permanently failed for the delegate lifetime.
3. Add a bounded retry/backoff policy:
   - fast retry after scroll settles;
   - exponential backoff for provider/network failures;
   - maximum attempts per path+identity+bucket window.
4. Keep `Kind::None` and decode failures negative-cacheable.
5. Add QML-side logic or controller signal so retryable states can trigger a
   later `thumbnailRevision` bump.

### Acceptance criteria

- A temporarily busy provider can still show the thumbnail later without changing
  directory or manually refreshing.
- Permanent no-thumbnail paths do not retry forever.
- No visible flicker loop from repeated placeholder reloads.

### Suggested tests

- Simulate provider semaphore exhaustion and verify thumbnails retry later.
- Simulate `TemporaryUnavailable` followed by success and verify the image
  appears after a revision update.
- Verify permanent `None` is still negative-cached.

## Phase 2: `ThumbnailController` service shell

### Tasks

1. Add `ThumbnailController` to app services and expose it to QML.
2. Define QML-callable methods:
   - `requestThumbnail(path, width, height, priority, reason)`;
   - `cancelThumbnail(path)` or request-id cancellation;
   - `warmThumbnails(paths, width, height, priority)`;
   - `stateFor(path, width, height)` for debugging/diagnostics.
3. Define signals:
   - `thumbnailReady(path, identity, width, height, revision)`;
   - `thumbnailUnavailable(path, identity, permanent, reason)`;
   - `thumbnailStateChanged(path, state)` for trace/debug builds.
4. Initially back the controller with existing in-memory cache/provider logic so
   behavior changes are minimal.
5. Wire QML visible delegates to call `requestThumbnail()` when they would
   otherwise enable `Image.source`.

### Acceptance criteria

- QML can ask for thumbnails through the controller without losing current
  rendering behavior.
- Existing `image://thumbnail/...` source remains the rendering mechanism.
- Controller emits a ready signal that can bump `thumbnailRevision`.

### Suggested tests

- QML smoke test/manual run: thumbnails still render in local folders.
- Provider smoke test: Google Drive/MEGA thumbnails still render.
- Log/trace confirms controller sees visible thumbnail requests.

## Phase 3: scheduler queues, priorities, and coalescing

### Tasks

1. Implement `ThumbnailScheduler` with bounded queues.
2. Add priorities:
   - `VisibleHigh`: items in the active viewport;
   - `Visible`: visible but less urgent;
   - `NearVisible`: cache buffer / likely next scroll area;
   - `Warm`: opportunistic prefetch;
   - `Background`: low-priority maintenance.
3. Coalesce duplicate requests by request key so repeated QML delegates do not
   start duplicate provider/decoder work.
4. Add cancellation/staleness:
   - cancel requests when delegate is pooled/reused;
   - downgrade requests when the path leaves viewport;
   - drop stale results if identity changed before completion.
5. Add queue metrics.

### Acceptance criteria

- Duplicate visible requests produce one job and multiple waiters/signals.
- Fast scrolling does not leave a long tail of obsolete high-priority jobs.
- Visible jobs are served before warm/background jobs.

### Suggested tests

- Unit-test priority ordering.
- Unit-test duplicate coalescing.
- Manual large-folder scroll trace to verify stale work is reduced.

## Phase 4: separate execution lanes

### Tasks

1. Split scheduler workers into lanes:
   - `LocalImageLane` for cheap image decode;
   - `LocalExpensiveLane` for PDF/video/audio/font/archive extraction;
   - `ProviderHttpLane` for HTTP thumbnail providers like Drive/Instagram;
   - `ProviderSdkLane` for SDK-backed providers like MEGA;
   - `DeviceLane` for WPD/MTP-style device thumbnails.
2. Configure conservative defaults:
   - local image: higher concurrency;
   - local expensive: low concurrency;
   - provider HTTP: moderate concurrency;
   - provider SDK: low per-provider concurrency;
   - device: very low concurrency.
3. Add per-provider overrides by scheme.
4. Ensure lane workers never touch QML objects directly.
5. Keep all completed images flowing through the same cache and ready signal.

### Acceptance criteria

- Slow MEGA/device jobs cannot starve local image thumbnails.
- Expensive local video/PDF jobs cannot starve cheap local images.
- Provider concurrency can be tuned per scheme without QML changes.

### Suggested tests

- Mixed folder manual test: large images + video/PDF/audio.
- Provider mixed test: Drive + MEGA + local folder navigation.
- Instrumentation confirms lane wait/fetch/decode times separately.

## Phase 5: move provider fetches out of `ThumbnailProvider::requestImage()`

### Tasks

1. Change `ThumbnailProvider` provider path behavior:
   - memory/disk cache hit: return image;
   - pending/retryable: return soft placeholder;
   - missing job: optionally enqueue via controller/scheduler and return
     placeholder;
   - permanent unavailable: return soft miss/fallback result.
2. Move `FileProviderPluginRegistry::thumbnailForPath()` calls into scheduler
   provider jobs.
3. Preserve provider-native contract and provider cache identities.
4. Make Instagram special network worker path use the same scheduler flow or move
   it behind provider `thumbnailForPath()`.
5. Ensure provider objects are created/used safely on worker threads, or route
   calls through provider-owned safe APIs.

### Acceptance criteria

- Provider network/SDK calls are not performed inside image-provider request
  handling.
- Provider thumbnails still appear for Drive/MEGA/portable/Instagram where
  supported.
- Temporary provider failures result in scheduled retries, not hard delegate
  failures.

### Suggested tests

- Drive thumbnail cache miss -> scheduler job -> ready signal -> image appears.
- MEGA SDK thumbnail miss -> provider SDK lane -> ready signal -> image appears.
- Provider busy path -> placeholder -> retry -> success.

## Phase 6: decoded disk cache

### Tasks

1. Add a `ThumbnailDiskCache` component.
2. Store decoded images by request key + identity + bucket.
3. Use safe atomic writes:
   - write temp file;
   - fsync/flush where appropriate;
   - rename into final cache path.
4. Add size limits and eviction policy:
   - total byte budget;
   - LRU by access time;
   - optional per-provider budget.
5. Add cache versioning so format changes can invalidate old entries.
6. Add privacy controls if needed for remote/provider thumbnails.

### Acceptance criteria

- Reopening a previously viewed local/provider folder uses disk cache.
- Stale cache entries are invalidated when local mtime/size or provider identity
  changes.
- Cache stays within configured size limits.

### Suggested tests

- Generate local thumbnails, restart app, verify disk cache hits.
- Change local file mtime/size, verify stale thumbnail is not reused.
- Provider identity change invalidates provider thumbnail.
- Cache eviction test with small budget.

## Phase 7: provider-specific refinements

### Google Drive

- Reuse authenticated network/session objects where safe.
- Batch or prefetch visible thumbnail links.
- Cache downloaded encoded bytes by Drive thumbnail identity.
- Treat 401/429/5xx as retryable with backoff.

### MEGA

- Run SDK thumbnail calls only in the MEGA scheduler lane.
- Keep cooldown behavior, but expose it as scheduler backoff state.
- Consider a stricter per-node duplicate coalescing key.
- Avoid multiple simultaneous thumbnail/preview requests for the same node.

### Portable device

- Cache WPD thumbnail/icon bytes by portable identity.
- Keep non-Windows MTP thumbnails disabled unless a cheap native thumbnail
  resource becomes available.
- Keep device lane concurrency low.

### FTP

- Do not use generic full-file materialization.
- Either leave thumbnails disabled/fallback-only or implement a provider-specific
  cheap preview mechanism if one exists.

### Instagram/Telegram-style providers

- Migrate special thumbnail URL/download handling toward `thumbnailForPath()`.
- Keep provider-specific auth, byte limits, and headers inside the provider.
- Let scheduler handle priority, retry, cache, and cancellation.

## Phase 8: QML polish and UX

### Tasks

1. Add subtle loading state only if needed; avoid noisy spinners in dense grids.
2. Keep fallback icons visible until thumbnails are ready.
3. Avoid layout shifts: placeholders should be transparent/fixed-size.
4. Add optional debug overlay/trace mode for thumbnail states.
5. Ensure hover preview/provider avatars use the same cache/scheduler results.

### Acceptance criteria

- No flicker during thumbnail retry.
- No jarring layout movement as thumbnails appear.
- Scrolling remains smooth while thumbnails load.

## Phase 9: measurement and rollout

### Metrics

Track before/after numbers for:

- time to first visible thumbnail;
- time to all visible thumbnails;
- cache hit rate: memory and disk;
- provider fetch count;
- duplicate request coalescing count;
- queue wait by lane;
- decode/fetch time by lane;
- temporary retry count;
- permanent unavailable count;
- UI scroll FPS or frame time during thumbnail load.

### Rollout strategy

1. Land cleanup/instrumentation first.
2. Land retryable soft-miss behavior.
3. Add controller shell behind an env flag if needed.
4. Enable scheduler for providers first because provider thumbnails are the
   riskiest current path.
5. Extend scheduler to local expensive decoders.
6. Add disk cache after request keys and identities are stable.
7. Enable by default after metrics show no regressions.

## Implementation checklist

- [x] Remove or quarantine provider materialization fallback.
- [x] Add thumbnail state model.
- [x] Add retryable placeholder behavior.
- [x] Add `ThumbnailController` service and QML exposure.
- [x] Add `ThumbnailScheduler` queues and priorities.
- [x] Add duplicate coalescing.
- [x] Add cancellation/staleness handling.
- [x] Add execution lanes and per-provider limits for providers.
- [x] Move provider `thumbnailForPath()` calls into scheduler jobs.
- [x] Migrate Instagram special path into provider/scheduler flow.
- [x] Add decoded disk cache.
- [x] Add provider-specific optimizations.
- [x] Add debug overlay/tracing where useful.
- [ ] Add tests for retry, cache identity, coalescing, and provider fallback.
- [ ] Collect before/after performance metrics.

## Risk register

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Scheduler introduces stale thumbnails | Wrong image may appear | Always validate identity before publishing ready signal |
| Retry loop causes churn | CPU/network waste | Bounded attempts, exponential backoff, permanent negative cache |
| Disk cache leaks sensitive remote previews | Privacy concern | Configurable cache, provider-aware opt-out, cache root under app data |
| Provider object thread-safety issues | Crashes/data races | Use provider-safe worker APIs or marshal to provider-owned threads |
| Too many QML revision bumps | Flicker/perf issue | Coalesce ready signals and update only when identity changes |
| Lane defaults too conservative | Slow thumbnails | Add metrics and tunable per-lane env/settings |
| Lane defaults too aggressive | Network/SDK overload | Conservative defaults, per-provider caps, backoff |

## Definition of done

The plan is complete when:

- provider thumbnails no longer perform network/SDK fetches inside
  `ThumbnailProvider::requestImage()`;
- temporary provider busy states retry automatically;
- local expensive decoders cannot starve cheap local thumbnails;
- decoded disk cache speeds up repeat navigation;
- provider paths never use generic full-file materialization in production;
- instrumentation can explain thumbnail latency and failures;
- Google Drive, MEGA, portable devices, Instagram/Telegram-style providers, and
  unsupported providers all have documented, tested behavior.
