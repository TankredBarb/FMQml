# Telegram Shared Files Provider Plan

Goal: add a `telegram://` provider that makes files from Telegram chats,
channels, and Saved Messages usable from FMQml as a file-manager source. The
provider should target the common "Telegram as a file dump" workflow without
presenting Telegram as a guaranteed unlimited cloud drive.

The recommended implementation path is TDLib-backed. TDLib is the official
Telegram client library, integrates with C++/CMake, handles MTProto, local
state, file downloads, and asynchronous updates, and is a better fit than the
Bot API for user-owned chats and channels.

## Product Positioning

The provider should be described as access to Telegram files, not as generic
cloud storage replacement.

Primary workflows:

- browse files from Saved Messages;
- browse files from selected chats, groups, and channels the user can access;
- copy/download selected files or folders to local storage;
- mirror a selected Telegram source to a local folder as a one-way operation;
- search/filter file-heavy Telegram sources by name, type, date, and chat;
- preview supported media through the existing provider materialization path.

Non-goals for the first implementation:

- no claim of unlimited storage;
- no two-way sync;
- no conflict resolver;
- no automatic delete propagation;
- no background archival daemon;
- no Telegram chat UI or messaging client features;
- no attempt to bypass Telegram client/API limits.

## Current Assumptions

- The provider uses TDLib, not the Bot API.
- The first production milestone is read-only.
- Authentication is user-account based and must support phone/code/2FA.
- Local TDLib database and downloaded files are stored under an app-controlled
  provider data directory.
- TDLib local database encryption uses a user/app-provided key and must not be
  logged.
- File-manager operations remain provider-based: scan, copy/download,
  preview/openRead, optional metadata cache, and cancellation.
- Uploading files back to Telegram is a later phase only if read-only browsing
  proves useful and stable.

## Current Implementation Status

- `FM_ENABLE_TELEGRAM_PLUGIN` exists and defaults to `ON`.
- The CMake target is gated behind `find_package(Td QUIET)` and is not built
  when TDLib is missing.
- TDLib is installed in the local vcpkg tree as `tdlib:x64-linux`, and the
  plugin links through `Td::TdStatic`.
- The initial source layout is split by responsibility under
  `src/plugins/telegram/`.
- The first skeleton contains path parsing, presentation/cache helpers, a
  TDLib-backed client lifecycle, auth actions, and a read-only provider shell.
- Settings has a Telegram login/status surface for API ID/API hash, phone,
  code, 2FA password, source open, sign-out, and local data reset.
- Telegram API credentials are entered by the user during login. They are kept
  in memory during the login flow and are persisted to the system credential
  store only after TDLib reaches `Ready`.
- Saved Telegram API credentials are reused on restart, so environment
  variables are no longer required after a successful login. Environment
  variables remain a developer fallback when no saved credentials exist.
- `telegram://saved` requests the user's self chat through TDLib and maps the
  history pages of document/photo/video/audio/animation/voice-note/video-note
  messages into file entries.
- Text messages with URL entities or link previews are exposed as virtual
  `.url` files that can be opened or copied without downloading media.
- Grid and brief views can show Telegram thumbnails when the global
  `showThumbnails` option is enabled, preferring already-local TDLib thumbnail
  files and falling back to minithumbnail metadata. Generated minithumbnail
  files are placed under cleanup-managed thumbnail staging.
- Saved Messages and chat/channel views expose a synthetic `Load more...` entry
  when TDLib reports additional history.
- Load-more navigation is treated as an in-place append for Instagram and
  Telegram provider views, preserving scroll position across large channel
  pagination.
- `telegram://chats` lists a bounded set of TDLib chats, and
  `telegram://chat/<chat-id>` / `telegram://channel/<chat-id>` scan files with
  the same read-only message filtering used by Saved Messages.
- `telegram://downloads` browses the local TDLib files directory as a read-only
  local cache view.
- Settings can open a Telegram source from a numeric chat id, `@username`, or a
  supported `t.me` link; public usernames are resolved through TDLib before
  scanning.
- Cached `telegram://saved/...` file entries can be downloaded through TDLib
  and copied/opened through the provider `copyToLocalFile()` and `openRead()`
  paths.
- Telegram plugin actions are shown only inside `telegram://` panels.
- `Forget Telegram local data` closes the TDLib client, clears saved API
  credentials, clears in-memory Telegram cache, and removes the provider-owned
  TDLib database/files directory after explicit confirmation in Settings.
- The shared TDLib client is started lazily by Telegram operations and is
  closed after an idle period without logging out, so saved TDLib session data
  can be resumed without asking the user to log in again.
- Settings authorization status does not start an idle TDLib client; it reports
  saved credentials/session intent until a Telegram source or auth action is
  actually used.
- Telegram downloads report progress during the TDLib download stage and during
  the final local copy stage. Cancellation requests are forwarded to TDLib
  without deleting partially cached data.
- Telegram supports provider-level batch materialization, so multi-file copies
  report aggregate progress and stop cleanly on cancel/failure while preserving
  TDLib's resumable cache behavior.
- Telegram thumbnail requests first try to use already-local TDLib thumbnail
  files, then download TDLib thumbnail files on demand for visible UI requests,
  and only fall back to embedded minithumbnails when higher-quality thumbnails
  are unavailable.
- Copying local files into `telegram://saved`, `telegram://chat/<id>`, or
  `telegram://channel/<id>` sends them through TDLib as file/media messages.
  The provider does not send standalone text messages.
- Multi-file local uploads use provider-level batch copy. Consecutive image/video
  files are sent as Telegram albums in groups of 2-8 items when possible;
  other files are sent as individual file/media messages.
- Invite-link resolution and one-way mirror UX are not implemented yet.

## Developer Setup

TDLib is expected from vcpkg for local development:

```sh
vcpkg install tdlib:x64-linux
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$HOME/.local/share/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --target fm_telegram_provider -j2
```

Telegram authorization requires application credentials from Telegram's API
developer portal. Do not commit personal credentials and do not put them in
logs. The normal source-built workflow is:

1. Open Settings -> Telegram -> Log in.
2. Enter API ID/API hash from `https://my.telegram.org`.
3. Enter phone, login code, and optional 2FA password.
4. FMQml saves the API credentials to the system credential store after login
   succeeds, and TDLib keeps the account session under the provider data
   directory.

For developer automation, environment variables are still accepted when no
saved credentials are available:

```sh
export FM_TELEGRAM_API_ID=123456
export FM_TELEGRAM_API_HASH=your_api_hash
```

Optional local TDLib database encryption can be supplied through
`FM_TELEGRAM_DATABASE_KEY`. This value is also secret and must not be logged.

The TDLib client is closed after 120 seconds of Telegram inactivity by default.
For local debugging, `FM_TELEGRAM_IDLE_TIMEOUT_SECONDS` can override this value;
set it to `0` to disable idle closing.

## TDLib Fit

TDLib is suitable for this provider because it provides:

- user authorization and session resume;
- access to chats, channels, messages, and message content metadata;
- documented asynchronous request/response and update flow;
- file download management and local file paths;
- local database/cache managed by the library;
- C++ and C-compatible JSON interfaces;
- CMake integration through `find_package(Td ...)` or vendored build.

Recommended interface: start with the C++ API if it links cleanly in the
current build. Fall back to the JSON interface only if packaging or ABI issues
make the C++ API difficult.

Build and packaging risks:

- TDLib is a large dependency compared to the existing provider plugins.
- Linux packaging may need distro TDLib packages or a clear external dependency
  path.
- Windows packaging likely needs explicit runtime copy rules.
- CI/build configuration should gate the plugin behind `FM_ENABLE_TELEGRAM_PLUGIN`
  and build it only when TDLib is found.

## Provider Path Model

Initial path scheme:

- `telegram:///` - provider root;
- `telegram://saved` - Saved Messages file view;
- `telegram://chats` - browse pinned/recent/selected chats;
- `telegram://chat/<chat-id>` - files from a specific chat;
- `telegram://channel/<chat-id>` - alias/presentation path for channels;
- `telegram://downloads` - optional view of locally downloaded Telegram files.

Path rules:

- internal paths should use stable Telegram ids, not display names;
- user-visible names can come from chat titles and message/file names;
- private titles, phone numbers, invite links, and usernames must not be printed
  in normal logs;
- exported/copied local filenames should be deterministic and safe for the file
  system;
- files without names should get stable synthesized names from message id,
  media type, and extension.

## Metadata Model

Each Telegram file entry should map into `FileEntry` with:

- `name`;
- `path`;
- `size`;
- `modified` or message date;
- `isDirectory = false`;
- MIME/suffix from TDLib metadata or `QMimeDatabase`;
- provider capability text such as `Telegram`, `Downloaded`, `Remote`, or
  `Read-only`;
- stable provider metadata for chat id, message id, file id, local path, and
  download state.

Virtual folders should represent:

- provider root sections;
- chats/channels;
- optional date buckets for very large sources;
- optional media-type buckets such as Documents, Photos, Videos, Audio.

The first version should avoid deep virtual folder design unless a flat source
becomes unusable. A practical default is a flat file list with sorting and
filters.

## Architecture

Suggested files:

- `src/plugins/telegram/TelegramFileProviderPlugin.h/.cpp`;
- `src/plugins/telegram/TelegramClient.h/.cpp`;
- `src/plugins/telegram/TelegramAuth.h/.cpp`;
- `src/plugins/telegram/TelegramPath.h/.cpp`;
- `src/plugins/telegram/TelegramCache.h/.cpp`;
- `src/plugins/telegram/TelegramTypes.h`;
- `src/plugins/telegram/TelegramPresentation.h/.cpp`;

Responsibilities:

- `TelegramFileProviderPlugin`: implements the `FileProvider` interface and
  translates file-manager calls into Telegram operations.
- `TelegramClient`: owns TDLib client lifecycle, request ids, update dispatch,
  download requests, and cancellation.
- `TelegramAuth`: handles authorization state, phone/code/password prompts, and
  session logout.
- `TelegramPath`: normalizes and parses `telegram://` paths.
- `TelegramCache`: stores scanned entries, chat metadata, parent/children
  relationships, and download state.
- `TelegramPresentation`: maps Telegram media types into names, icons,
  capability labels, and synthesized filenames.

The provider should follow the smaller split-file style now used by the
Instagram provider instead of growing one monolithic plugin source.

## Phase 0: Dependency Probe

Priority: highest.

Goal: prove that TDLib can be found, linked, and called from the FMQml build
without committing to provider behavior.

Tasks:

1. Add `FM_ENABLE_TELEGRAM_PLUGIN` CMake option.
2. Add `find_package(Td QUIET)` detection.
3. Build a tiny gated plugin target only when TDLib is available.
4. Verify whether `Td::TdStatic`, `Td::TdJson`, or both are available in the
   target environment.
5. Document the expected install dependency for local development.

Acceptance criteria:

- builds without TDLib continue to work and print a clear disabled-provider
  status;
- builds with TDLib can link a minimal provider target;
- no Telegram UI or runtime behavior is exposed yet;
- CMake changes are isolated to the gated provider target.

## Phase 1: TDLib Client Skeleton

Priority: after dependency probe.

Goal: create a small internal TDLib adapter that can start, receive
authorization state updates, send requests, and shut down cleanly.

Tasks:

1. Create `TelegramClient` with a single TDLib client instance.
2. Implement request id mapping and response dispatch.
3. Implement update handling for authorization state and file download updates.
4. Store TDLib database/files under an app-specific Telegram provider
   directory.
5. Add a provider-owned encryption key path; do not log the key.
6. Ensure shutdown cancels outstanding waits and destroys TDLib state cleanly.

Acceptance criteria:

- a development-only action can initialize and close the TDLib client;
- authorization state transitions are visible as sanitized status values;
- app exit does not hang with an active TDLib client;
- logs never include phone numbers, auth codes, TDLib database keys, chat
  titles, or raw update JSON.

## Phase 2: First-Class Authorization

Priority: required before useful browsing.

Goal: let a user sign in, resume a session, and sign out through provider
actions/settings consistent with existing remote providers.

Status: implemented for the read-only provider.

Completed:

1. Add Telegram provider actions: sign in, sign out, status.
2. Add UI prompts for API ID/API hash, phone number, login code, and 2FA
   password.
3. Persist session/database state in the TDLib directory.
4. Store user-provided Telegram API credentials through the existing
   credential-store path after successful login.
5. Remove saved API credentials on sign-out.
6. Add an explicit local data reset that closes TDLib and removes provider-owned
   TDLib database/files.

Remaining:

1. Add a small help link/copy text for creating API credentials on
   `my.telegram.org`.

Acceptance criteria:

- user can sign in without environment variables;
- user can restart FMQml and remain signed in;
- user can sign out/forget session;
- failed login states surface as global provider messages without leaking
  secrets;
- no auth material appears in trace logs, warning logs, URLs, or file paths.

## Phase 3: Read-Only Saved Messages MVP

Priority: first user-visible feature.

Goal: expose files from Saved Messages as a read-only provider folder.

Status: implemented, with follow-up polish pending.

Completed:

1. Add `telegram:///` root with a `Saved Messages` entry.
2. Implement `telegram://saved` scanning by reading message history.
3. Filter to messages with supported file/media content, including documents,
   photos, videos, audio, animations, voice notes, video notes, and URL-only
   messages.
4. Convert TDLib file metadata into `FileEntry`.
5. Add lazy pagination through a synthetic `Load more...` entry or an internal
   continuation mechanism matching existing provider behavior.
6. Implement `copyToLocalFile()` by asking TDLib to download the file and then
   copying from TDLib's local path to the requested destination.
7. Implement `openRead()` through TDLib local paths or virtual buffers for
   URL/text entries.
8. Preserve scroll position when using `Load more...`.

Remaining:

1. Add focused fake-client tests for pagination and stale generation handling.
2. Add clearer progress/cancellation behavior for very large download batches.

Acceptance criteria:

- `telegram://saved` lists document/photo/video/audio files from Saved
  Messages;
- large histories load incrementally and do not freeze the panel;
- downloading a file to local storage works with progress;
- preview/openRead works through existing materialization paths;
- cancellation does not leave unmanaged temporary files;
- scanning failures show sanitized errors.

## Phase 4: Chats and Channels

Priority: after Saved Messages MVP.

Goal: expose selected chats/channels as read-only file sources.

Status: implemented for direct open/list flows, source-management polish
pending.

Completed:

1. Add `telegram://chats` root.
2. List a bounded set of TDLib chats.
3. Add an action to add/open a chat by username/link/id where TDLib supports it.
4. Implement `telegram://chat/<chat-id>` scanning with the same file filtering
   and pagination model as Saved Messages.
5. Add presentation labels for private chat, group, supergroup, and channel
   without leaking sensitive titles into logs.

Remaining:

1. Add source history/favorites for frequently opened chats/channels.
2. Decide whether `telegram://chats` should filter to file-heavy chats or stay
   as a bounded TDLib chat list.
3. Add invite-link resolution only if it becomes necessary; it is intentionally
   out of scope for now.

Acceptance criteria:

- user can browse files from at least one non-Saved-Messages chat/channel they
  can access;
- inaccessible/deleted/private sources fail with clear sanitized messages;
- large channels remain responsive through pagination;
- switching away from a scanning chat cancels or ignores stale work.

## Phase 5: Local Mirror / One-Way Sync

Priority: after stable read-only browsing.

Goal: support the real "sync files to computer" workflow as an explicit
one-way mirror from Telegram to a local folder.

Scope:

- source: one Telegram folder/view;
- destination: one local folder;
- direction: Telegram to local only;
- conflict policy: initially skip existing same-size files or rename on
  conflict, matching existing FMQml copy policy where possible;
- deletion propagation: out of scope for first mirror version.

Tasks:

1. Reuse existing operation queue copy paths where possible.
2. Add a provider action such as `Mirror to local folder` only if current UI
   patterns support it cleanly.
3. Persist optional mirror presets only after one-off copy works.
4. Add progress summaries for large channel mirrors.
5. Add resume-friendly behavior by checking existing local files before
   downloading.

Acceptance criteria:

- user can copy/mirror many Telegram files to a local folder without manually
  opening each file;
- interrupted mirrors can be restarted without redownloading every completed
  file;
- no remote deletes are performed;
- mirror state does not store chat titles or links in plaintext unless the user
  explicitly chose that local path/name.

## Phase 6: Thumbnails and Media Polish

Priority: after browsing and downloads are stable.

Goal: make Telegram media feel native in grid/brief/preview views.

Status: partially implemented.

Completed:

1. Use TDLib thumbnails/minithumbnails for photo/video/document entries when
   available.
2. Avoid downloading full video files for thumbnails.
3. Put generated minithumbnail files under cleanup-managed thumbnail staging.
4. Map Telegram media types to existing file-type icons.
5. Add readable provider capability text for remote/downloaded states.

Remaining:

1. Add negative thumbnail cache entries for failed/expired thumbnails.
2. Consider selective higher-quality thumbnail download for visible items only,
   without mass media downloads.
3. Add more media-specific presentation for unnamed Telegram videos and
   generated filenames.

Acceptance criteria:

- grid view shows thumbnails for common Telegram photos and videos;
- videos use poster/thumbnail data, not full video downloads;
- thumbnail failures fall back to icons;
- thumbnail work does not block navigation.

## Phase 7: Optional Uploads

Priority: later, only after read-only value is proven.

Goal: consider copying local files into Saved Messages or a selected channel.

This phase should be explicitly re-approved before implementation. Uploads
change the provider from a read-only file source into a Telegram content writer,
which raises new product and privacy concerns.

Potential scope:

- upload local files to Saved Messages;
- upload local files to a user-selected channel where the user has permission;
- no folder semantics beyond batch file upload;
- no rename/move/delete illusion for remote Telegram messages.

Acceptance criteria:

- upload permission errors are clear;
- upload progress and cancellation are reliable;
- uploaded message/file mapping is cached immediately;
- user-facing UI makes it clear that Telegram receives messages/files, not
  normal filesystem objects.

## Phase 8: Visual Normalization

Priority: after upload basics are manually verified.

Goal: make `telegram://` folders read as Telegram-native locations in the file
manager while keeping the product focused on files, not chat UI.

Status: static provider-folder visuals and lazy visible chat avatar badges are
implemented.

Assumptions:

- Static provider folders should use bundled SVG assets, matching the existing
  GDrive/MEGA/Instagram approach.
- Dynamic chat badges should use TDLib chat photo metadata only when it is
  already available or requested for a visible chat/source.
- Chat avatars are visual metadata, not navigation requirements. Missing or
  failed avatar loads must fall back to a Telegram-style folder badge.
- No chat message text, phone numbers, invite links, or private usernames should
  be exposed through visual metadata.

Planned scope:

1. Add Telegram-style bundled assets under `qml/assets/filetypes-next/`:
   provider root, Saved Messages, Chats, Downloads, Load more, and a generic
   chat/channel badge.
2. Map Telegram root folders to explicit `iconName` values in the provider:
   `telegram://saved`, `telegram://chats`, `telegram://downloads`, and the
   synthetic load-more entry.
3. Extend `FilePanelIconPolicy.qml` so Telegram folders get native-folder
   overlays in grid/brief views, following the existing GDrive/Instagram badge
   pattern.
4. Extend breadcrumbs so `telegram://saved`, `telegram://chats`,
   `telegram://chat/<id>`, `telegram://channel/<id>`, and
   `telegram://downloads` show Telegram-specific icons instead of plain folder
   icons.
5. Add a small presentation field for chat visual identity, separate from file
   thumbnails, so chat/channel folders can display an avatar badge when TDLib
   provides a local or cheaply downloadable small chat photo.
6. Keep avatar loading lazy and bounded: request only the current chat/source
   or visible chat entries, avoid scanning full history, and cache negative
   results for the session.
7. Make privacy behavior explicit: private chats may show a local avatar badge
   if TDLib exposes it, but logs and docs must not include private titles or
   avatar paths.

Completed:

1. Added bundled Telegram SVG assets for the provider root, Saved Messages,
   Chats, Downloads, generic chat/channel badges, Downloads badge, and Load
   more badge.
2. Mapped Telegram root folders, chat entries, and load-more entries to explicit
   Telegram `iconName` values.
3. Extended grid/brief folder overlay policy for Telegram provider paths.
4. Extended breadcrumbs so Telegram paths use Telegram-specific icons and
   branded icon coloring.
5. Cached TDLib `chat.photo.small` metadata for chat entries and exposed it
   through the existing provider thumbnail adapter.
6. Added lazy avatar badges for visible `telegram://chat/<id>` and
   `telegram://channel/<id>` folders in grid and brief/icon-cell based views,
   falling back to the static Telegram chat/channel badge on missing photos or
   thumbnail errors.

Implementation order:

1. Static assets and icon names for Telegram system folders.
   Verify: root, Saved Messages, Chats, Downloads, and Load more render with
   Telegram-specific visuals in grid and brief views.
2. Breadcrumb mapping for Telegram paths.
   Verify: breadcrumb icons match provider folders and still navigate to the
   same normalized paths.
3. Chat/channel folder badge plumbing without avatar downloads.
   Verify: chat/channel folders fall back to a generic Telegram chat/channel
   badge. Done.
4. Optional TDLib chat photo badge.
   Verify: visible chats with an already-local or cheaply downloadable small
   photo show it; missing photos and download failures fall back cleanly. Done.

Out of scope:

- full contact cards or chat profile UI;
- showing online status, unread counters, or message previews;
- eager avatar downloads for every chat;
- changing upload/download semantics.

Acceptance criteria:

- Telegram locations are visually distinct from normal folders in grid, brief,
  and breadcrumbs;
- static Telegram folder badges work without starting TDLib unnecessarily;
- chat avatar badges never block navigation or folder scanning;
- all failures fall back to stable bundled icons;
- visual metadata follows the Telegram privacy/logging policy.

## Privacy and Logging Policy

Never log:

- phone numbers;
- login codes;
- 2FA passwords;
- TDLib database encryption keys;
- raw TDLib updates;
- raw message text;
- private chat titles by default;
- invite links;
- usernames from private sources;
- local TDLib database paths if they include account identifiers.

Allowed logs:

- provider state names such as `authorizationStateWaitCode`;
- numeric counts;
- sanitized error categories;
- request type labels;
- file sizes and MIME categories when not tied to private names.

Trace logging must be behind a Telegram-specific environment flag and still
follow the same redaction rules.

## Temporary Files and Local Data

Temporary files must follow existing cleanup subsystem rules:

- preview/openRead staging goes through `CleanupSubsystem`;
- generated Telegram minithumbnail files go through cleanup-managed thumbnail
  staging and old unmanaged thumbnail cache directories are scheduled for
  cleanup;
- failed downloads do not leave unmanaged `.part` files;
- provider-to-provider transfers use existing operation staging;
- TDLib persistent database/files live in a provider-owned data directory, not
  arbitrary temp locations;
- a user-facing forget-session action must be able to remove provider-owned
  Telegram local state.

## Testing Strategy

Unit tests:

- path normalization and parsing;
- synthesized filename generation;
- media type to presentation/icon mapping;
- cache parent/children behavior;
- redaction helpers.

Provider tests with a fake Telegram client:

- Saved Messages scan with pagination;
- chat/channel scan with pagination;
- file download success/failure/cancellation;
- stale generation handling;
- auth state transitions;
- sanitized error propagation.

Manual integration tests:

- sign in with phone/code/2FA;
- restart and resume session;
- sign out/forget session;
- browse Saved Messages with many files;
- browse a large channel;
- copy a folder-sized selection to local storage;
- preview photos, videos, and documents;
- verify logs with Telegram trace enabled.

## Regression Boundaries

- Do not change local file-provider behavior.
- Do not change Instagram, Google Drive, MEGA, FTP, MTP, or archive providers
  while adding Telegram.
- Keep Telegram-specific UI and provider behavior guarded by `telegram://`.
- Do not introduce global file-model fields unless an existing provider-neutral
  extension point is insufficient.
- Do not add background sync by default.
- Do not make Telegram account data visible in Places until auth and privacy
  behavior are stable.

## Suggested Implementation Order

1. Add upload polish after manual testing: byte-level upload progress when TDLib
   exposes enough state, plus broader media metadata mapping.

Each phase should build and pass relevant tests before the next phase starts.
