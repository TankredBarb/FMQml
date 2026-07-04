# Instagram Provider Roadmap

This document describes the next implementation phases for the experimental
Instagram provider. The provider is intentionally best-effort: Instagram's
private web/API behavior can change, but the file manager must remain stable,
avoid unbounded downloads, and keep provider-specific behavior scoped to
`instagram://` paths.

## Current Baseline

- Profile links are represented as `instagram://user/<username>`.
- Post and reel links are represented as `instagram://post/<shortcode>` and
  `instagram://reel/<shortcode>`.
- Profile browsing loads media entries into one flat folder.
- Carousel media from profiles is flattened into the same folder.
- Profile pagination is exposed through a synthetic `__load_more__` entry and
  a context menu action.
- `Load More` must preserve the current scroll position.
- Preview, QuickLook, and download use the provider's normal remote
  materialization paths.
- Temporary preview files must stay under the existing cleanup subsystem roots
  and leases.

## Phase 1: Instagram Thumbnails

Priority: highest.

Goal: grid view and brief view should show thumbnails for Instagram media when
this can be done with acceptable performance and without downloading full media.
The behavior should match local-provider expectations: thumbnails load lazily,
do not block navigation, are cached, and failures degrade to regular file icons.

### Policy

- Thumbnails are allowed only for `instagram://` media entries.
- Thumbnail generation must not download full video files.
- Thumbnail generation should avoid downloading full-resolution photos when a
  smaller preview URL is available.
- Thumbnail downloads should be bounded by response size and timeout.
- Thumbnail failures should not surface as user-facing errors during browsing.
- Thumbnail data should be kept in memory through the existing thumbnail cache.
  Do not create persistent files for Instagram thumbnails unless a later design
  explicitly routes them through the cleanup subsystem.
- Do not enable the generic provider materialization path for Instagram
  thumbnails. That path can materialize full remote files and is not acceptable
  for profile grids.

### Data Model

Extend the Instagram internal media model, not the global `FileEntry` contract:

- Add `thumbnailUrl` to `InstagramMediaItem`.
- Keep `url` as the full media URL used for preview/download.
- `entryFromMedia()` should set `hasThumbnail = !item.thumbnailUrl.isEmpty()`.
- `Load more...` and directory entries must keep `hasThumbnail = false`.

This keeps the global file model stable and confines Instagram-specific preview
URLs to the provider implementation.

### Thumbnail URL Selection

When parsing Instagram JSON:

- For images, prefer the smallest image candidate that is still usable for a
  grid thumbnail.
- Candidate sources include `image_versions2.candidates`, `thumbnail_src`,
  `thumbnail_url`, and similar fields already present in fetched metadata.
- For videos, use the poster/thumbnail image from metadata. Do not use
  `video_url` for thumbnails.
- If no lightweight thumbnail URL exists for a video, leave `thumbnailUrl`
  empty and show the normal video icon.
- For photo fallback, `display_url` may be used only when no smaller preview is
  exposed. This is less ideal but still acceptable for photos if bounded by the
  thumbnail download limit.

For HTML/post parsing:

- Preserve the existing extraction of image and video URLs for full media.
- Capture a separate poster/thumbnail URL when available.
- For carousel posts, each child media item should carry its own thumbnail URL
  if metadata exposes one.

### Provider Lookup

Add a small provider-side lookup for thumbnails:

- Input: normalized `instagram://.../<item>` path.
- Output: thumbnail URL for an already cached `InstagramMediaItem`.
- The lookup must not fetch or paginate profiles by itself.
- If the item is not in cache, return empty and let the UI show the icon.

This prevents the thumbnail provider from issuing profile/post network fetches
for every visible delegate.

### Thumbnail Provider Integration

Add an early Instagram branch in `ThumbnailProvider::requestImage()` before the
generic provider materialization logic:

- Decode the `image://thumbnail/<encoded path>` request.
- If path starts with `instagram://`, ask the Instagram provider/cache for a
  thumbnail URL.
- Download the thumbnail bytes with the same browser-like headers used by the
  provider.
- Enforce a small maximum response size, initially 1-2 MB.
- Decode from memory using `QBuffer` + `QImageReader`.
- Apply EXIF/metadata orientation if supported by the reader.
- Scale to the existing thumbnail cache bucket size.
- Store the result in the existing in-memory thumbnail cache.
- On any failure, insert a negative cache entry and return an empty image.

The implementation must not call `copyToLocalFile()` for Instagram thumbnails.

### Performance Requirements

- Thumbnail requests should be lazy and driven by existing grid/brief delegate
  visibility rules.
- Scrolling should remain responsive. If many Instagram thumbnails appear at
  once, existing thumbnail scheduling throttles must still apply.
- A failed thumbnail URL should be negative-cached so it does not loop on every
  repaint.
- Cache keys should include the Instagram media path and requested bucket size.
  If thumbnail URLs are volatile, the URL or a stable timestamp can be included
  in the cache key to avoid stale results after refresh.

### Acceptance Criteria

- Grid view shows Instagram thumbnails for photos when metadata exposes preview
  URLs.
- Brief view shows the same thumbnails under the same lazy-loading rules.
- Videos show poster thumbnails when metadata exposes poster images.
- Videos never download full mp4/webm content just to create a thumbnail.
- `Load more` keeps working and added media can receive thumbnails.
- If CDN URLs expire, browsing remains stable and falls back to icons.
- `cmake --build build --parallel` succeeds.
- `ctest --test-dir build --output-on-failure` succeeds.

## Phase 2: Direct Post and Carousel Links

Priority: after thumbnails.

Goal: opening a direct Instagram post/reel link should show only media from that
specific post. If the post is a carousel, the folder should contain only the
photos/videos in that carousel, not profile media and not related content.

### Supported Inputs

- `https://www.instagram.com/p/<shortcode>/`
- `https://www.instagram.com/reel/<shortcode>/`
- Existing normalized paths:
  - `instagram://post/<shortcode>`
  - `instagram://reel/<shortcode>`

### Behavior

- A post/reel root opens as a virtual folder.
- A single-media post contains exactly one media file.
- A carousel post contains one entry per carousel item.
- Carousel children should keep deterministic names and ordering.
- Only the current post's media should appear.
- No synthetic `Load more...` item should appear for post/reel roots.
- Profile pagination state must not affect direct post roots.

### Parser Requirements

- Prefer structured JSON embedded in the page or authenticated responses when
  available.
- Preserve current fallback parsing for public pages.
- Extract both full media URL and thumbnail URL per item.
- For carousel children, do not collapse everything into the parent thumbnail.
- Deduplicate by media URL or media id, but preserve carousel order.

### Acceptance Criteria

- Opening a normal post displays only that post's media.
- Opening a carousel post displays all carousel items and nothing else.
- Opening a reel displays the reel video and poster thumbnail if available.
- Direct post folders support preview, QuickLook, and download through existing
  provider paths.
- Thumbnails from Phase 1 work for direct post entries.

## Phase 3: First-Class Login

Priority: after direct post/carousel behavior.

Goal: replace the current environment-variable cookie workflow with a proper
login/session configuration similar in spirit to existing remote providers such
as Google Drive and Mega.

### Policy

- The provider should work without login where public endpoints still allow it.
- Login should unlock more reliable profile pagination and private/session-bound
  content where the account is allowed to see it.
- Session storage must not leak cookies into logs.
- Trace logs may show whether auth markers are present, but not the cookie
  values.
- Existing `FM_INSTAGRAM_COOKIE_FILE` can remain as a developer escape hatch
  until the first-class login path is stable.

### UI/Configuration Direction

The final shape should match other provider account flows where possible:

- A provider/account setup entry for Instagram.
- A way to import or paste a session cookie bundle if full OAuth-style login is
  not available.
- Clear status for logged-in vs anonymous mode.
- A way to forget/remove the session.
- Provider code should read credentials from the shared settings/secret path
  used by other remote providers, not from ad hoc files.

### Session Handling

- Normalize cookies into a single internal session representation.
- Require at least the markers needed by Instagram web requests, such as
  `sessionid`, `ds_user_id`, and `csrftoken`, when using authenticated mode.
- Attach CSRF and browser-like headers consistently.
- Keep anonymous and authenticated requests separated enough that one failing
  path can fall back to the other when appropriate.
- Rate-limit or back off on login-required and request-failed envelopes.

### Acceptance Criteria

- User can configure Instagram auth without setting environment variables.
- Profile pagination works through the configured session when Instagram allows
  it.
- Anonymous public profile/post browsing still works when no session is present.
- Logs never print cookie values.
- Removing the session returns the provider to anonymous mode.
- Existing Google/Mega style account expectations are followed closely enough
  that the provider does not feel special-cased in the app UI.

## Regression Boundaries

- Do not change local file thumbnail behavior for this work.
- Do not change Mega, Google Drive, FTP, MTP, or archive provider behavior.
- Keep Instagram-specific UI rules guarded by `instagram://` checks.
- Do not widen synthetic `__load_more__` handling beyond Instagram profile
  folders.
- Do not write thumbnail temp files outside cleanup-managed locations.
- Do not add background network fetches that continue after provider cancel or
  navigation generation changes.

## Suggested Implementation Order

1. Add `thumbnailUrl` to Instagram media items and populate it from current
   parsers.
2. Mark Instagram media entries as thumbnail-capable only when `thumbnailUrl`
   exists.
3. Add a provider/cache lookup for thumbnail URL by media path.
4. Add the Instagram branch in `ThumbnailProvider`.
5. Verify grid/brief thumbnail behavior and performance.
6. Improve direct post/reel parsing so carousel links become exact virtual
   folders.
7. Design and implement first-class session configuration.

Each phase should be built and tested before moving to the next one.
