# SFTP manual creation and local behavior parity

2026-10-14.

UI policies hid manual creation for every provider path, independently of
`Create` support. The panel's creation and rename methods also used its local
filesystem utility provider rather than the provider selected for the URL.

`FilePanelController.canCreateManualItem` now supplies one native policy for the
empty-space context menu, folder shortcut route and command palette. It enables
local paths and authorized SFTP paths with creation support, preserving the
existing restrictions for other remote services. Creation availability for SFTP
checks in-memory credentials without opening SSH connections. Mutations and
their parent/child/name handling use the provider selected by the actual URL.

Manual SFTP creation trims names like local creation and chooses numbered names
when needed, preserving a file's last extension. Candidate checks and creation
use one SSH session. Files use exclusive creation and folders use a single
mkdir, so a collision does not overwrite or adopt an existing object. Recursive
parent creation used by transfers keeps its existing separate path.

The backend test checks folder suffixes, Unicode file names, preservation of an
existing file's contents, empty created files, invalid/escaping names, and
creation availability before login and after disconnect. The SFTP client and
OperationQueue transfer regressions passed after these backend changes.

The user's default-name report was separately reproduced: create New Folder,
then press Return without changing its name. The folder existed successfully,
but the attempted rename to the same destination reported
`The SFTP destination already exists.` The SFTP provider now treats an unchanged
normalized destination as success, clears stale errors and opens no SSH session
for that operation, matching the local provider's behavior. Backend checks cover
folders, files with an extension, no extra authentication, and clearing an older
error. The GUI test confirms unchanged default folder/file names through Return
in all three views, then checks that repeated creation chooses numbered names.

`sftp_creation_ui_test` runs the actual app and plugin against a temporary
loopback SSH/OpenSSH SFTP server with an isolated profile and known_hosts.
It checks visible/enabled New Folder, New File and New Text File menu items,
the folder command used by shortcuts, created objects on the remote filesystem,
automatic inline rename in Details/Grid/Brief, and entering a new filename and
pressing Return to commit the rename. It repeats creation and inline rename
against a local temporary folder, including numbered names, for comparison.

The GUI test reproduced the hidden New Folder action, then passed in 8.21
seconds after fixing policy and routing. An intermediate test diagnostic also
showed why checking the local utility provider's scheme was wrong:
`create=1 manual=0` on an authorized SFTP path.

Logs: `/tmp/fmqml-sftp-create-before.log`,
`/tmp/fmqml-sftp-create-ui-tests.log`, `/tmp/fmqml-sftp-create-tests.log`.
The default-name failure is recorded in `/tmp/fmqml-sftp-default-name-before.log`.
After the correction, the same creation/default-name/inline-rename workflow
passed on native Wayland (`/tmp/fmqml-sftp-create-wayland.log`) using the temporary
loopback server. Navigation and selection regressions also passed after routing
changes: 2/2 in 10.87 seconds (`/tmp/fmqml-sftp-create-routing-tests.log`).
No laptop files or user credentials are used by these tests.

After the default-name fix, all three focused SFTP checks passed in 37.16
seconds: creation UI, standalone client and actual OperationQueue transfers
(`/tmp/fmqml-sftp-default-name-tests.log`). The application/plugin/client were
rebuilt, and `git diff --check` passed.

The completion stage moves SFTP create/rename off the GUI thread and extends this
GUI regression with actual F2/Ctrl+X, visible Rename/Cut item-menu actions and a
click on Cancel during pending creation. The updated workflow passes both
offscreen and native Wayland. See [SFTP completion](sftp-completion.md) for the
final build, full-suite and focused regression evidence.
