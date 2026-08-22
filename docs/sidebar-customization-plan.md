# Sidebar Customization Plan

## Goal

Turn the sidebar from one visually continuous list into a configurable stack
of independent rounded panels. Users must be able to choose which panels are
shown, hide the sidebar entirely, and change the vertical panel order without
losing navigation, keyboard, preview, or persisted workspace behavior.

The initial panel catalog is:

- `places`: system locations, drives, portable devices, and providers;
- `recent`: a compact list of recently/frequently visited local folders;
- `folders`: the hierarchical folder tree.

`recent` reuses the existing `FavoritesController.frequentModel`. It must not
introduce another visit tracker or persistent history store.

## Success Criteria

- Places, Recent folders, and Folder tree are visually separate rounded cards.
- Any subset of the three panels can be enabled, including none.
- The entire sidebar can be hidden and restored without losing its stored
  width, panel order, collapsed state, or panel proportions.
- Enabled panels can be reordered from Settings.
- Main menu `View` provides immediate sidebar and panel visibility actions.
- Focus Sidebar, Tab/Shift+Tab, Escape, Quick Look, selection, and scrolling
  work for every visible order and subset.
- Settings survive restart and participate in settings export, import, and
  workspace reset.
- Unknown, duplicate, or missing panel IDs in persisted settings are handled
  deterministically.
- Existing installations retain the current effective layout by default:
  Places and Folder tree visible, Recent folders available but initially off.

## Current Architecture

### Layout

- `qml/components/Sidebar.qml` is one `Pane` with an
  `AmbientPanelBackground` and a static `ColumnLayout`.
- Places and Folder tree each use `Layout.fillHeight`; a separator and two
  headers make them appear as sections of one large list.
- `qml/App.qml` owns Sidebar as the leading item in the outer horizontal
  `SplitView`, with a persisted width constrained to 140-300 px.

### Input and focus

- `Sidebar.qml` exports the concrete `placesList` and `foldersTree` items.
- `focusSidebar()` remembers a boolean `lastFocusedTree`.
- Places and Tree directly send Tab and Shift+Tab to each other.
- `App.qml`, `AppShortcuts.qml`, and the command registry assume exactly those
  two focus targets.
- Sidebar rows may drive Quick Look, and preview publication is delayed while
  the corresponding list is actively scrolling.

### Persistence and settings

- `WorkspaceStateCoordinator.qml` saves `sidebarWidth`, but no visibility,
  order, collapsed state, or vertical size information.
- `AppSettingsController` validates workspace values and owns workspace
  export/import behavior.
- Settings -> Workspace currently exposes only Split view and Preview pane.
- Main menu -> View currently has no Sidebar controls.

## Product Decisions

### Panel catalog, not a plugin framework

Use a fixed internal catalog of stable IDs. The first implementation must not
create a public registration API, plugin contract, arbitrary QML URL loader,
or configurable third-party panels. A catalog is sufficient for three known
panels and keeps validation, focus, and persistence explicit.

### Third panel: Recent folders

Recent folders is preferred over the following alternatives:

- Operations duplicates the existing Operations drawer.
- Storage duplicates This PC and drive entries in Places.
- Preview duplicates the independent Preview pane.
- Quick Access containing both pinned and frequent entries would substantially
  duplicate Places and the full Favorites view.

The Recent panel shows a bounded compact view of `frequentModel`, opens a row
in the active file panel, and supports the same Quick Look behavior as other
sidebar navigation rows. Empty state text is shown when no history exists.
Pinned-item management remains in Favorites.

### Default behavior

- `sidebarVisible = true`
- `sidebarPanelOrder = [places, recent, folders]`
- `sidebarHiddenPanels = [recent]`
- Places and Folder tree begin expanded.

This exposes the new capability without silently changing an existing user's
workspace after upgrade.

### Reordering interaction

The first complete version uses Move up and Move down actions in Settings.
They are deterministic, keyboard accessible, and easy to test. Direct dragging
of cards in the live Sidebar or Settings is a follow-up enhancement and must
reuse the same order API rather than add a second state path.

### Vertical sizing

Visible expanded cards share the available height using normalized weights.
Each card has a header-only collapsed height and an expanded minimum height.
Changing a boundary updates the two adjacent expanded weights. Hiding or
collapsing a card temporarily removes it from weight distribution without
discarding its stored weight.

If only one panel is expanded, it fills the available content height. If the
window is too short for all minimums, headers stay reachable and content areas
shrink to their safe minimum; the whole sidebar does not gain a competing
outer vertical Flickable.

Nested panel scrolling remains local to the Places, Recent, and Tree views.

## Persisted State Contract

Add these workspace keys:

```text
sidebarVisible: bool
sidebarPanelOrder: string list
sidebarHiddenPanels: string list
sidebarCollapsedPanels: string list
sidebarPanelWeights: map<string, real>
```

Stable panel IDs are `places`, `recent`, and `folders`.

### Sanitization

On load and import:

1. Ignore unknown IDs.
2. Keep only the first occurrence of a duplicate ID.
3. Append missing catalog IDs in catalog order.
4. Filter hidden/collapsed lists to known unique IDs.
5. Clamp each finite positive weight to an approved range.
6. Supply default weights for missing or invalid entries.
7. Normalize only the currently expanded visible weights for layout use; do
   not rewrite stored weights merely because a panel is temporarily hidden.

An empty visible set is valid. The Sidebar may remain logically enabled while
all panels are disabled; the UI should present this as an empty configuration
and offer panel actions from the menu/settings. The outer sidebar should not
occupy layout width when it has no visible panels.

### State ownership

- `App.qml` owns effective sidebar visibility and stored width, matching the
  existing preview/split workspace pattern.
- `Sidebar.qml` owns the live ordered panel state and reports user changes.
- `WorkspaceStateCoordinator.qml` captures and restores all keys in one
  transaction while persistence is paused.
- `AppSettingsController` validates the serialized representation for normal
  save, export, and import.

Do not store live QML object references, indices, pixel heights, or
`SplitView.saveState()` blobs as the long-term panel-order contract.

## Runtime Section Contract

Introduce one internal descriptor shape:

```text
{
    id: "places",
    title: "Places",
    icon: "...",
    enabled: true,
    collapsed: false,
    weight: 0.40
}
```

The descriptor is configuration, not content. Models and selection objects
remain owned by their concrete panels.

Each panel component exposes a narrow common surface:

- stable `panelId`;
- `focusPanel()`;
- `containsActiveFocus`;
- `previewCurrentItem()` where applicable;
- scroll-activity state where applicable;
- optional state needed to preserve position when reordered.

The shared `SidebarSectionCard` owns only:

- rounded surface and stroke;
- header title/icon;
- collapse action;
- content clipping;
- focus indication;
- resize-handle affordance supplied by the stack.

It must not know Places, Tree, Favorites, or navigation-controller semantics.

## Focus and Keyboard Contract

- Replace `lastFocusedTree` with a stable `lastFocusedPanelId`.
- `focusSidebar(trapTab)` resolves the last ID against the currently visible,
  expanded, focusable order and falls back to the first available panel.
- Tab advances through visible expanded panels in configured order and wraps.
- Shift+Tab moves in reverse and wraps.
- When a focused panel is hidden, collapsed, or the Sidebar is hidden, focus
  moves to the nearest remaining panel; if none remains, it returns to the
  active file panel.
- Escape always returns focus to the active file panel.
- `App.qml` queries a single `sidebar.containsActiveFocus` property rather than
  inspecting concrete child views.
- Focus Sidebar is disabled when the effective Sidebar has no focusable panel.
- Command text changes from "places and folders" to generic sidebar wording.

Reordering must not recreate `TreeView.selectionModel`, reset the selected tree
index, or discard Places/Recent scroll positions. Prefer stable panel instances
whose visual position is controlled by the stack. If a Loader-based approach
is used later, state preservation must be proven before switching.

## Main Menu Contract

Add to Main menu -> View:

- `Sidebar`: checked when effectively shown;
- `Sidebar Panels` submenu:
  - `Places`;
  - `Recent folders`;
  - `Folder tree`;
  - separator;
  - `Customize...`.

Panel items toggle membership in the visible set immediately. Enabling a panel
while the Sidebar itself is hidden does not have to force the Sidebar open;
the checked Sidebar action remains the explicit global visibility preference.
`Customize...` opens Settings focused on the Workspace/Sidebar section.

## Settings Contract

Extend Settings -> Workspace with a separate Sidebar section containing:

- Show sidebar toggle;
- one ordered row per catalog panel;
- visibility toggle per row;
- Move up and Move down actions;
- Restore defaults action.

Rows show title and a short purpose statement. Move actions operate on the
full catalog order, including hidden panels, so a hidden panel's future
position is predictable. Settings changes apply live through `App.qml`; the
dialog does not maintain an independent persisted copy.

Collapsed state and vertical sizes are direct Sidebar interactions, not
additional Settings controls in the first version.

## Implementation Phases

### Phase 1: independent card shells for existing panels

Work:

- Replace the single-plane visual composition with two rounded card shells.
- Keep the existing Places ListView and Folder TreeView instances and IDs.
- Preserve the current equal-height allocation, focus code, selection,
  scrolling, context menus, and Quick Look behavior.
- Use theme surface/stroke/radius tokens; do not introduce sidebar-only color
  constants or change global theme tokens.

Acceptance:

- Places and Folders visibly read as separate cards in built-in light and dark
  themes.
- No row geometry, model, selection, or keyboard behavior changes.
- Both lists still scroll independently and their scrollbars remain clipped.
- Sidebar resizing remains smooth and respects 140-300 px bounds.

### Phase 2: section card component and stable runtime catalog

Work:

- Extract the proven duplicated card shell into `SidebarSectionCard.qml`.
- Add the fixed catalog and ordered configuration helpers.
- Wrap Places and Tree content in concrete panel components only where doing
  so does not recreate their state during reorder.
- Add `containsActiveFocus`, panel lookup, and ordered focus helpers.

Acceptance:

- Swapping `[places, folders]` to `[folders, places]` changes only position.
- Selection, current item, content position, and Tree expansion survive reorder.
- Focus and Tab order follow the configured order.
- Invalid runtime IDs are ignored without QML Loader warnings.

### Phase 3: panel visibility and collapse

Work:

- Add enable/disable and collapse operations to the runtime stack.
- Resolve focus before removing or collapsing the active content area.
- Add accessible collapse actions to card headers.
- Define effective Sidebar visibility as global visibility plus at least one
  enabled panel.

Acceptance:

- Places only, Tree only, both, and neither configurations behave correctly.
- Hiding/collapsing the focused panel never leaves keyboard focus in an
  invisible item.
- Restoring a panel preserves its model-backed selection and reasonable scroll
  position for the current session.

### Phase 4: sidebar-level visibility and outer SplitView integration

Work:

- Add `sidebarVisible`, effective visibility, and show/hide API in `App.qml`.
- Collapse the Sidebar SplitView item to zero without overwriting
  `sidebarStoredWidth`.
- Restore the stored width when shown.
- Update leading Preview placement behavior when Sidebar is absent.
- Update focus routing, shortcuts, and command availability.

Acceptance:

- Hide/show cycles restore the previous width.
- Preview placement `after-sidebar` correctly becomes the leading visible pane
  while Sidebar is hidden and returns after Sidebar when restored.
- No stray splitter or blank strip remains.
- Startup restore does not briefly persist a zero sidebar width.

### Phase 5: validated persistence and migration

Work:

- Add all sidebar state keys to workspace capture and restore.
- Add C++ sanitizers for order, sets, and weights.
- Cover normal save, export, import, reset, and older settings with no keys.
- Pause save timers during transactional restore as for existing workspace
  geometry.

Acceptance:

- Restart reproduces visibility, order, collapse, width, and proportions.
- Old settings produce Places + Tree with Recent hidden.
- Corrupt/duplicate/unknown IDs recover to a deterministic valid catalog.
- Export/import round-trip preserves sidebar state.
- Workspace reset restores documented defaults.

### Phase 6: main menu and Settings controls

Work:

- Add View menu actions and checked states.
- Extend Settings Workspace UI with ordered panel rows and visibility controls.
- Add Move up/down and Restore defaults.
- Add a way for `Customize...` to open or focus the Sidebar settings section.

Acceptance:

- Menu and Settings remain synchronized during live changes.
- Reordering hidden and visible panels is deterministic.
- Controls remain usable at supported font scales and narrow dialog widths.
- Keyboard activation and accessible names are present.

### Phase 7: Recent folders panel

Work:

- Add a compact panel bound to `FavoritesController.frequentModel`.
- Limit rendered rows without creating a second filtered history store.
- Open rows in the active file panel and support Quick Look.
- Provide an empty state and react to unavailable-path removal.
- Reuse an existing favorite/recent delegate only if its geometry and actions
  match; otherwise create a sidebar-specific delegate without changing the
  full Favorites view.

Acceptance:

- Visits recorded by existing panel navigation appear without restart.
- No duplicate tracking or persistence file is introduced.
- Opening a row targets the active panel in split and single-panel modes.
- Missing paths cannot strand focus or trigger navigation errors.
- The panel adds no work while disabled.

### Phase 8: resizable panel proportions

Work:

- Add narrow vertical resize handles between adjacent expanded cards.
- Convert pointer deltas to normalized adjacent weights.
- Enforce header/content minimums and preserve the total expanded allocation.
- Reduce expensive visual effects while a divider is being dragged.
- Persist weights after interaction settles, not on every pointer frame.

Acceptance:

- Resizing affects only the adjacent expanded cards.
- No card overlaps, negative content height, or unreachable header occurs.
- Hide/collapse/show restores the stored proportions.
- Live resize remains responsive with a large expanded Folder tree.

### Phase 9: regression and visual acceptance

Automated checks:

- C++ sanitizer and workspace-state tests;
- QML load/startup smoke;
- focused tests for menu/settings state propagation;
- existing navigation, favorites, preview, and settings suites;
- `git diff --check` and full build.

Manual matrix:

- light/dark built-in themes, including Aurora;
- 140, 200, and 300 px sidebar widths;
- short and tall windows;
- 100%, enlarged font scale, and HiDPI;
- every panel visibility subset and all six three-panel orders;
- focus before/after hide, collapse, reorder, and app restart;
- single/split file panels and each Preview placement;
- long Places lists, deep Tree paths, empty and populated Recent history;
- scroll while Quick Look is open and rapid active-panel switching.

## Risks and Mitigations

### Stateful QML views recreated during reorder

Recreating a TreeView can reset expansion, selection, current index, and scroll.
Keep concrete panel instances stable and move their presentation through a
controlled stack. Treat any Loader/Repeater design as unacceptable until state
survival is verified.

### Nested scrolling and insufficient height

Do not put all cards inside a general outer Flickable. Keep one viewport and
local scrolling per expanded card, with explicit minimums and collapsible
headers.

### Persistence feedback during restore

Visibility and geometry changes can trigger width/weight save handlers. Use the
existing workspace restore/save pause and delayed-commit pattern.

### Focus points into hidden content

Resolve the next focus target before applying visibility/collapse changes and
centralize this in Sidebar rather than duplicating it in menu/settings code.

### Recent panel becomes a second Favorites product

Keep it read-only and compact: navigate and preview only. Pinning, tags,
history management, and rich metadata stay in Favorites.

### Visual cards consume too much width

Use small outer margins and existing row padding. Verify at the 140 px minimum
before increasing padding, icon sizes, or header actions.

## Non-Goals

- Public/plugin-defined sidebar panels.
- Multiple copies of the same panel type.
- Per-window or per-tab sidebar configurations.
- Dragging arbitrary files onto panel headers.
- Adding file selection, rename, context menus, or bulk actions to Recent.
- Combining Sidebar customization with a redesign of Places rows or Tree
  indentation.
- Changing global theme semantic tokens solely to style these cards.
- Direct drag reordering in the first complete version.

## Delivery Strategy

Land the feature in independently verifiable slices. Phase 1 intentionally
changes only visual containment. Phases 2-4 establish runtime behavior before
persistence or settings expose it. Phase 5 makes state durable. Phase 6 exposes
the controls. Phase 7 adds the new content only after the host contract is
stable. Phase 8 completes adjustable geometry, and Phase 9 closes the workstream
with the full interaction matrix.
