# FMQml QA Regression Suite

This suite gives QA and developers a repeatable baseline for new FMQml builds. The focus is the file manager core: navigation, file operations, rename/delete behavior, access rules, native watcher updates, and UI synchronization.

Out of scope: theme editor coverage, deep visual theme testing, rare platform integrations, and benchmark-grade performance testing.

## Setup

1. Build or install the candidate FMQml build.
2. Create the QA sandbox:

```powershell
.\scripts\New-QaSandbox.ps1
```

The default sandbox path is:

```text
D:\QASandbox
```

If Windows blocks `.ps1` execution, use:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\New-QaSandbox.ps1
```

To recreate the sandbox:

```powershell
.\scripts\New-QaSandbox.ps1 -Clean
```

To create a larger dataset:

```powershell
.\scripts\New-QaSandbox.ps1 -BulkCount 1000 -NestedDepth 5
```

3. Open FMQml and navigate both panels into different folders inside `D:\QASandbox`.
4. For watcher tests, keep a separate PowerShell window or Explorer open and mutate files outside FMQml.
5. For every failed case, record build id, OS, exact steps, expected result, actual result, repro rate, and attachments.

## High Priority Areas

Run these carefully during the current stabilization campaign:

- Single rename: cases 28-41.
- Batch rename: cases 42-52.
- Delete: cases 53-63.
- Copy, move, paste: cases 64-76.
- External watcher mutations: cases 77-87.
- Current folder externally deleted: cases 88-92.
- Permissions and access UI: cases 93-101.

## 1. Launch And Basic Smoke

1. Launch the application. Expected: both panels render, no blank or frozen window.
2. Close the application via the window close button. Expected: exits without crash.
3. Launch the application again. Expected: starts normally and state is not corrupted.
4. Switch the active panel with the mouse. Expected: focus and actions follow the selected panel.
5. Switch the active panel with the keyboard if a shortcut exists. Expected: active panel changes predictably.

## 2. Navigation

6. Open `D:\QASandbox` in the left panel. Expected: contents match the filesystem.
7. Open `Alpha`. Expected: path updates and folder contents render.
8. Navigate up one level. Expected: returns to the parent without losing usable focus.
9. Open `Copy Source`, which contains a space in the name. Expected: path and contents render correctly.
10. Open `Nested\Level-01\...`. Expected: deep navigation works without UI errors.
11. Refresh the current folder if refresh is available. Expected: no duplicated or missing entries.
12. Navigate the right panel to a different sandbox folder. Expected: panels remain independent.
13. Navigate both panels to the same folder. Expected: both show the same contents without selection/focus conflicts.

## 3. Selection And Active Panel

14. Select one file. Expected: actions match a single-file selection.
15. Select several adjacent files. Expected: selection count/status is correct.
16. Select files and folders together. Expected: only valid operations are enabled.
17. Clear selection. Expected: selection state is cleared.
18. Switch the active panel after selecting items. Expected: selection does not migrate between panels.
19. Open the context menu on a selected file. Expected: items apply to the selected file.
20. Open the context menu on empty space. Expected: items apply to the current folder.
21. Run a keyboard command while the right panel is focused. Expected: action applies to the right panel.

## 4. Create

FMQml create actions generate the new file/folder name themselves. They do not currently ask for an arbitrary name during creation. Invalid-name validation must be tested through rename.

22. Create a folder with the application command. Expected: a folder appears and watcher/UI do not duplicate it.
23. Create a file with the application command. Expected: a file appears and remains visible after refresh.
24. Create several files/folders in a row. Expected: generated names do not conflict.
25. Create a file, then immediately refresh. Expected: file remains visible and is not duplicated.
26. Create a folder, then immediately enter it. Expected: navigation succeeds.
27. Create in both panels sequentially. Expected: each panel updates only for its current folder.

## 5. Single Rename

28. Rename a normal file. Expected: old name disappears, new name appears, selection/focus stay coherent.
29. Rename a folder. Expected: folder is accessible under the new name and contents are preserved.
30. Rename a file to a name containing spaces. Expected: operation succeeds.
31. Rename a file to a mixed-case name. Expected: displayed case exactly matches the filesystem.
32. Perform a Windows case-only rename: `case_only.txt` -> `Case_Only.txt`. Expected: not treated as a conflict and displayed case updates.
33. Rename `Alpha` externally or through FMQml and refresh. Expected: displayed case does not randomly drift to lowercase.
34. Rename a file to an existing name. Expected: conflict is reported and no silent overwrite happens.
35. Rename a file, then immediately delete it. Expected: the new path is used.
36. Rename a file, then immediately copy it to the other panel. Expected: the new path is copied.
37. Start rename and cancel with Escape or by leaving edit mode. Expected: name does not change.
38. Start rename and move window focus away. Expected: application remains responsive and result is predictable.
39. Rename a file to a name containing an invalid Windows character: `<`, `>`, `:`, `"`, `/`, `\`, `|`, `?`, or `*`. Expected: operation is rejected or fails cleanly; original file is preserved.
40. Rename a file to a reserved Windows device name: `CON`, `PRN`, `AUX`, `NUL`, `COM1`, or `LPT1`. Expected: operation is rejected and original file is preserved.
41. Rename a file to a reserved name with an extension, for example `CON.txt`. Expected: operation is rejected and original file is preserved.

## 6. Batch Rename

42. Select several files and apply a simple batch rename pattern. Expected: all selected files are renamed.
43. Check batch rename preview before applying. Expected: preview matches final result.
44. Batch rename with numbering. Expected: order and numbers are stable.
45. Batch rename with mixed-case output names. Expected: displayed case matches preview and filesystem.
46. Batch rename that conflicts with an existing file. Expected: conflict is shown and no silent overwrite happens.
47. Batch rename with case-only changes. Expected: preview does not show false conflicts.
48. Batch rename where one file is externally deleted before apply. Expected: partial failure is clear and app does not crash.
49. Batch rename folders. Expected: folder contents are preserved.
50. Batch rename with selection in the active panel while the other panel shows another folder. Expected: only active-panel selection is affected.
51. Batch rename swap case: `a.txt` -> `b.txt`, `b.txt` -> `a.txt`. Expected: if unsupported, scenario is blocked without partial damage.
52. Batch rename that generates an invalid or reserved Windows name. Expected: preview/apply blocks the bad items and source files are preserved.

## 7. Delete

53. Delete one file through UI. Expected: confirmation appears and file disappears after confirmation.
54. Cancel the delete confirmation. Expected: file remains.
55. Delete several files. Expected: all selected files are removed and selection is cleared.
56. Delete a folder with nested files. Expected: folder is fully removed or a clear error is shown.
57. Delete a file immediately after rename. Expected: the renamed path is deleted.
58. Delete a file that was externally removed before confirmation. Expected: app does not crash and reports/ignores cleanly.
59. Delete a read-only file. Expected: behavior matches app/OS policy with no false success.
60. Try delete in a protected or inaccessible location. Expected: operation is disabled by preflight or fails with a clear error.
61. Delete from the second panel through context menu. Expected: operation applies to the second panel.
62. Delete through keyboard shortcut. Expected: same preflight and confirmation path as UI button/menu.
63. Delete a large group from `Bulk`. Expected: queue/progress completes and list updates.

## 8. Copy, Move, Paste

64. Copy one file from left panel to right panel. Expected: file appears in destination.
65. Copy a folder with nested contents. Expected: structure and contents are preserved.
66. Copy several files. Expected: all files appear and progress completes.
67. Copy into a folder where the name already exists. Expected: conflict policy/UI is used, no hidden overwrite.
68. Move/cut one file between panels. Expected: source disappears and destination appears.
69. Move/cut a folder between panels. Expected: structure is preserved.
70. Cut and paste into the same folder. Expected: no-op does not create duplicates or corrupt history.
71. Copy and paste into the same folder. Expected: keep-both/conflict policy or clear error.
72. Move a file immediately after an external rename. Expected: current path is used or a clear error is shown.
73. Run a large copy while navigating UI. Expected: application remains responsive.
74. Cancel a long operation if cancellation is supported. Expected: partial result and status are clear.
75. Run undo/redo after copy/move if supported. Expected: operation is reversible or clearly rejected.
76. After copy/move, check current-folder capabilities. Expected: access state updates without waiting several seconds.

## 9. External Watcher Mutations

For these tests, use:

```powershell
.\scripts\Invoke-QaWatcherMutation.ps1
```

Execution Policy fallback:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Invoke-QaWatcherMutation.ps1
```

77. Create a file externally in the open folder. Expected: file appears without manual refresh.
78. Delete a file externally in the open folder. Expected: file disappears.
79. Rename a file externally in the open folder. Expected: new name appears and old name is gone.
80. Create a folder externally. Expected: folder appears.
81. Delete a folder externally. Expected: folder disappears.
82. Quickly create and delete a file externally. Expected: UI does not show a permanent phantom entry.
83. Rename one file externally several times quickly. Expected: final name is correct.
84. Externally replace a file by deleting and recreating the same name. Expected: metadata/size updates.
85. Externally create 500 files in the open folder. Expected: UI catches up without crash.
86. Externally delete 500 files from the open folder. Expected: UI catches up without crash.
87. Run external mutations in the inactive panel folder. Expected: inactive panel updates too.

## 10. Current Folder Externally Deleted

88. Open `DeleteMeCurrent` in the active panel and delete that folder externally. Expected: application does not crash.
89. After external deletion of the current folder, run refresh. Expected: UI enters a valid state or shows a clear error.
90. After external deletion of the current folder, press Back or Up. Expected: user can return to an existing parent.
91. After external deletion of the current folder, open context menu. Expected: impossible create/delete actions are not offered.
92. Delete the inactive panel current folder externally. Expected: activating that panel does not crash and state is coherent.

## 11. Permissions And Access UI

93. Open `ReadOnly Area`. Expected: create/rename/delete are disabled or fail cleanly according to OS permissions.
94. Try to create a file in `ReadOnly Area`. Expected: no false success.
95. Try to rename a file without write/delete permission. Expected: blocked by preflight or OS error.
96. Verify context menu does not offer Cut/Delete where `canDelete` is false.
97. Verify toolbar, shortcuts, and context menu use the same access rules.
98. Change folder permissions externally and wait less than 3 seconds. Expected: after local operation/refresh, UI does not remain stuck in stale capability state.
99. Change folder permissions externally and wait more than 3 seconds. Expected: cache TTL does not hide the new state forever.
100. Check a protected/system-like location if safe. Expected: app does not crash and does not offer false destructive actions.
101. Try delete confirmation from a location that becomes inaccessible after the dialog opens. Expected: confirm path revalidates and blocks cleanly.

## 12. Filter, Sort, Search

102. Sort by name. Expected: order is stable and displayed names keep correct case.
103. Sort by size. Expected: sizes match files.
104. Sort by modified date. Expected: recently changed files move according to sort order.
105. Enable a `.txt` filter. Expected: only matching items are shown.
106. Clear filter. Expected: full list is restored.
107. Create a matching file externally while filter is active. Expected: file appears.
108. Rename a filtered file externally so it no longer matches. Expected: file disappears from filtered view.
109. Select a file, then change sort. Expected: selection still points to the same file.

## 13. Preview, Properties, Metadata

110. Open properties for a file. Expected: path, size, and dates are correct.
111. Open properties for a folder. Expected: no crash and data is understandable.
112. Modify a file externally and reopen properties. Expected: size/date update.
113. Run checksum if available. Expected: process completes and UI remains responsive.
114. Delete a file externally during metadata/properties flow. Expected: clear error and no crash.

## 14. Archives And ISO

115. Open a normal archive if supported. Expected: browsing works.
116. Try to delete an item inside archive/virtual location. Expected: destructive action is blocked if backend does not support it.
117. Open or mount an ISO if enabled. Expected: panel shows contents.
118. Try rename/delete inside ISO. Expected: operation is blocked as read-only/managed.
119. Close ISO/virtual root and return to a normal folder. Expected: capabilities return to normal.

## 15. Persistence And Dialog Safety

120. Open different folders in both panels and close the app. Expected: exits without crash.
121. Start again. Expected: persisted state is restored if supported; otherwise default state is stable.
122. Change column sizes or sorting if persisted. Expected: restart does not corrupt state.
123. Open delete confirmation and press Enter. Expected: same safe confirm path as button click.
124. Open delete confirmation and press Escape. Expected: operation is canceled.
125. Open rename and press Enter. Expected: rename is applied once.
126. Open rename and press Escape. Expected: rename is canceled.
127. Open several dialogs/overlays sequentially. Expected: no stuck dimming layer or invisible modal state.

## 16. Stability And Long Run

128. Leave the app open for 30 minutes while an external script creates/deletes files. Expected: no crash and memory growth is not obviously abnormal.
129. Perform 50 renames in one folder. Expected: UI does not lose entries.
130. Perform 20 copy/move operations between panels. Expected: operation queue remains coherent.
131. Perform 20 delete operations. Expected: confirmations and status do not break.
132. Switch panels quickly during watcher events. Expected: active panel does not get confused.
133. Change folders quickly during watcher events. Expected: events are not applied to the wrong current path.

## Minimal Release Gate

Run at least these for every new build:

- 1-13: launch and navigation.
- 22-27: create workflow with generated names.
- 28-41: single rename, including invalid and reserved names.
- 42-52: batch rename.
- 53-63: delete.
- 64-76: copy/move/paste.
- 77-92: watcher updates and externally deleted current folder.
- 93-101: access rules and capability cache.
- 123-127: dialog and keyboard parity.

If time is limited, use this high-signal subset:

- 32, 33, 35, 39, 40, 41.
- 47, 51, 52.
- 57, 58, 62, 63.
- 70, 71, 76.
- 77, 79, 82, 85, 88, 90, 92.
- 96, 97, 98, 99, 101.

## Reporting Template

```text
Build:
OS:
Test case:
Preconditions:
Steps:
Expected:
Actual:
Repro rate:
Attachments:
Notes:
```
