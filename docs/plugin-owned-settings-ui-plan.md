# Plugin-owned settings UI plan

## Status

The visual settings-UI separation is complete. The settings capability and
dynamic host loader are implemented, and all four provider cards are embedded
in their respective plugins. Descriptor validation, deterministic ordering,
and broken-component containment have automated coverage.

Provider-specific authorization workflows and dialogs still use the existing
host bridge. Moving those workflows is intentionally outside this visual
change so saved authorization data and behavior remain untouched.

Plugin load/unload only changes which UI is instantiated. It must not clear or
migrate saved settings, credentials, sessions, tokens, or provider caches.

This document records the investigation and implementation plan for removing
provider-specific settings UI from the FM host application. The first
implementation slice should use the MEGA provider as the reference plugin.

## Problem statement

Optional provider plugins are dynamically discovered and loaded, but the
Settings UI currently assumes that several specific plugins always exist.

`SettingsDialog.qml` owns provider-specific state and behavior for:

- Google Drive;
- MEGA;
- Instagram;
- Telegram.

It stores authorization flags and status text, calls fixed plugin action IDs,
knows provider URI schemes, creates provider login dialogs, and starts a
Telegram-specific authorization timer. `SettingsProvidersSection.qml` always
creates all four provider cards. Consequently, removing or not building a
plugin does not remove its functionality from the visible UI.

The required behavior is stricter than disabling unavailable controls:

- if a plugin is not loaded, the host must not show any settings UI belonging
  to that plugin;
- the host Settings implementation must not need to know which optional
  providers exist;
- loading, unloading, or rescanning plugins must update Settings without an
  application restart;
- provider-specific authorization workflows and text must live with the
  provider plugin.

## Current architecture findings

### Plugin registry

`FileProviderPluginRegistry` already owns the authoritative list of loaded
plugins. `FilePluginInfo` reports provider, action, places, and book-preview
capabilities. `PluginActionController::plugins()` exposes this information to
QML, and `pluginsChanged()` is emitted after load, unload, and rescan actions.

This is enough to implement a temporary visibility check, but it is not enough
for the target architecture because Settings would still contain every
provider's UI and action protocol.

### Settings

The following host files contain provider-specific settings knowledge:

- `qml/components/SettingsDialog.qml`
  - authorization state for four providers;
  - hardcoded action IDs;
  - provider URI schemes;
  - login, logout, import, and reset workflows;
  - MEGA and Telegram dialog instances;
  - Telegram polling timer.
- `qml/components/settings/SettingsProvidersSection.qml`
  - four permanently instantiated provider cards;
  - provider-specific buttons and controls.
- `qml/components/settings/MegaLoginDialog.qml`
- `qml/components/settings/TelegramLoginDialog.qml`
- `qml/components/settings/TelegramForgetLocalDataDialog.qml`

The Settings dialog also calls every provider's status action when opened,
even if that provider is not loaded.

### Existing plugin-owned QML precedent

The audio-tag editor plugin embeds its QML in the plugin resource and returns a
`qrc:/` component URL. The host loads it through `PluginUiDialog`. This proves
that a dynamically loaded plugin can own and expose a QML component without
adding that component to the main FM QML module.

### Places

Places is partially availability-aware. `PlacesModel` checks
`hasPluginProviderForPath()` before adding Google Drive, MEGA, and Telegram
roots. However, it still hardcodes their names, paths, icons, account-status
actions, and fallback subtitles.

Places migration is related, but it should follow the Settings migration so
that the first patch remains reviewable. The existing `PlacesProviderPlugin`
interface is the likely destination for those roots.

## Rejected short-term solution

Do not solve this by adding four expressions such as:

```qml
visible: root.hasPlugin("mega")
```

That would hide unavailable cards but retain all of the architectural
coupling: the host would continue to own provider IDs, action IDs, dialogs,
status polling, text, and authentication flows. It would also require another
host patch for every new settings-capable plugin.

Availability flags may be used temporarily while migrating one provider at a
time, but they are not the end state.

## Target architecture

### New plugin capability

Add a small independent interface, tentatively named `PluginSettingsUi`.
Keeping it separate from `FileProviderPlugin` and `FileActionPlugin` allows:

- a provider without settings UI;
- a non-provider plugin with settings UI;
- interface evolution without changing the existing provider/action ABI;
- one plugin object to implement several capabilities via `Q_INTERFACES`.

Proposed API v1:

```cpp
inline constexpr int FM_PLUGIN_SETTINGS_UI_API_VERSION = 1;

class PluginSettingsUi
{
public:
    virtual ~PluginSettingsUi() = default;

    virtual int settingsUiApiVersion() const = 0;
    virtual QString settingsUiPluginId() const = 0;
    virtual QString settingsUiTitle() const = 0;
    virtual QString settingsUiComponentUrl() const = 0;
    virtual int settingsUiOrder() const = 0;
};
```

Possible descriptor returned to QML:

```text
pluginId
title
componentUrl
order
loaded
```

Do not add speculative schema, icon, category, search keyword, or navigation
fields in v1 unless the first MEGA slice demonstrates a concrete need.

### Ownership boundary

The host owns:

- discovery and validation of settings-capable plugins;
- ordering and loading settings components;
- common Settings layout and styling primitives;
- the generic plugin action bridge;
- reacting to plugin load/unload/rescan.

Each plugin owns:

- its settings card/component;
- its authorization and status text;
- its action IDs and parameters;
- login/import/reset dialogs;
- provider-specific timers or asynchronous state transitions;
- provider-specific URI schemes used by its settings workflow;
- its embedded QML resources.

### Runtime flow

```text
FileProviderPluginRegistry
    -> discovers PluginSettingsUi capability
    -> validates API version and matching plugin ID
    -> exposes sorted settings descriptors

PluginActionController
    -> exposes settingsComponents() to QML
    -> emits pluginsChanged after load/unload/rescan

SettingsProvidersSection
    -> refreshes descriptors
    -> Repeater creates one Loader per descriptor
    -> Loader loads the plugin-owned qrc:/ component
```

If the descriptor list is empty, the entire Providers section must be absent,
not an empty bordered section.

## Implementation phases

### Phase 1: settings capability contract

Add:

- `src/core/PluginSettingsUi.h`;
- API version and Qt interface IID;
- a settings capability pointer in the registry entry;
- a `hasSettingsUi` field in `FilePluginInfo`;
- validation in `loadPluginFile()`:
  - supported API version;
  - non-empty plugin ID;
  - settings interface ID matches the other interfaces implemented by the
    same object;
  - non-empty `qrc:/` component URL;
- `settingsUiDescriptors()` in the registry;
- `settingsComponents()` in `PluginActionController`.

The registry should return plain descriptors and must not instantiate QML.

Verification:

- existing plugins still load;
- a plugin with an invalid settings API version is rejected with a useful
  registry error;
- a plugin with no settings capability is unaffected;
- descriptor ordering is deterministic.

### Phase 2: MEGA vertical slice

Use MEGA to validate the complete design before migrating all providers.

Move into the MEGA plugin:

- MEGA settings card;
- `MegaLoginDialog.qml`;
- authorization status refresh;
- login and logout actions;
- status and storage text;
- `mega:///` navigation after successful login;
- signed-out workspace handling if it belongs to the settings workflow.

Embed those files with `qt_add_resources(fm_mega_provider ...)` and return the
main component URL from `PluginSettingsUi`.

The plugin component can use the generic `pluginActionController` context
property initially. It must contain the `mega::...` action IDs internally so
the host never sees them.

Important lifecycle requirements:

- do not call an action before the component is loaded;
- close plugin-owned dialogs before unloading the plugin;
- stop timers and asynchronous callbacks when the component is destroyed;
- do not retain QML objects from an unloaded plugin resource.

Verification:

- MEGA plugin present: MEGA settings UI appears and login/logout still works;
- MEGA plugin absent at startup: no MEGA text, controls, dialogs, or action
  calls exist in Settings;
- MEGA plugin unloaded while Settings is open: its UI is destroyed safely;
- MEGA plugin loaded/rescanned while Settings is open: its UI appears.

### Phase 3: dynamic Providers section

Replace the fixed contents of `SettingsProvidersSection.qml` with a generic
descriptor model and `Loader` instances.

The host component should only provide:

- common width and spacing;
- descriptor ordering;
- loading/error containment;
- optional generic message if a component fails to load.

The host must not special-case a plugin ID to select a component.

The surrounding `SettingsDialog` should instantiate the Providers section
only when at least one settings descriptor exists.

### Phase 4: remaining provider migrations

Move providers one at a time, preserving behavior.

#### Google Drive

Move:

- authorization status;
- browser login action;
- sign out;
- credential-manager explanation;
- `gdrive://` navigation and signed-out handling.

#### Instagram

Move:

- cookie/session import UI;
- file dialog ownership;
- environment-override status;
- sign out and `instagram://` handling.

#### Telegram

Move:

- API ID/hash/phone login flow;
- confirmation code and 2FA password flow;
- authorization polling timer;
- forget-local-data confirmation dialog;
- chat/source field and open action;
- `telegram://` navigation and signed-out handling.

Telegram should migrate last because its UI has the most states and lifecycle
behavior.

FTP and portable-device plugins currently have no provider settings card. They
should not implement `PluginSettingsUi` until they have actual settings to
expose.

### Phase 5: host cleanup

After all four migrations, remove from the host:

- `googleDriveAuthorized`, `megaAuthorized`, `instagramAuthorized`, and
  `telegramAuthorized`;
- provider status strings;
- all provider refresh/login/logout/import/reset functions;
- Telegram settings timer;
- fixed provider action IDs and URI schemes;
- host-owned MEGA and Telegram dialog instances;
- migrated QML files from the main `MY_QML_FILES` list;
- provider-specific imports made unused by the migration.

Run a full reference search before deletion. Every removed host line should be
traceable to functionality now owned by a plugin.

### Phase 6: Places follow-up

After Settings is complete, migrate cloud root entries to
`PlacesProviderPlugin`:

- Google Drive plugin returns its places and current account subtitle;
- MEGA plugin returns its root and account subtitle;
- Telegram plugin returns its root and account subtitle;
- `PlacesModel` consumes provider places without fixed provider IDs or paths;
- provider authorization changes request a generic places refresh.

Remove hardcoded cloud provider helpers and root construction from
`PlacesModel.cpp` only after equivalent plugin-provided behavior is verified.

Generic icon rendering may still recognize icon tokens such as `mega` or
`gdrive`; that is presentation vocabulary, not proof that the provider exists.

## Test plan

### Registry/controller tests

- settings capability is detected;
- descriptors contain the expected ID, title, URL, and order;
- invalid API versions and mismatched IDs produce load errors;
- a provider/action plugin without settings UI has no descriptor;
- unloaded plugins are excluded from active descriptors;
- load, unload, and rescan emit the update signal.

### QML behavior tests or focused runtime harness

- zero settings plugins means no Providers section;
- one plugin means exactly one loaded component;
- unloading removes the component;
- loading adds it without reopening Settings;
- a broken component URL shows a generic error and does not break the rest of
  Settings.

### Build matrix

- normal Linux build with all available plugins;
- `FM_ENABLE_MEGA_PLUGIN=OFF`;
- MEGA enabled but SDK unavailable;
- `FM_ENABLE_TELEGRAM_PLUGIN=OFF`;
- Telegram enabled but TDLib unavailable;
- Windows core workflow where MEGA and Telegram are disabled;
- packaged install contains each plugin's settings QML resources only when the
  plugin itself is present.

### Manual acceptance

For each migrated provider:

1. Start without the plugin binary: no related UI is visible.
2. Load the plugin through Plugin Manager: its settings UI appears.
3. Exercise login/status/logout behavior.
4. Unload the plugin: its settings UI disappears safely.
5. Restart and verify persisted authorization behavior.
6. Confirm unrelated Settings sections are unchanged.

## Risks and constraints

### Plugin unload and QML resource lifetime

This is the highest-risk area. A plugin must not be unloaded while QML objects
created from its resources remain alive. The unload flow may need to notify the
Settings host, destroy the relevant Loader item, and only then unload the
library. Investigate the current `QPluginLoader` lifetime behavior before
finalizing this phase.

### QML access to host services

Plugin QML currently can access context properties such as
`pluginActionController`. Keep the initial interface narrow and reuse this
bridge. Do not introduce a new provider-specific C++ object into the host
context.

### Packaging

Each plugin's QML must be embedded in that plugin target. Windows and Linux
packaging must include the plugin binary; no provider settings files should be
copied as part of the host QML module.

### Migration state

During phased migration, the Providers section may temporarily contain both a
dynamic MEGA component and fixed cards for providers not migrated yet. Keep
this intermediate state explicit and short-lived. Do not create duplicate
cards for the same provider.

## Recommended first work session

1. Add `PluginSettingsUi.h` and registry plumbing.
2. Add a minimal mock settings capability and tests.
3. Expose descriptors through `PluginActionController`.
4. Embed a minimal MEGA settings component in `fm_mega_provider`.
5. Render it dynamically next to the still-static remaining provider cards.
6. Verify startup without the MEGA plugin and runtime unload before moving the
   full MEGA authentication UI.

Stop after this vertical slice for review. Do not migrate the other providers
until plugin unloading, QML resource lifetime, and reactive updates are proven.

## Completion criteria

The project reaches the target state when:

- the host Settings QML contains no IDs, paths, dialogs, text, or workflows for
  Google Drive, MEGA, Instagram, or Telegram;
- a provider's settings UI exists if and only if its plugin is loaded and
  advertises the settings capability;
- plugin load/unload/rescan updates an open Settings dialog safely;
- builds without optional SDKs contain no UI for the unavailable providers;
- all provider authentication workflows retain their current behavior;
- automated tests and the Linux/Windows build matrix pass.
