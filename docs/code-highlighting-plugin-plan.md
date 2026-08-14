# Code highlighting plugin plan

## Scope

The first version supports C, C++, C/C++ headers, Python, and Bash/Shell.
Highlighting is optional. Removing the plugin must leave the ordinary text
preview usable and must remove all language-specific behavior and settings UI.

## Ownership boundary

The FM host owns only generic text-preview facilities:

- asynchronous bounded text pages;
- a read-only painted document view;
- optional styled character ranges with four opaque role numbers;
- an optional font-family override supplied with those ranges;
- discovery and lifetime validation for a text-decoration plugin capability.

The plugin owns:

- filename, MIME, and shebang recognition for supported languages;
- C/C++, Python, and Bash lexical rules;
- token-to-role assignment;
- lexer state across page boundaries;
- the code font and four token colors;
- persistence and plugin-owned Settings QML.

The host must not contain supported suffix tables, keyword lists, language
labels, or plugin setting keys. Without the plugin, `.cpp`, `.py`, and `.sh`
are rendered as ordinary text using the application's selected font.

## Contract

Add an independent `TextPreviewDecorationPlugin` Qt interface. A request
contains the display filename, MIME, decoded page content, and opaque state
from the preceding page. A result contains:

- whether the plugin claims the document;
- language id and user-facing label;
- default wrap and line-number preferences;
- optional font-family override;
- four colors;
- non-overlapping character ranges using role values 1 through 4;
- opaque end state for the next page.

The opaque state is stored with the page snapshot. Core never interprets it.
Decoration runs on the text controller's worker after decoding and before the
single snapshot publication. Cached pages retain their decoration and state.

## Rendering

`FmDocumentViewVisual` accepts a list of `{start, length, role}` ranges and four
colors. It paints only the intersection between visible rows and ranges. Base
text, selection, wrapping, scrolling, and copying remain unchanged. Selection
repaint keeps the existing selected-text color rather than token colors.

Plain text uses `Theme.fontFamily`. A plugin font override is applied only when
the plugin claims the document.

## Settings

The plugin implements `PluginSettingsUi` and embeds one settings component.
It provides:

- a font selector popup scoped to the code font;
- four color controls: Keywords, Strings, Comments, and Literals/Types;
- reset to plugin defaults.

Settings are stored under a plugin-owned QSettings group. No host theme or
typography setting is modified. Changes affect newly decorated pages; the
active preview is explicitly refreshed through a narrow decoration-revision
notification rather than by re-reading the source.

## Verification

- contract validation and absent-plugin fallback;
- lexer fixtures for every supported language, multiline state, escaped
  strings, comments, and page-boundary continuation;
- renderer range clipping and selection tests;
- settings persistence without changing the application font;
- local, provider-materialized, admin, and archive pages receive identical
  decoration for identical bytes;
- build FM and the plugin, run all tests, and run `git diff --check`.
