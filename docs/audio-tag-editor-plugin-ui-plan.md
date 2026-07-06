# Audio Tag Editor Plugin UI Plan

## Goal

Add an experimental plugin capability where a plugin can contribute both:

- a context-menu action for a selected batch of audio files;
- the UI shown after that action is triggered.

The first useful plugin will be an audio tag editor. When installed, selecting
audio files in a file panel should expose an action such as `Edit audio tags`.
The plugin-provided dialog should let the user review each selected file, edit
common tags, choose cover art, then either cancel or apply the changes.

This document is a planning/research step only. It does not implement the
feature.

## Current Architecture Findings

Existing plugin support is split across three C++ interfaces:

- `FileProviderPlugin` for virtual filesystem providers;
- `FileActionPlugin` for context-menu actions;
- `PlacesProviderPlugin` for sidebar places.

The relevant path for this feature already exists:

1. `FilePanelContextMenu.qml` builds a `FileActionContext` from the current
   panel, target item, opposite-panel destination, and selected paths.
2. `PluginActionController::actionsForContext()` asks
   `FileProviderPluginRegistry` for all action descriptors.
3. `FileProviderPluginRegistry::actionsForContext()` qualifies each action id as
   `<pluginId>::<actionId>` and sorts by `order`.
4. Triggering an action calls `PluginActionController::triggerAction()`.
5. `WorkspaceOverlays.qml` currently shows the returned `QVariantMap` through
   `PluginActionResultDialog.qml`.

The gap: `FileActionPlugin::triggerAction()` is synchronous and can only return
a result map. There is no first-class result type for "open a plugin UI", no
plugin QML loading contract, and no host-side object that plugin UI can call to
read/write audio tags.

Audio metadata support already exists for reading:

- `MetadataExtractor::extractAudio()` reads Title, Artist, Album, Year, Track,
  Genre, Comment, Duration, Bitrate, Sample Rate, and Channels through TagLib.
- `QuickLookController` and `ThumbnailProvider` already extract audio cover art
  for preview/thumbnail paths.

The gap: there is no reusable write API for audio tags or cover art. Preview
code owns read/extract helpers locally, so a write-capable editor should not
reuse private QuickLook implementation directly.

## Assumptions

- First milestone targets local filesystem paths only. Provider paths,
  archives, managed ISO mounts, and remote materialized previews are out of
  scope until the local editor is stable.
- First milestone edits existing audio files in place.
- TagLib remains the audio metadata backend.
- The plugin may provide QML, but file mutation should happen through a C++
  bridge owned by the plugin or by the host, not by ad hoc JavaScript file IO.
- The host remains responsible for dialog containment, modality, sizing, theme
  context, and refresh after apply.
- The editor action is offered only when every selected item is a supported
  local audio file.

## Non-Goals For The First Pass

- No provider-to-provider or remote metadata editing.
- No automatic online metadata lookup.
- No automatic lyrics or album-art download.
- No embedded lyrics, composer, disc number, BPM, replay gain, or custom frame
  editing in the initial writer.
- No undo after apply beyond relying on the user's backups/VCS/filesystem.
- No mass rename based on tags.
- No attempt to normalize all container-specific tag differences in the UI.
- No sandbox/security model for untrusted third-party QML beyond the existing
  native plugin trust boundary.

## Recommended Contract

Use the existing `FileActionPlugin` menu discovery path, but extend the action
result protocol to support a UI request.

Candidate trigger result:

```cpp
return {
    {"ok", true},
    {"resultType", "pluginUi"},
    {"title", "Edit Audio Tags"},
    {"pluginId", "fm.audio-tags"},
    {"componentUrl", "qrc:/qt/qml/FM/AudioTags/AudioTagEditorDialog.qml"},
    {"context", QVariantMap{
        {"selectedPaths", context.selectedPaths},
        {"currentPath", context.currentPath}
    }}
};
```

Host behavior:

- if `resultType != "pluginUi"`, keep the current result dialog behavior;
- if `resultType == "pluginUi"`, open a new `PluginUiDialog` host shell;
- load the plugin component with `QQmlComponent`;
- inject a controlled context object, for example `pluginUiHost`, and the
  result `context` map;
- close without mutation on cancel;
- after successful apply, refresh the active panel and preview metadata.

This keeps the existing menu plumbing small and avoids inventing a second menu
registry before it is needed.

## Plugin QML Loading Options

### Option A: QRC-backed QML inside the plugin

The plugin compiles its QML into its native library resources and returns a
`qrc:/...` component URL.

Pros:

- simple deployment for a built-in experimental plugin;
- no extra plugin asset directory scanning;
- works with the existing dynamic library loading model.

Cons:

- QML resource paths must be unique across plugins;
- hot-reloading plugin UI during development is awkward;
- third-party plugin packaging conventions remain undefined.

Recommendation for milestone 1: use this option.

### Option B: Plugin asset directory with QML files

The registry records a plugin root directory and permits component URLs under
that root.

Pros:

- easier third-party packaging;
- easier QML iteration during development;
- assets such as placeholder cover images can live beside QML.

Cons:

- needs canonical path validation;
- needs install/copy rules;
- needs a policy for imports and relative asset paths.

Recommendation: defer until the QRC path proves the UI bridge.

### Option C: Host-owned generic schema UI

The plugin returns a form schema and the host renders the editor.

Pros:

- safer and more theme-consistent;
- no arbitrary QML loading.

Cons:

- does not satisfy "plugin provides its UI" in the direct sense;
- likely becomes a custom UI framework before the audio editor is useful.

Recommendation: do not use for this experiment.

## Host-Side Objects

### `PluginActionController`

Add small result routing support, not business logic:

- preserve `triggerAction()` for compatibility;
- optionally add `triggerActionRequest()` later only if async/open flows need a
  stronger type;
- do not parse audio metadata here.

### `WorkspaceOverlays.qml`

Change `openPluginActionResult(result)` into a dispatcher:

- `pluginUi` -> create/open `PluginUiDialog`;
- default -> existing `PluginActionResultDialog`.

### `PluginUiDialog.qml`

New host shell component:

- modal dialog using existing dialog styling;
- title/subtitle/icon come from the trigger result;
- content is a loaded plugin `Item`;
- fixed minimum usable size, responsive maximum size;
- Cancel and Apply buttons can be host-owned or plugin-owned.

For this feature, prefer plugin-owned content and host-owned dialog chrome. The
plugin gets `cancelRequested()` and `applyRequested()` signals from the host or
exposes `canApply`, `busy`, and `apply()` on its root item.

## Audio Tag Editing Backend

Add a small reusable C++ backend for read/write operations. It can live in the
audio tag plugin for the experiment; if it becomes useful outside the plugin,
move it later to `src/core`.

Candidate types:

```cpp
struct AudioTagData {
    QString path;
    QString title;
    QString artist;
    QString album;
    QString albumArtist;
    QString year;
    QString track;
    QString genre;
    QString comment;
    QString coverSource;
    bool hasEmbeddedCover = false;
    QString error;
};

struct AudioTagPatch {
    QString path;
    QVariantMap fields;
    QString coverImagePath;
    bool removeCover = false;
};
```

Required operations exposed to QML:

- `load(paths)` returns per-file tag data and read errors;
- `chooseCover(path)` can be host-owned through a normal `FileDialog`;
- `apply(patches)` writes all requested changes and returns per-file results;
- `supportedAudioPath(path)` filters by suffix/container.

Supported suffixes for milestone 1:

- MP3;
- FLAC;
- M4A/M4B/MP4 audio;
- OGG/OGA.

WAV can be left unsupported initially because tag support is inconsistent.

## Batch Editing UX

The dialog should be useful for a real batch, not just a single-file form.

Minimum layout:

- left list/table of selected files with status badges;
- right editor for the current file;
- a compact "Apply to all selected" control per shared field;
- cover art preview with buttons to choose, clear, and apply cover to all;
- footer state showing pending changes and write errors.

Core fields:

- Title;
- Artist;
- Album;
- Album Artist;
- Year;
- Track;
- Genre;
- Comment;
- Lyrics, after the base writer is stable;
- Cover.

Batch behavior:

- load tags for every selected file when the dialog opens;
- show mixed values as blank/indeterminate in batch controls;
- editing a field on one file affects only that file by default;
- explicit "apply to all" copies that value to all selected files;
- Apply writes only changed fields;
- Cancel discards all pending changes;
- after Apply, keep the dialog open if any file failed and mark failures
  inline.

## Menu Availability

The plugin action should be returned only when:

- `context.scope == "item"`;
- `context.selectedPaths` has at least one path;
- every selected path is local, not a provider URL;
- every selected item is a file, not a directory;
- every selected suffix is in the supported audio set;
- the current path is not a read-only container such as `archive://`.

Current `FileActionContext` contains `selectedPaths` but not per-selection
directory flags or MIME types. For milestone 1, the plugin can cheaply check
local `QFileInfo` and suffixes. If this becomes expensive or inaccurate, add
selection metadata to `FileActionContext` later.

## Mutation And Refresh

After successful apply:

- plugin result should include `refreshCurrentPath: true`;
- host refreshes the active panel, matching current custom action behavior;
- QuickLook/preview should be re-requested for the active path if it points to
  an edited file;
- thumbnail cache for `path::cover` should be invalidated or bypassed.

Thumbnail invalidation is a likely hidden bug. Existing cover art thumbnails
are generated through `image://thumbnail/<path>::cover`; if the cache key only
uses path and not mtime, changed covers may appear stale. The implementation
phase must inspect `ThumbnailProvider` cache behavior before declaring cover
editing complete.

## Failure Handling

Expected per-file failures:

- file disappeared;
- file is read-only;
- unsupported container;
- TagLib read or save failed;
- cover image cannot be decoded;
- cover image is too large or unsupported;
- partial batch success.

Do not fail the whole batch at first error. Apply should return:

- total count;
- changed count;
- failed count;
- list of `{path, ok, message}`.

The UI should keep failed rows visible and allow the user to retry after fixing
permissions or removing failed files from the pending set.

## Security And Trust Boundary

Native plugins are already trusted code because they are loaded into the host
process. Plugin QML does not materially worsen that trust boundary for installed
native plugins, but the host should still avoid giving QML unnecessary global
objects.

Rules for milestone 1:

- plugin UI receives only its component properties and a narrow backend object;
- do not expose `workspaceController`, file panel controllers, or arbitrary app
  services to plugin QML;
- validate component URL scheme before loading;
- for QRC milestone, require component URLs to start with the plugin-declared
  QML prefix.

## Build And Packaging Plan

Add a new experimental plugin target:

- `fm_audio_tag_editor_plugin`;
- source directory: `src/plugins/audio_tags/`;
- implements `FileActionPlugin`;
- links `Qt6::Core`, `Qt6::Gui` if image handling is needed, `Qt6::Qml` if it
  registers QML types, and TagLib;
- builds only when TagLib is available;
- outputs to the existing provider plugin directory for now.

The current plugin output directory is named `plugins/providers`, but it already
stores action-only capable plugins. Renaming that directory is not required for
the experiment. A later cleanup can introduce `plugins/actions` or
`plugins/extensions` if plugin categories start to diverge.

## Implementation Phases

### Phase 1: Result Routing And Host Dialog Shell

Success criteria:

- a mock action can return `resultType: "pluginUi"`;
- the host opens a modal `PluginUiDialog`;
- existing plain plugin action results still open `PluginActionResultDialog`;
- invalid/missing component URLs produce a normal error dialog.

Verification:

- manual trigger from the mock plugin;
- QML smoke test if the project already has a suitable harness;
- build `fm`.

### Phase 2: Audio Tag Plugin Skeleton

Success criteria:

- `fm_audio_tag_editor_plugin` builds when TagLib is available;
- plugin manager lists it as an Actions plugin;
- selecting supported local audio files shows `Edit audio tags`;
- mixed non-audio selections do not show the action.

Verification:

- unit test action availability with synthetic paths where practical;
- manual context-menu check in local folders.

### Phase 3: Read-Only Plugin UI

Success criteria:

- plugin UI opens with selected paths;
- per-file tag data loads through the backend;
- cover preview displays existing embedded cover when available;
- read failures are shown inline without closing the dialog.

Verification:

- test files with MP3, FLAC, M4A, and OGG;
- files with and without embedded cover art;
- read-only and missing-file cases.

### Phase 4: Tag Writes Without Cover Writes

Success criteria:

- editing core text fields writes changed tags only;
- Apply supports partial success;
- Cancel makes no changes;
- panel refreshes after successful writes;
- preview reflects edited text metadata after refresh.

Verification:

- focused TagLib backend tests using temporary audio fixtures;
- manual edit and re-open with another tag reader if fixtures are available.

### Phase 5: Cover Art Writes

Success criteria:

- choose image file and embed it into supported formats;
- clear cover for supported formats;
- apply one cover to all selected files;
- thumbnail/preview cover updates without app restart.

Verification:

- per-format cover write tests where TagLib supports it;
- inspect cache invalidation for `path::cover`;
- manual preview after apply.

### Phase 6: Polish And Guardrails

Success criteria:

- busy state prevents duplicate apply;
- dialog cannot close accidentally while writes are in progress;
- error summaries are concise;
- unsupported files stay out of the action rather than failing late;
- settings/plugin manager still works with the new plugin loaded/unloaded.

Verification:

- repeated open/apply/cancel cycles;
- unload plugin after using action;
- run existing plugin/provider tests.

### Phase 7: Lyrics And Album Art Lookup

Success criteria:

- the plugin can search for lyrics using existing tags and filename fallback;
- the plugin can search for album art using Artist, Album, Album Artist, and
  embedded metadata;
- lookup results are previewed before writing;
- the user can choose one result or keep the existing embedded data;
- writes remain explicit and batch-safe.

Recommended UX:

- add `Fetch lyrics` and `Fetch cover` actions in the editor, not in the file
  panel context menu;
- show source, confidence, dimensions for art, and a short lyrics preview;
- never overwrite existing lyrics or cover art without an explicit selection;
- allow applying fetched cover art to all selected files from the same album;
- keep network lookup optional and cancellable.

Backend notes:

- model lyrics as a separate text field because many formats store unsynced
  lyrics differently from comments;
- model cover lookup as candidate images with URL/source/size/thumbnail and
  only download the selected full image before writing;
- cache lookup responses and selected images under the plugin data/cache
  directory, not beside the audio files;
- keep provider modules isolated so sources can be added or removed without
  changing the tag writer.

Open source/provider questions:

- which lyrics provider is acceptable for licensing and API stability;
- which cover provider should be first, for example MusicBrainz/Cover Art
  Archive if its metadata match quality is good enough;
- whether the app should ship API keys, require user keys, or support only
  keyless sources;
- how to present "no reliable match" without encouraging blind overwrites.

## Open Questions

- Should the first plugin UI component be a full dialog root or content item
  inside host chrome? Recommendation: content item inside host chrome.
- Should Apply be host-owned or plugin-owned? Recommendation: host-owned chrome
  with plugin root methods/properties, so keyboard handling and button placement
  stay consistent.
- Should the audio tag backend live in the host immediately? Recommendation:
  keep it in the plugin first; move it only when another feature needs it.
- Should provider paths eventually be supported by materializing then uploading
  back? Recommendation: no until the local editor is reliable; provider write
  semantics vary too much.
- Do we need a new plugin directory name? Recommendation: no for the experiment.
- Should online lookup live in the same audio tag plugin or a separate metadata
  lookup plugin? Recommendation: same plugin while experimental, with source
  providers split internally.

## Risks

- TagLib write support differs by container, especially cover art.
- Existing thumbnail cache may hide changed cover art.
- Plugin QML loading can grow into a broader extension API if not kept narrow.
- Synchronous action trigger is acceptable for opening UI, but metadata loading
  and writes must be asynchronous or clearly busy-gated to avoid UI stalls.
- Batch editing can become destructive if "apply to all" state is ambiguous;
  mixed-value UI must be explicit.
- Lyrics and album-art lookup introduce API, licensing, rate-limit, privacy, and
  false-match risks.

## Minimal First Milestone Definition

The smallest valuable milestone is:

1. action-only plugin appears for local MP3/FLAC/M4A/OGG selections;
2. action opens plugin-provided QML inside host dialog chrome;
3. dialog reads and displays tags for all selected files;
4. user can edit Title, Artist, Album, Year, Track, Genre, Comment;
5. Apply writes changed text tags and refreshes the current panel;
6. cover editing is visible in the UI but can remain disabled until Phase 5.

This proves the plugin-provided UI architecture without taking on the highest
risk part, which is cross-format embedded cover writing and cache invalidation.
