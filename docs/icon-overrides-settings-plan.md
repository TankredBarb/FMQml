# User-Managed Icon Overrides Plan

## Decision

Replace the hardcoded native-icon overrides for APK, EPUB, FB2, and FB2-in-ZIP with user-managed rules.

There will be no special built-in override list to administer. A user can create, inspect, edit, and remove a rule for any file suffix, including compound suffixes such as `fb2.zip`. Removing a rule restores the platform/default icon for that suffix.

The rule affects icon selection only while **Native icons** is enabled. The manager remains available when it is disabled so rules can be prepared or cleaned up; it must state that they are currently inactive.

## Current State

`FileTypeIconResolver` currently has a private hardcoded table:

| Suffix | Forced bundled icon |
| --- | --- |
| `apk` | `archive` |
| `epub` | `epub` |
| `fb2`, `fb2.zip` | `fb2` |

That table currently serves two different purposes:

1. it selects the bundled file-type icon when native icons are disabled;
2. it bypasses the platform icon when native icons are enabled.

The first purpose stays as normal bundled file-type classification. The second purpose becomes the user rule system.

The native override currently happens in several QML presentation paths, while other paths call `image://icon` directly. That makes an override inconsistent across file panels, previews, dialogs, and properties views. The new implementation must make the native icon provider enforce the same rules for every caller.

## User Experience

Add a compact **Icon overrides** entry to Settings near the Native icons toggle. It opens a dedicated overlay that follows the application's existing overlay shell and navigation conventions. The same overlay is available through the command palette.

The overlay contains:

- a short status line: `Active while Native icons is enabled` or `Inactive because Native icons is disabled`;
- an **Add override** action;
- a compact list of existing rules, each showing:
  - the suffix (for example, `epub` or `fb2.zip`);
  - a live icon preview;
  - the chosen source and value;
  - **Edit** and **Restore default** actions;
- an empty state explaining that no suffix currently replaces the system icon.

The editor supports one suffix per rule and these icon sources:

| Source | Stored value | Selection UI |
| --- | --- | --- |
| Theme icon | freedesktop/Windows theme icon name | searchable theme-icon picker with preview |
| Local file | absolute path to SVG, PNG, ICO, or other decodable image | existing file chooser, with preview and validation |
| FM bundled icon | asset name from `qml/assets/filetypes-next` | bundled-icon picker with preview |

Saving normalizes the suffix (lowercase, no leading dots) and replaces an existing rule for that exact suffix. Compound suffixes are first-class values: a rule for `fb2.zip` applies only to files whose names end in `.fb2.zip`; it must not require a rule for `zip` and must not change other ZIP archives. **Restore default** deletes the persisted rule after a confirmation that names the suffix. A bulk **Restore all defaults** action is allowed only when at least one rule exists and also requires confirmation.

## Data Model and Persistence

Create an `IconOverrideRegistry` C++ owner rather than storing rule logic in QML or directly in `FileTypeIconResolver`.

Each persisted rule has this shape:

```text
suffix: "fb2.zip"
sourceType: "theme" | "file" | "bundled"
sourceValue: "book-open" | "/path/to/icon.svg" | "epub"
```

Persistence lives in `QSettings` under the appearance settings and participates in existing settings export/import. Invalid entries are ignored safely during loading and reported as an unavailable rule in the settings list instead of crashing icon rendering.

The registry exposes QML-friendly rows and invokables through `AppServices` / the existing `fileTypeIconResolver` context object:

- `iconOverrides()`;
- `addOrUpdateIconOverride(suffix, sourceType, sourceValue)`;
- `removeIconOverride(suffix)`;
- `clearIconOverrides()`;
- `availableBundledIconNames()`;
- revision/change notification for immediate visual refresh.

For local-file rules, the row retains a missing or unreadable path and exposes its unavailable state so the user can repair or remove it. Rendering falls back to the platform/default icon until it is valid again.

## Resolution Architecture

1. Remove `nativeIconOverrideRules()` and all APK/EPUB/FB2 hardcoded native overrides from `FileTypeIconResolver`.
2. Keep bundled type classification independent: EPUB may still resolve to the bundled EPUB asset when Native icons is off, but that is not a user override.
3. Make `IconProvider` consult `IconOverrideRegistry` before asking Windows Shell, Linux icon themes, or provider-specific icon lookup. When a valid rule exists, render the selected theme/local/bundled source and cache it with the registry revision.
4. Migrate `FileIconCell`, preview renderers, archive preview, and `FileEntryPresentationResolver` away from their duplicated native-override checks. They should request the native provider normally and let it make the decision.
5. Include the registry revision in QML icon source/cache identity, so an add, edit, or removal updates visible file delegates without reopening a panel.
6. Preserve thumbnail precedence: an image thumbnail remains more important than a file-type icon; override rules affect the icon fallback only.

## Implementation Sequence

### 1. Registry and tests

Implement normalization, validation, persistence, migration-safe loading, replace-by-exact-suffix semantics, removal, and reset. Match suffixes case-insensitively and choose the longest matching suffix, so `fb2.zip` wins over a possible `zip` rule. Add focused C++ tests for all source types, invalid values, persistence round-trip, source-file disappearance, and compound-suffix matching that leaves ordinary ZIP files untouched.

### 2. Single native resolution path

Remove the hardcoded native rule table. Integrate the registry into `IconProvider`, add cache revisioning, and remove duplicate QML/native interception. Verify that APK, EPUB, and FB2 now use the platform icon by default.

### 3. Manager UI

Add the dedicated manager overlay and a compact Settings entry that opens it. Use existing dialog controls and file chooser patterns; do not introduce a second persistence layer in QML. Add explicit inactive-state wording for disabled Native icons.

### 4. Regression and manual verification

Test these cases on Linux and Windows where available:

1. no rules: APK, EPUB, and FB2 show platform/default native icons;
2. each source type applies to an arbitrary suffix in panel, Preview Pane, Quick Look, properties, conflict/delete dialogs, and search;
3. editing a visible rule refreshes icons immediately;
4. a `fb2.zip` override affects only `.fb2.zip`, while ordinary `.zip` files retain their own default or explicit `zip` rule;
5. restoring one suffix and restoring all rules return the platform/default icon;
6. disabled Native icons leaves the rule list visible but does not apply its rules;
7. local-file rule that becomes unavailable is visible as broken, falls back safely, and can be repaired or removed;
8. thumbnails continue to win over icon overrides.

Run the focused tests, `cmake --build build -j 12`, `ctest --test-dir build --output-on-failure`, and `git diff --check`.

## Non-Goals for the First Version

- per-file or per-folder icon overrides;
- MIME-type, glob, or provider-path rule matching;
- importing/exporting icon image files themselves (only their paths are stored);
- changing application, toolbar, or provider-root branding icons;
- modifying thumbnails or cover-art extraction.
