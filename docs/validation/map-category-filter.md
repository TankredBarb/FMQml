# Map category filtering

Map keeps folders as navigation containers and applies the panel's category
filter to files at every loaded depth. Other views retain their existing policy.
Contextual text search is unchanged: the root model still applies its current
search, without adding recursive search to Map.

DirectoryModel supplies a bounded snapshot before category filtering, retaining
hidden-file, text-search and sort policies. Partial sorting bounds the sorted
prefix for Map instead of sorting an entire large directory unnecessarily.
Nested listings remain lazy and unfiltered in the cache. Separate child lists
select displayed entries without deleting cached nodes. New listings use the
current category when committed, including requests started before a filter
change. Hidden files are omitted from hit testing, selection reconciliation and
Quick Look sibling navigation. Rename and deletion traverse all cached children.
Folders with filtered-out contents use a no-matching-files label.

Category changes perform no explicit directory reads and preserve the current
Map root's geometry, camera center and scale. Filtering runs on changes/listing
completion, not during rendering or individual wheel events. The extra cached
child lists remain bounded by Map's existing node limits. Existing per-folder
preview limits still apply; filtering does not scan the complete filesystem.

Validation:

- Build succeeded; git diff --check passed; added lines contain no Cyrillic.
- Six focused CTest targets passed in 23.89 seconds: Map unit tests, software
  fallback, startup, selection regression, navigation GUI and model algorithms.
- New regressions cover root snapshots retaining folders, nested and late
  listings, category changes/reset, camera state, filtered Quick Look siblings,
  rename/removal of hidden cached files and preservation of cached nodes.
- Forty category changes on 4096 cached files averaged 2.586 ms per change.
  Listing revision and pending-read counters stayed unchanged. This measures
  CPU filtering/layout cost, not an end-to-end FPS claim.
- Logs: /tmp/fmqml-map-filter-build.log and /tmp/fmqml-map-filter-tests.log.

The focused native Wayland/GPU run also passed (`mapCategoryFilter=true`,
`filterFilesReady=true`, `filterCameraEqual=true`) using the real panel model.
It checks root/nested images, hidden nonmatching files, retained folders, reset
and camera position relative to the viewport center. A viewport-height change
must not be mistaken for a camera pan. Log: /tmp/fmqml-map-category-focused.log.
The earlier full GPU benchmark was not green: its clipboard transfer cases lost
URI data, and its original camera assertion compared absolute screen positions.
Only the new filter case was rerun after correcting that assertion; the full
GPU benchmark is not claimed as passing.
