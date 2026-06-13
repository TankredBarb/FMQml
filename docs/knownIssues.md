# Known Issues

This document lists known issues, bugs, and regressions in the project along with their preliminary research findings.

---

### [BUG-001] Silent Execution Block when Launching Executables (.exe) with MOTW on Windows

*   **Component:** Process Launch Layer ([FilePanelController.cpp](file:///c:/Users/tankr/Documents/FM/FMQml/src/controllers/FilePanelController.cpp#L1834))
*   **Platform:** Windows
*   **Status:** Researched (pending fix)

#### Description
Attempting to launch a downloaded executable (`.exe`) file via double-click in the file manager results in absolute silence ("zero reaction"). However, running the same file from Windows Explorer triggers the Microsoft Defender SmartScreen prompt ("Windows protected your PC"), which allows the application to run after user confirmation.

#### Research Findings
1.  **Block Cause:** Files downloaded from the internet receive a `Zone.Identifier` alternative data stream (Mark-of-the-Web). Upon execution, the Windows security subsystem (SmartScreen) intercepts it to perform a reputation check.
2.  **Reason for Missing Dialog:** The launch layer uses `QDesktopServices::openUrl(QUrl::fromLocalFile(path))`, which invokes Win32's `ShellExecute` with parent handle `hwnd = NULL`.
3.  **OS Behavior:** Without a valid parent window handle (`HWND`), the OS suppresses the modal SmartScreen/UAC warning dialogs to prevent background clickjacking/hijack attempts. Consequently, `ShellExecute` fails with `SE_ERR_ACCESSDENIED`, execution is blocked, and the file manager silently fails without displaying any error UI to the user.

#### Recommended Solution
On Windows, replace `QDesktopServices::openUrl` with a native call to `ShellExecuteExW` and pass a valid parent `HWND` resolved from the active GUI window (`QGuiApplication::focusWindow()->winId()`). This anchors the dialogs to the file manager window, forcing them to display.

---

### [BUG-002] Folder Double-Click Blocked when Selection Badges are Enabled

*   **Component:** Item Delegates ([FileDelegate.qml](file:///c:/Users/tankr/Documents/FM/FMQml/qml/components/FileDelegate.qml), [FileTableDelegate.qml](file:///c:/Users/tankr/Documents/FM/FMQml/qml/components/FileTableDelegate.qml))
*   **Platform:** All
*   **Status:** Researched (pending fix)

#### Description
When selection badges (checkboxes) are enabled, double-clicking a directory icon frequently fails to open the directory.

#### Research Findings
The `isPointOnBadge(x, y)` method checks if a click falls within the badge boundaries to toggle selection. A double-click on the badge returns early to prevent opening the folder. However, the badge geometrically overlaps the folder icon:
1.  **List View (`FileDelegate.qml`):** The `fileContent` container containing the icon has a static `anchors.leftMargin: 12` and does not shift. The badge is positioned at `x: 8` with a width of `16` (spans **8–24 px**), while the icon starts at `x = 12` and is `16` wide (spans **12–28 px**). This causes the badge area to overlap the left half of the icon (**12–24 px**). Double-clicking this area triggers `isPointOnBadge` and prevents the directory from opening.
2.  **Detailed View (`FileTableDelegate.qml`):** The content shifts to `16` (`showSelectionBadges ? 16 : 4`), but the badge is at `x: 7` with a width of `16` (spans **7–23 px**), while the icon starts at `x = 16` (spans **16–32 px**). The left 7 pixels of the icon (**16–23 px**) are overlapped by the badge, blocking the double-click.

#### Recommended Solution
Adjust the left margin of the content when selection badges are enabled to prevent overlap:
*   In `FileDelegate.qml`: `anchors.leftMargin: showSelectionBadges ? 28 : 12`
*   In `FileTableDelegate.qml`: `anchors.leftMargin: showSelectionBadges ? 28 : 4`

---

### [BUG-003] PathBar Selector Transition Animation Lags on Windows

*   **Component:** Navigation Bar ([PathBar.qml](file:///c:/Users/tankr/Documents/FM/FMQml/qml/components/PathBar.qml))
*   **Platform:** Windows (smooth on Linux)
*   **Status:** Registered (pending research)

#### Description
Changing the active focus between the left and right panels (via mouse click, keyboard shortcuts, etc.) causes the transition animation of the active panel selector in the path bar (`PathBar`) to stutter/lag on Windows, whereas it runs smoothly on Linux.

#### Directions for Research
1.  **Qt Quick Graphics Backend on Windows:** Check the active graphics API on Windows (Direct3D 11/12, Vulkan, OpenGL, or Software). The backend setup might cause lag during state transitions.
2.  **UI Thread Block:** Switching panels might trigger synchronous I/O operations (e.g., querying drive space/status on Windows using slower Win32 APIs).
3.  **Binding Loops:** Check if changing panel focus triggers cascade updates or layout calculation loops in `PathBar.qml`.
