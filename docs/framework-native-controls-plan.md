# Native framework controls: progress and next steps

Status as of 2026-08-09. The current changes are intentionally left uncommitted for the project owner to commit.

## Completed

### Native menus

- Added `FmMenu`, `FmMenuItem`, and `FmMenuSeparator`.
- Menu surfaces, item states, separators, icons, and selection highlighting use the framework implementation and native `QPainter` visuals.
- Replaced and removed `ThemedContextMenu`, `ThemedMenuItem`, and `ThemedMenuSeparator`.
- Reused the same menu visuals for `FmComboBox` popup rows and the specialized popup lists converted in this pass.
- Fixed icon resource normalization, hover contrast, disabled-item hover state, and the obsolete leading item marker.
- `Open in Terminal` is hidden for non-directory file context items.

### Category 1: transparent aliases

Removed aliases whose behavior belongs in the base framework controls:

- `IconButton` -> `FmIconButton`
- `DialogActionButton` -> `FmButton`
- `SettingsComboBox` -> `FmComboBox`
- `PremiumTextField` -> `FmTextField`
- `ThemeEditorModePill` -> configured `FmButton`

The required defaults were folded into `FmButton`, `FmComboBox`, and `FmTextField`. Old component references are gone.

### Follow-up visual fix

Advanced permission toggles and the `Advanced permissions` heading now consistently use `Theme.accent`. Warning colors remain reserved for actual warning messages and administrator-mode indicators.

### Repeated toggle rows and text fields

- Added `FmToggleRow` with a native `QPainter` surface visual.
- Replaced and removed `SettingsToggleRow` and `AttributeToggleRow`; their 17 uses now share the framework control.
- Kept `SpecialModeToggle` as a domain component because only its switch is interactive.
- Replaced the remaining raw `TextField` controls in Batch Rename, the audio-tags plugin, and Telegram settings with `FmTextField`.

### Canvas visuals

- Added native `FmProgressRing` and replaced the checksum, RAM, CPU, and busy-row Canvas rings.
- Replaced the sort arrow, sidebar disclosure arrow, and hover-preview Play Canvas drawings with recolored SVG assets.
- Added native `FmRubberBandVisual` for the dashed file-selection outline.
- No QML `Canvas` instances remain in application or plugin components.

### Remaining raw controls and local aliases

- Replaced the remaining application-level raw `TextArea` and attached `ScrollBar` instances with `FmTextArea` and `FmScrollBar` while preserving the transparent text-preview presentation.
- Removed the local `ThemedComboBox` and `ThemedSpinBox` aliases; Checksum and Batch Rename now use `FmComboBox` and `FmSpinBox` directly.
- Added the theme font defaults required by those consumers to `FmSpinBox`.

### Linear progress

- Extended the native `FmProgressBar` visual with the proven shared track-height, track-color, animation-duration, and minimum-fill behavior.
- Replaced `LinearProgress` and the manual progress tracks in Operations Drawer and Drive Properties with `FmProgressBar`.
- Removed `LinearProgress`; no manual progress-width geometry remains in application or plugin QML.

### Remaining interactive pseudo-controls

- Replaced the local Proton toggle rows with configured `FmToggleRow` instances, including the unavailable-vkBasalt warning state.
- Moved File Panel error-banner actions onto `FmButton` while retaining the domain-specific local wrapper and layout.
- Moved the Typography font-family selector onto `FmButton` with its existing custom content layout.

## Verification completed

- `cmake --build build --target fm -j 12`
- Application startup smoke test
- `ctest --test-dir build --output-on-failure -j 12`: 39/39 passed
- `git diff --check`
- Search for raw Qt Quick Controls outside the framework
- Search for removed aliases, `Canvas`, `Shape`, `LinearProgress`, and manual progress-width geometry

## Next steps

### 1. Manually validate the final migration routes

Exercise the affected routes in the supported themes:

- Checksum and Batch Rename combo/spin controls
- Text Preview and the audio-tags lyrics editor
- progress bars in Disk Usage, Favorites, storage cards, drive previews, Operations Drawer, and Drive Properties
- Steam/Proton options, File Panel error actions, and the Typography font selector

Confirm keyboard navigation, focus, disabled states, scrolling, animation, and contrast across the System, Aurora, Porcelain, Ember, Graphite, Calus, and Catppuccin themes.

### 2. Keep domain components unless repetition is proven

Components such as `SpecialModeToggle`, `ActionPill`, and metric/card rows remain domain-specific. Revisit them only if at least two independent consumers prove the same interaction and presentation pattern. Decorative rectangles and domain visualizations are not migration targets.

### 3. Validate before any later removal slice

For every category:

1. Record all consumers and behavior differences.
2. Move only shared behavior into the native framework control.
3. Replace consumers and remove the obsolete component.
4. Build, run the exact affected UI route, run tests, check stale references, and run `git diff --check`.
