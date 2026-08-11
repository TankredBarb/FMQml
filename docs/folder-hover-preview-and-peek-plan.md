# Folder Hover Preview and Folder Peek Plan

## 1. Goal

Extend FM's existing media hover preview with two optional folder-oriented modes:

1. **Folder Hover Preview** — a lightweight, cursor-anchored snapshot of a folder that appears after a short hover delay.
2. **Folder Peek** — an explicitly opened, pinned mini-browser with navigation, Grid/List switching, and bottom breadcrumbs.

Both modes must follow the current theme, support independent transparency and backdrop blur settings, preserve per-panel preferences, and remain safe for large local folders and remote providers.

The concept image is a visual reference, not a requirement to reproduce a mobile file manager inside a hover popup.

## 2. Product boundary

### 2.1 Folder Hover Preview

Hover Preview answers: “What is roughly inside this folder?”

It is intentionally read-only and shallow:

- shows at most 6–9 child entries;
- supports a compact Grid/List presentation preference;
- shows the folder name and a concise bounded item summary;
- provides explicit `Open` and `Peek` actions;
- never navigates inside the card;
- never changes the active panel path or selection;
- disappears on scroll, drag, navigation, context menu, rename, or pointer departure.

It must not enumerate an entire directory merely to calculate an exact count. If the bounded request sees more entries than it can display, use `9+ items` or `More items`, not an invented total.

### 2.2 Folder Peek

Peek answers: “Let me inspect and navigate this folder without replacing my active panel.”

It is an explicit, pinned surface:

- opened from the folder hover card, a context action, or a future dedicated shortcut;
- owns one temporary browsing session at a time;
- supports Grid and List views;
- supports opening child folders inside the Peek session;
- shows clickable breadcrumbs at the bottom;
- supports Back, Up, Open in active panel, and Close;
- keeps the active panel path, selection, history, filters, and scroll position unchanged until `Open in active panel` is chosen;
- stays open while the pointer moves and closes only through Escape, Close, outside click according to the accepted dismissal policy, or invalidation of its source.

Peek is not a second general-purpose `FilePanel`. It does not initially expose rename, delete, copy, paste, drag/drop, context menus, terminal actions, filters, provider actions, selection badges, or the panel action strip.

## 3. Existing implementation to preserve

The current implementation already has useful foundations:

- one `FileHoverPreviewCard` per panel, hosted by `FilePanelOverlayHost`;
- cursor-relative placement stored in `FilePanel.hoverPreviewAnchorRect`;
- delayed activation and suppression during scrolling, dragging, rubber-band selection, rename, menus, and model loading;
- `FilePanelController.hoveredFileInfo`, built from roles already present in the active `DirectoryModel`;
- per-panel persistence for the existing `showHoverPreviews` flag;
- an accepted `TranslucentSurface` shell using FM theme tokens;
- optional hover-preview transparency and global surface blur;
- provider-native thumbnail paths that avoid full-file materialization.

Do not replace these mechanisms with per-delegate popups or a second hover coordinator.

## 4. Settings and persistence

### 4.1 File-panel View menu

Replace the single ambiguous `Show Hover Previews` action with a small `Hover previews` submenu:

- `Media hover previews` — preserves the existing image/video behavior;
- `Folder hover previews` — enables the lightweight folder snapshot;
- `Folder Peek` — exposes the Peek action from folder hover/context surfaces.

All three are independent. Disabling Folder Hover Preview must not disable explicitly invoked Peek. Disabling Folder Peek hides or disables the Peek action but leaves the simple folder hover card available.

These are per-panel visual preferences and must be stored independently for left and right panels:

```text
leftShowMediaHoverPreviews
rightShowMediaHoverPreviews
leftShowFolderHoverPreviews
rightShowFolderHoverPreviews
leftFolderPeekEnabled
rightFolderPeekEnabled
```

Migration rule:

- existing `leftShowHoverPreviews` / `rightShowHoverPreviews` values become the initial media-hover values;
- folder hover and Peek default to off for existing installations until accepted;
- after one compatibility release, obsolete keys may be removed in a separate cleanup, not during initial implementation.

Persistence must update both `WorkspaceStateCoordinator.qml` and `AppSettingsController` import/export/reset paths. A QML-only setting is incomplete.

### 4.2 Transparency settings

Add two explicit surface groups in Transparency Settings:

1. `Hover previews` — applies to both media and folder hover cards.
2. `Folder Peek` — applies to the pinned Peek surface.

Each group contains one on/off switch which opts that surface into the shared
transparency and blur effects. Transparency strength and blur strength remain
global: the two sliders at the top of Surface Effects control every opted-in
surface.

Add only the two persisted participation settings:

```text
hoverPreviewTransparency
folderPeekTransparency
```

Defaults should preserve today's opaque hover behavior. Blur must use the existing bounded backdrop source and never capture the desktop behind the application.

## 5. Visual design

### 5.1 Shared visual language

Both surfaces use:

- `TranslucentSurface` and existing theme tokens;
- the accepted 8 px maximum corner radius for hover surfaces;
- `icons-classic` only;
- the same semantic icon recoloring as other FM controls;
- restrained borders and no heavy shadow stack;
- clear loading, empty, unavailable, and error states;
- stable geometry while asynchronous results arrive.

Shared visual language does not require a universal popup abstraction. Extract a shared folder-entry delegate only after Hover and Peek are both real consumers with matching requirements.

### 5.2 Folder Hover Preview geometry

Initial target:

- width: 260–320 px, clamped to the panel;
- height: stable 260–330 px depending on view mode;
- appears near the pointer after 320–450 ms;
- flips left/up and clamps using the existing cursor-anchor logic;
- never covers the panel footer or escapes the active panel boundary.

Layout:

```text
folder name                         type/count
------------------------------------------------
bounded 3x2 or 3x3 grid / compact list
------------------------------------------------
Open                                      Peek
```

The view selector may be a two-state icon control in the header. It changes only the folder-hover presentation and is persisted per panel.

Breadcrumbs do not belong in the hover card because hover itself does not navigate. A single elided location line is sufficient.

### 5.3 Folder Peek geometry

Peek should be larger and visually pinned:

- default width: 420–560 px;
- default height: 440–620 px;
- clamp to the application window and minimum supported size;
- on narrow windows, occupy most of the available panel/workspace area rather than overflowing;
- remember the last accepted size only after resize behavior exists and is visually accepted.

Layout:

```text
Back  Up    folder title         Grid/List  Open in panel  Close
----------------------------------------------------------------
scrollable child content
----------------------------------------------------------------
root / parent / current-folder                         menu
```

Bottom breadcrumbs follow the concept image and are part of the Peek navigation model. The current folder crumb is active; ancestors are clickable; inaccessible provider ancestors are disabled rather than guessed.

## 6. Interaction contract

### 6.1 Hover behavior

- Folder hover starts only for entries already known to be directories.
- Moving between files restarts the generation and delay.
- Moving from the delegate into the card uses the existing delayed-clear bridge.
- Scrolling, flicking, drag/drop, rubber-band selection, inline rename, context menus, panel navigation, and application overlays suppress it immediately.
- A stale result may populate a cache but must never update a card whose path/generation changed.
- Clicking `Open` uses the existing `FilePanel.openHoverPreviewPath()` route.
- Clicking `Peek` closes the hover card and opens one Peek session for that path.

### 6.2 Peek behavior

- Opening a second path replaces the current Peek session after cancelling its pending load.
- Escape first closes any Peek-owned submenu; a subsequent Escape closes Peek.
- Outside-click dismissal must be tested. If it makes breadcrumb or view-menu use fragile, prefer explicit Close plus Escape.
- Double-clicking a directory navigates inside Peek.
- Double-clicking a file initially invokes the same explicit file-opening policy as the active panel only after that route is reviewed; the conservative first version selects the item and offers `Open`.
- `Open in active panel` navigates the source panel to Peek's current folder and then closes Peek.
- Peek history is session-local and never enters the main panel back/forward history unless `Open in active panel` is chosen.
- View mode is persisted per panel; Peek navigation history and current path are session-only.

### 6.3 Keyboard and accessibility

Required:

- Tab traversal through header controls, content, breadcrumbs, and Close;
- arrow navigation within Grid/List;
- Enter opens a directory within Peek or activates the selected file action;
- Backspace/Alt+Left goes back inside Peek;
- Alt+Up goes up inside Peek;
- Escape follows the dismissal rule above;
- accessible names for view toggle, crumbs, Open, and Close;
- focus returns to the originating panel item when Peek closes.

Do not assign Space to Peek globally: Space already owns Quick Look. A dedicated shortcut may be chosen only after the UI action is accepted.

## 7. Data and ownership architecture

### 7.1 Folder preview snapshot

Add a focused C++ controller, tentatively:

```text
src/controllers/FolderPreviewController.{h,cpp}
```

It owns the lightweight request lifecycle and exposes an immutable snapshot:

```text
requestId
path
state: idle/loading/ready/empty/unavailable/error
entries: up to maxEntries
hasMore
displayedCount
errorText (sanitized)
```

Each entry contains only presentation data required by the card:

```text
name, path, isDirectory, suffix, iconName,
mimeType, isImage, hasThumbnail, thumbnailIdentity
```

The controller must not expose or reuse the active panel's mutable selection model.

### 7.2 Bounded local loading

Local folder hover loading runs on a private bounded executor or the existing suitable metadata lane, never the GUI thread.

Rules:

- stop after `maxEntries + 1` eligible children;
- do not recurse;
- do not calculate recursive size;
- do not read file contents;
- do not pre-generate thumbnails for every child;
- skip hidden entries according to the source panel's current hidden-files policy;
- cooperative cancellation plus request-generation filtering;
- hard cap result memory and string lengths;
- return `hasMore`, not an exact total, when the cap is reached.

Thumbnail requests are issued lazily by visible delegates through the existing thumbnail provider after the snapshot is accepted.

### 7.3 Provider loading

Do not call `FileProvider::childPaths()` from the hover path: the existing contract may enumerate a complete remote folder and is not guaranteed to be cheap or cancellable.

Provider folder hover requires an explicit bounded capability. Add it only after local behavior is accepted, for example an asynchronous request that guarantees:

- maximum entry count;
- cancellation/generation;
- no full-file materialization;
- no recursive calls;
- no secret-bearing logs;
- clear `Unsupported` result.

Until a provider implements that capability, show a metadata-only folder card with `Open` and optional `Peek`, not a spinner that never completes.

Provider rollout should be incremental: Mock first, then one real provider with cheap paged listing, then the remaining providers. Telegram, Instagram, cloud, FTP, and portable devices must be reviewed independently because their latency and pagination semantics differ.

### 7.4 Folder Peek session

Add a separate session controller, tentatively:

```text
src/controllers/FolderPeekController.{h,cpp}
```

It may own one dedicated `DirectoryModel` because Peek is explicit and pinned, but it must configure that model as a preview session:

- one active model per Peek surface, not per item;
- no directory watcher unless a later requirement proves it useful;
- no mutation actions;
- explicit cancellation on close or path replacement;
- independent current path and Back stack;
- bounded or paged provider loading using existing `Load more` semantics;
- no copy of panel selection/history/filter state;
- source panel held through `QPointer` or another lifetime-safe reference.

Reuse `FileProviderFactory` and provider navigation semantics. Do not implement path parsing or breadcrumb construction in QML; reuse `FilePanelController` path helpers or move the provider-neutral helpers to an owning path service if both controllers genuinely need them.

### 7.5 QML ownership

Proposed components:

```text
qml/components/filepanel/FolderHoverPreviewCard.qml
qml/components/filepanel/FolderPeekOverlay.qml
qml/components/filepanel/FolderPreviewEntryDelegate.qml   # only after two consumers exist
```

`FilePanelOverlayHost` owns both surfaces. `FilePanel` remains responsible for cursor anchoring, suppression, and invoking existing panel navigation. Controllers own loading and navigation state; QML owns presentation and user interaction.

## 8. Caching and resource policy

Use a small process-session cache for accepted folder snapshots:

- key: normalized provider/path identity plus hidden-files policy;
- short TTL for local folders;
- provider-specific TTL only when the provider can supply stable identities;
- strict entry-count and total-cache limits;
- invalidate the hovered local parent on known directory watcher events;
- never persist folder contents to settings;
- never cache credentials, signed URLs, cookies, account identifiers, or provider-private transport data.

Peek's live model is not the hover cache. It may seed initial presentation from a matching snapshot, but authoritative navigation comes from its own session load.

## 9. Failure and empty states

Hover states:

- `Loading folder…`
- `Empty folder`
- `Preview unavailable` for unsupported providers;
- `Folder cannot be read` for a sanitized local failure;
- bounded results with `More items` when capped.

Peek states:

- loading skeleton with stable geometry;
- empty folder action/state;
- permission denied with `Open in active panel` only if that route remains valid;
- provider disconnected/session expired;
- source removed while Peek is open;
- retry action only where an actual retry is safe.

Errors must not leak full private provider paths into logs or user-visible diagnostics unless the existing provider UI already considers that path safe.

## 10. UI Lab before runtime integration

Add deterministic scenarios before connecting real loading:

### Folder Hover Preview

- Grid with 6 entries;
- List with long filenames;
- empty;
- loading;
- unavailable provider;
- `9+ items` bounded state;
- pointer-edge placement near all four panel edges;
- opaque/translucent/blurred surfaces;
- light/dark/custom themes.

### Folder Peek

- Grid and List;
- deep bottom breadcrumbs;
- narrow and full viewport;
- loading, empty, error, and paged-provider states;
- keyboard focus map;
- transparency/blur combinations;
- long translated labels and long provider paths.

The UI Lab prototype uses deterministic data only. It must not instantiate a real `DirectoryModel` or scan the user's filesystem.

## 11. Implementation phases

### Phase 0 — UI Lab visual contract

1. Add deterministic Folder Hover and Folder Peek scenarios.
2. Accept geometry, Grid/List density, bottom breadcrumbs, actions, and narrow behavior.
3. Accept opaque, translucent, and blurred variants.

Exit criterion: both modes are visually distinct and useful; Peek is not mistaken for an ephemeral hover card.

Current prototype status:

- `hover-preview/interactive` is available as a dedicated UI Lab page and through Overview coverage;
- the page includes the existing media-hover family for direct visual comparison with folder hover and Peek;
- deterministic Hover and Peek scenes support Grid/List, Ready/Loading/Empty/Unavailable, opaque/translucent, and blur controls;
- the Hover scene includes bounded-result language and Open/Peek actions;
- the Peek scene includes Back/Up, selection, Open in panel, Close, and bottom breadcrumbs;
- filesystem and provider loading are intentionally absent;
- live visual acceptance across Narrow through Full Desktop remains pending.

### Phase 1 — Settings and migration

1. Split the existing media-hover toggle from folder-hover and Peek toggles.
2. Persist all per-panel values and migrate the old hover keys.
3. Add independent transparency strength and blur settings.
4. Extend settings import/export/reset and Debug Information where appropriate.

Exit criterion: every toggle survives restart independently for both panels without clearing unrelated settings.

### Phase 2 — Local Folder Hover Preview

1. Implement bounded local snapshot loading and cancellation.
2. Add the real folder card to the existing overlay host.
3. Add lazy visible-entry thumbnails and safe fallback icons.
4. Wire Open and Peek entry points; Peek may initially open the deterministic shell only.

Exit criterion: large local folders never block the UI, stale hover results never flash, and no request reads more than the configured bound for presentation.

### Phase 3 — Local Folder Peek

1. Implement the single Peek session controller and dedicated model.
2. Add Grid/List, Back/Up, bottom breadcrumbs, and Open in active panel.
3. Add keyboard navigation, focus restore, cancellation, and source invalidation.

Exit criterion: Peek can navigate a realistic local tree without changing the active panel until explicitly requested.

### Phase 4 — Provider capability and rollout

1. Define the bounded provider-preview contract and unsupported fallback.
2. Implement Mock provider tests.
3. Enable one paged real provider and verify cancellation/privacy.
4. Audit and enable remaining providers individually.
5. Connect provider-capable Peek sessions with existing Load more behavior.

Exit criterion: no provider hover performs an unbounded listing or full-file download, and unsupported providers degrade immediately and honestly.

### Phase 5 — Hardening and polish

1. Tune cache limits and TTL using measurements.
2. Verify scrolling, rapid hover, drag, context menus, panel close, plugin unload, and application shutdown.
3. Verify high DPI, minimum window size, split panels, custom themes, and live theme changes.
4. Audit accessible names and keyboard-only operation.
5. Add Debug Information counters for session requests, cancellations, cache hits, failures, and Peek opens without recording paths.

Exit criterion: exact runtime routes pass the testing matrix and diagnostics contain counts only, never sensitive paths.

## 12. Testing matrix

### Hover

- Grid, Brief, and Details panel views;
- left and right panels with different settings;
- local small, empty, unreadable, and 100k-entry folders;
- rapid pointer movement across many folders;
- scroll while delay and loading are active;
- drag, rubber band, rename, context menu, and panel navigation;
- thumbnails on/off and hidden files on/off;
- cached and uncached paths;
- window and panel edges;
- provider supported, unsupported, disconnected, and slow.

### Peek

- open from hover card and context action;
- replace an already open Peek path;
- deep Back/Up/breadcrumb navigation;
- Grid/List persistence per panel;
- narrow/full viewport and split-panel geometry;
- close with Escape and explicit Close;
- focus restore to the originating item;
- source panel removed or changed while Peek remains open;
- local permissions failure;
- provider pagination, cancellation, logout, and plugin unload;
- Open in active panel preserves the expected navigation semantics.

### Visual settings

- opaque;
- translucent without blur;
- translucent with blur;
- blur capability unavailable;
- strength at minimum, default, and maximum;
- live light/dark/custom theme changes while each surface is open.

## 13. Acceptance criteria

The feature is complete when:

1. Media hover, Folder Hover, and Folder Peek can be enabled independently per panel.
2. All settings persist across restart and survive settings import/export.
3. Folder Hover remains shallow, bounded, cancellable, and read-only.
4. Folder Peek has an independent session and never silently mutates the active panel.
5. Grid/List and bottom breadcrumbs work with mouse and keyboard.
6. Transparency, strength, and blur are independently controllable for Hover and Peek.
7. Local 100k-entry folders do not freeze the GUI or cause unbounded memory growth.
8. Provider hover never calls full-file materialization or an unbounded child listing.
9. Stale async results never appear for a newer hover path or Peek generation.
10. Scroll, drag, rename, menus, navigation, shutdown, and plugin unload are safe.
11. Logs and diagnostics contain no sensitive provider paths, URLs, tokens, cookies, or account identifiers.
12. The exact Grid/Brief/Details, left/right, local/provider, and visible/closing routes are manually verified rather than inferred from a green build.

## 14. Explicit non-goals

- embedding the complete `FilePanel` inside a hover card;
- file mutations from Folder Hover or the first Peek version;
- recursive folder sizes or exact full-folder counts on hover;
- preloading all child thumbnails;
- video playback or document rendering inside folder snapshots;
- a new universal popup/dialog abstraction;
- persisting Peek history or cached folder contents;
- enabling every provider before its bounded behavior is proven;
- changing Space from Quick Look without a separate shortcut decision.
