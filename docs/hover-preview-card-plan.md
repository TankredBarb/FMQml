# Hover Preview Card Plan

## Goal

Add a safe, polished hover preview card for image and video files. The card
should feel rich in grid/brief views without turning hover into a heavy remote
download or a second Quick Look path.

## Principles

- Hover preview is lightweight. It may use existing thumbnails and cheap
  metadata, but it must not materialize full remote files.
- Quick Look remains the explicit heavy preview/open action.
- Remote providers must not log private paths, chat ids, filenames, tokens, or
  credentials while hover preview is resolving.
- Results must be guarded by generation/path checks so stale async work cannot
  update the visible card.
- The feature must be optional and live next to the existing per-panel visual
  options such as selection badges and action strip.

## Visual Direction

- One floating overlay per panel/workspace, not one card per delegate.
- Compact card with max 8px radius, restrained shadow, and theme colors.
- Top media area:
  - image: existing thumbnail/preview source;
  - video: poster thumbnail with a small play glyph;
  - fallback: file icon and concise unavailable state.
- Bottom metadata area:
  - filename;
  - size/type/dimensions or duration where available;
  - small actions such as Quick Look and Open when already supported.
- Position near hovered item and clamp within the window. Flip left/up when
  there is not enough space.

## Safety Rules

- Debounce before showing, initially 250-400 ms.
- Hide immediately during scroll, flick, drag, rubber band selection, context
  menus, navigation, and path changes.
- Do not request hover preview when thumbnails are disabled. A later pass may
  show metadata-only cards, but media hover should stay disabled.
- Local files may use `image://thumbnail`.
- Provider files may use provider thumbnail URLs only through the existing
  thumbnail provider path. They must not call provider `copyToLocalFile` for
  hover.
- Video playback on hover is out of scope for the first implementation. Start
  with poster thumbnails only.

## Implementation Phases

1. Foundation
   - Add `showHoverPreviews` to `FilePanel`.
   - Add the toggle in the file panel view menu near selection badges and
     action strip.
   - Persist the value through the existing workspace visual-state snapshot.
   - Add a shared QML overlay skeleton and connect it to hovered file state.

2. Images
   - Render existing `image://thumbnail/<path>` for eligible image files.
   - Show name, type, size, and image dimensions if already available cheaply.
   - Keep all async updates generation-guarded.

3. Videos
   - Use existing thumbnail/poster generation.
   - Show duration and dimensions only when metadata is already cheap or cached.
   - Do not start playback yet.

4. Polish
   - Add clamped/flipped positioning.
   - Add loading and unavailable states.
   - Tune spacing, type sizes, and icon-only actions.

5. Hardening
   - Test large local folders.
   - Test Telegram folders with many photos/videos.
   - Test scroll while hover preview is pending.
   - Test provider logout/idle close during hover.
   - Verify logs contain no private provider paths.

## Initial Success Criteria

- The option can be enabled/disabled without restarting.
- Hovering an eligible image in grid/brief/table shows one floating card after a
  short delay.
- Moving hover quickly does not show stale cards.
- Scrolling or dragging hides the card.
- Telegram/provider hover does not trigger full-file downloads.
- Build succeeds.
