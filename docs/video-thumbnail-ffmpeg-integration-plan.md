# FFmpeg Video Thumbnail Integration Plan

Goal: add video thumbnails and video preview stills based on FFmpeg, gated by an
optional CMake feature. The first target is thumbnail/still extraction for local
video files without blocking navigation, selection, or preview scheduling.

This plan follows:

- `suggest/12-view-reuse-and-navigation.md`: preview and thumbnail work must be
  asynchronous and generation-safe.
- `suggest/15-linux-port-roadmap.md`: thumbnails should move toward
  freedesktop thumbnail cache behavior where practical.
- `suggest/19-staging-and-large-temporary-io.md`: provider/archive materialized
  preview data needs bounded staging and cleanup.

## Current State

- QML file delegates request thumbnails through `image://thumbnail/<path>`.
- `src/core/ThumbnailProvider` already handles SVG, FB2 cover art, fonts, PDF
  with `Qt6::Pdf`, audio cover art with TagLib, image files through
  `QImageReader`, and Windows Shell thumbnails as a Windows fallback.
- Linux video files currently fall through to `QImageReader` and usually produce
  no thumbnail.
- `VideoPreview.qml` already attempts to show `ImagePreview` for videos. Once
  `image://thumbnail` can return a video frame, the preview pane and QuickLook
  still image path should start working without a large QML rewrite.
- Remote/provider thumbnail materialization already exists in
  `ThumbnailProvider`, capped by `kProviderThumbnailMaterializeLimit`, but this
  must remain conservative for video.

## Scope

In scope for the first implementation:

- Optional FFmpeg/libav-based video frame extraction for local files.
- Integration into the existing `ThumbnailProvider` pipeline.
- CMake option and compile definitions so builds without FFmpeg keep working.
- A small C++ extraction helper with clear ownership and no UI concepts.
- Timing/debug output compatible with `FM_THUMBNAIL_TIMING`.
- Tests for feature gating and suffix/MIME routing where practical.
- Manual QA plan for local videos, preview pane, QuickLook, and large folders.

Out of scope for the first implementation:

- Full video playback.
- Timeline scrubbing.
- Animated thumbnails.
- Audio waveform extraction.
- Large remote video materialization by default.
- Aggressive freedesktop cache writes before the extractor is stable.

## Architecture

Add a core helper:

```text
src/core/VideoThumbnailExtractor.h
src/core/VideoThumbnailExtractor.cpp
```

The helper should expose a narrow API:

```cpp
struct VideoThumbnailRequest {
    QString path;
    QSize targetSize;
    qint64 preferredTimestampMs = -1;
};

struct VideoThumbnailResult {
    QImage image;
    qint64 timestampMs = -1;
    QString error;
};

class VideoThumbnailExtractor {
public:
    static bool isAvailable();
    static VideoThumbnailResult extract(const VideoThumbnailRequest &request);
};
```

Keep FFmpeg headers and FFmpeg-specific error handling inside this helper. Do
not leak FFmpeg types into `ThumbnailProvider`, controllers, or QML.

Preferred backend is FFmpeg libraries:

- `libavformat`
- `libavcodec`
- `libavutil`
- `libswscale`

Do not start with an `ffmpeg`/`ffprobe` subprocess path. A process fallback can
be considered later, but the library path gives tighter timeout/cancellation
control and avoids shell quoting/path issues.

## CMake Plan

Add an option:

```cmake
option(FM_ENABLE_FFMPEG_THUMBNAILS "Enable FFmpeg video thumbnail extraction" ON)
```

Detection order:

1. Prefer `pkg-config` on Linux:
   - `libavformat`
   - `libavcodec`
   - `libavutil`
   - `libswscale`
2. Accept imported targets if a package manager provides them later.
3. If the option is `ON` but FFmpeg is missing, do not fail the whole build by
   default. Print a status/warning and build without the feature.
4. Add a stricter future option only if needed:
   `FM_REQUIRE_FFMPEG_THUMBNAILS`.

Compile definition:

```cmake
target_compile_definitions(fm PRIVATE HAS_FFMPEG_THUMBNAILS)
```

Only define it when all required libraries are available.

Add sources only when enabled:

```cmake
if(HAS_FFMPEG_THUMBNAILS)
    target_sources(fm PRIVATE
        src/core/VideoThumbnailExtractor.cpp
        src/core/VideoThumbnailExtractor.h
    )
endif()
```

Link the FFmpeg libraries privately to `fm`.

## Extraction Behavior

Frame choice:

- Use a timestamp near 10% of duration.
- Clamp to a practical range:
  - minimum: 500 ms
  - preferred: 10% duration
  - maximum first-pass seek: 30 seconds
- If duration is unknown, try 1 second.
- Avoid frame 0 as the default because many videos start black.

Decode flow:

1. Open input with `avformat_open_input`.
2. Read stream info with `avformat_find_stream_info`.
3. Select the best video stream with `av_find_best_stream`.
4. Open decoder for that stream.
5. Seek to the selected timestamp.
6. Decode packets until the first usable video frame after seek.
7. Convert to `QImage::Format_ARGB32` or `QImage::Format_RGB32` through
   `sws_scale`.
8. Scale to the requested bucket size using `Qt::KeepAspectRatio` and
   `Qt::SmoothTransformation`.

Bounds and safety:

- Cap source dimensions accepted for decode if FFmpeg reports absurd metadata.
- Refuse images with zero/negative dimensions.
- Limit packets/frames attempted after seek so damaged files cannot spin.
- Return an empty image with a short error string on failure.
- Never throw exceptions across Qt code.

Threading:

- `ThumbnailProvider` is already created with
  `ForceAsynchronousImageLoading`. Keep extraction inside `requestImage`.
- Do not call FFmpeg from the GUI thread.
- Do not add QML timers or JS decode logic.

Cancellation:

- QQuick image providers do not provide a simple per-request cancellation token.
  Therefore each extraction must be bounded by packet/frame limits and avoid
  unbounded retries.
- Async stale results are naturally discarded by QML if the image source has
  changed, but CPU work still happens; keep it short.

## ThumbnailProvider Integration

Add helpers near existing suffix checks:

```cpp
bool isVideoSuffix(const QString &suffix);
bool shouldAttemptVideoThumbnail(const QString &suffix, const QString &mimeName);
```

In `ThumbnailProvider::requestImage`:

1. Keep all existing early exits and cache checks.
2. After PDF/audio-cover handling and before generic `QImageReader`, call
   `VideoThumbnailExtractor::extract()` when:
   - `HAS_FFMPEG_THUMBNAILS` is defined;
   - the file is not `coverOnly`;
   - suffix/MIME is video-like;
   - `path` is local or has already been safely materialized.
3. If extraction succeeds, set:
   - `stage = "ffmpeg-video"`
   - `stageMs` from the stage timer
   - cache result using the existing `m_cache`.
4. If extraction fails, continue to existing fallbacks:
   - Windows Shell on Windows
   - final negative cache otherwise

Do not make video extraction the first branch. PDF/audio/image code should keep
its current behavior.

## Cache Strategy

Initial implementation:

- Reuse the existing in-memory `QCache<QString, QImage>`.
- Reuse the existing negative cache.
- Include the bucket size in the cache key, as it already does.

Later freedesktop cache phase:

- Read from `~/.cache/thumbnails/normal` and `large` before decoding.
- Write generated thumbnails only after validating the format and URI keying.
- Store PNG thumbnails with the required freedesktop metadata where practical.
- Do not block first implementation on freedesktop cache writes.

Cache invalidation:

- The current cache key does not include mtime or file size. For video, that can
  show stale thumbnails if a file is replaced in place.
- Plan a follow-up cache-key enhancement:
  - local files: include canonical path, size, mtime, requested bucket;
  - provider materialized files: include provider path and provider metadata if
    available.

## Provider And Archive Policy

MVP:

- Enable FFmpeg thumbnails for local filesystem paths only.
- Allow provider paths only when the existing `ThumbnailProvider`
  materialization path has already produced a bounded local temp file.
- Keep the existing `kProviderThumbnailMaterializeLimit` conservative.

Do not increase provider materialization limits just because video thumbnails
exist.

Remote videos:

- Most remote videos should not be downloaded just to create a thumbnail.
- Future provider APIs can expose provider-native thumbnails or partial-range
  reads. That should be a separate design.

Archive entries:

- Do not extract arbitrary video entries from archives for thumbnails in the
  first phase.
- Archive containers should keep the existing skip behavior unless a later
  archive-thumbnail policy is explicitly designed.

Cleanup:

- Reuse `CleanupSubsystem` artifact registration for materialized temp files.
- FFmpeg extractor itself should not create temp files.

## UI Behavior

No broad QML rewrite is required for MVP.

Expected visible changes:

- File-panel delegates that already request `image://thumbnail` should show
  still frames for supported local videos.
- `VideoPreview.qml` should show the still frame through `ImagePreview` instead
  of the current unavailable placeholder.
- If extraction fails, the existing video placeholder remains.

Settings:

- Existing `appSettings.showThumbnails` continues to control file-panel
  thumbnail requests.
- No new user-facing setting is required for MVP.
- A future advanced setting may disable video thumbnails separately if CPU cost
  becomes an issue.

## Video Playback Plan

The thumbnail work should be treated as the first step toward real video
preview playback. Video playback should follow the existing audio preview
pattern instead of creating a separate media stack.

Current audio reference:

- `AudioPreview.qml` presents metadata/cover art and loads playback controls
  only when details are shown and multimedia controls are available.
- `AudioPlaybackControls.qml` uses `QtMultimedia`, `MediaPlayer`, and
  `AudioOutput`.
- Media is not loaded until the user presses Play.
- `releaseMedia()` stops playback and clears `player.source` when the preview
  path changes or the control is destroyed.
- Controls expose play/pause, seek, elapsed/duration labels, mute, and volume.

Video playback should reuse those principles:

1. Keep playback optional behind the existing QtMultimedia feature gate.
   - Use the current `FM_ENABLE_MULTIMEDIA_PREVIEW` /
     `FM_MULTIMEDIA_PREVIEW_AVAILABLE` path.
   - Do not make FFmpeg thumbnails imply QtMultimedia playback.
   - Disabled QtMultimedia builds should still show FFmpeg still thumbnails.

2. Replace `VideoPreview.qml` placeholder-only behavior with a two-layer video
   preview:
   - still thumbnail layer from `image://thumbnail`;
   - playback layer loaded lazily after user action.

3. Add a dedicated QML component:

   ```text
   qml/components/preview/VideoPlaybackControls.qml
   ```

   It can either reuse shared pieces extracted from
   `AudioPlaybackControls.qml`, or start as a sibling component with matching
   behavior. The better long-term shape is to extract common controls into a
   small shared media controls component after the first video version works.

4. Use `QtMultimedia` objects:
   - `MediaPlayer`
   - `AudioOutput`
   - `VideoOutput`

   `VideoOutput` should fill the preview surface with
   `fillMode: VideoOutput.PreserveAspectFit`.

5. Preserve audio behavior:
   - no autoplay;
   - first Play sets `player.source`;
   - pause/stop/release on path/source changes;
   - release on component destruction;
   - no playback for stale preview targets.

6. Keep source URL plumbing centralized.
   - `QuickLookController` already exposes materialized/local media source
     behavior for audio/video through `content`/media source plumbing.
   - Video should use the same `mediaSourceUrl` strategy as audio.
   - Provider videos should play only after explicit, bounded preview
     materialization exists; do not stream arbitrary provider URLs directly in
     the first pass.

7. UI layout proposal:
   - Primary area: video still thumbnail.
   - Center overlay: play button when stopped and thumbnail is ready.
   - On Play: show `VideoOutput` above the thumbnail.
   - Bottom overlay or reserved strip: play/pause, progress rail, elapsed,
     duration, mute, volume.
   - Keep controls compact in preview pane mode and larger in QuickLook if that
     mode later needs separate sizing.

8. Failure behavior:
   - If QtMultimedia backend cannot play the file, keep the FFmpeg still
     thumbnail visible.
   - Show a compact "Playback unavailable" state rather than replacing the
     whole preview with an error wall.
   - Log detailed player errors only in debug output, matching audio controls.

9. Lifecycle and focus:
   - Stop playback when preview path changes.
   - Stop playback when preview pane closes or QuickLook closes.
   - Do not keep audio playing from an old video after selection moves.
   - Space/Enter shortcut behavior must respect overlay/input routing and
     should not steal file-panel shortcuts until deliberately designed.

10. Later polish:
    - fullscreen QuickLook video mode;
    - keyboard shortcuts while QuickLook owns focus;
    - persisted volume shared with audio controls;
    - optional loop;
    - provider-native streaming only if provider APIs can support cancellation
      and clear errors.

Playback acceptance checks:

- QtMultimedia enabled: local MP4/WebM/MKV opens with a still thumbnail and
  starts playback only after Play.
- QtMultimedia disabled: still thumbnails work and playback controls are not
  shown.
- Selecting another file stops video and clears `MediaPlayer.source`.
- Preview pane close stops video and audio.
- Corrupt or unsupported video keeps the still/placeholder and reports a small
  unavailable state.
- Existing audio playback still works.

## Diagnostics

Extend `FM_THUMBNAIL_TIMING` output with:

- `stage=ffmpeg-video`
- selected timestamp
- source video stream dimensions
- output image dimensions
- failure reason for null results, only when timing/debug is enabled

Avoid noisy logs by default.

## Tests

Unit tests:

- Add a compile-gated test target only when FFmpeg is enabled:
  `video_thumbnail_extractor_test`.
- Use a tiny generated or checked-in fixture if acceptable.
- If no fixture is committed, test only routing/helper behavior and keep decode
  tests as manual/CI artifact driven.

Recommended fixture strategy:

- Generate a 1-2 second color test video during the test with FFmpeg only if an
  `ffmpeg` executable is available. This keeps the repo free of binary video
  assets.
- Skip the decode test when the executable is unavailable, but still compile the
  library integration.

Manual QA:

- Linux with FFmpeg enabled:
  - open a folder with MP4, MKV, WEBM, MOV, AVI;
  - verify grid/list thumbnails appear without blocking selection;
  - open preview pane and click through several videos quickly;
  - verify stale thumbnails do not replace the currently selected item;
  - try a corrupt video and confirm placeholder/fallback appears;
  - try a huge video and confirm UI remains responsive.
- Linux without FFmpeg:
  - build succeeds;
  - videos keep existing placeholder behavior;
  - no missing-symbol or QML errors.
- Windows:
  - build behavior is unchanged when FFmpeg is disabled;
  - existing Shell thumbnail fallback remains.

Performance checks:

- With `FM_THUMBNAIL_TIMING=1`, inspect stage timings in a folder with many
  videos.
- Selection highlight must appear immediately with preview pane open.
- Large folder scrolling must not regress.

## Implementation Phases

### Phase 1: Build Gate And Skeleton

- Add CMake option and FFmpeg detection.
- Add empty `VideoThumbnailExtractor` behind `HAS_FFMPEG_THUMBNAILS`.
- Wire source files only when available.
- Verify enabled and disabled builds.

### Phase 2: Local Decode MVP

- Implement local video frame extraction.
- Add suffix/MIME routing in `ThumbnailProvider`.
- Keep extraction bounded.
- Add timing logs.
- Verify local MP4/MKV/WEBM manually.

### Phase 3: Preview Pane Polish

- Confirm `VideoPreview.qml` displays generated thumbnails.
- Adjust placeholder copy only if necessary.
- Ensure Image error/loading states remain clear.
- Verify QuickLook and preview pane selection changes.

### Phase 4: Video Playback MVP

- Add `VideoPlaybackControls.qml` or a shared media controls extraction.
- Wire `VideoPreview.qml` to show a lazy `VideoOutput` layer.
- Reuse audio-style media loading/release behavior.
- Keep playback gated by QtMultimedia availability.
- Verify play/pause/seek/mute/volume and path-change release.

### Phase 5: Tests And Diagnostics

- Add compile-gated extractor tests.
- Add optional generated-video decode test.
- Add failure-path checks for invalid files.
- Document manual QA commands.

### Phase 6: Cache And Staleness Improvements

- Improve thumbnail cache keys with mtime/size.
- Consider reading freedesktop thumbnail cache.
- Consider writing freedesktop cache after confirming URI/key behavior.

### Phase 7: Provider And Remote Follow-Up

- Decide whether provider-native thumbnails are exposed through provider APIs.
- Keep large remote video downloads disabled by default.
- If provider materialization is enabled for small videos, keep strict size caps
  and cleanup leases.

## Acceptance Criteria

MVP is complete when:

- `FM_ENABLE_FFMPEG_THUMBNAILS=ON` with FFmpeg installed produces video
  thumbnails on Linux.
- `FM_ENABLE_FFMPEG_THUMBNAILS=ON` without FFmpeg installed still builds with a
  clear CMake message and feature disabled.
- `FM_ENABLE_FFMPEG_THUMBNAILS=OFF` builds with no FFmpeg dependency.
- Existing image/PDF/audio-cover thumbnails still work.
- Existing Windows Shell thumbnail fallback is not regressed.
- Video thumbnail generation does not block navigation or selection painting.
- Corrupt/unsupported videos fail quietly to the existing placeholder.
- `FM_THUMBNAIL_TIMING=1` identifies FFmpeg thumbnail work clearly.

## Open Questions

- Should the first CMake option default to `ON` or `OFF` for release builds?
  The plan assumes `ON` with graceful auto-disable when FFmpeg is missing.
- Do we want to support distro FFmpeg only through pkg-config, or add vcpkg
  FFmpeg target support immediately?
- Should freedesktop cache read support land before write support?
- Should video thumbnail CPU work get a global concurrency limit if many videos
  enter view at once?
