# SFTP completion, 2026-10-14

This completes the remaining Linux first-version commands and preview paths.
User SSH keys are not required. The actual laptop is powered off, so new
network checks use temporary password-authenticated loopback SSH/OpenSSH SFTP.

F2, context Rename, Ctrl+X, cut/paste and move to the opposite panel use native
controller capabilities. Other services retain their existing restrictions.
Multi-item batch rename remains local; the local BatchRenameEngine cannot rename
SFTP entries. SFTP supports ordinary single-item rename and provider transfers.

Create/rename and download-for-open run in a worker with the existing thread
abort checker installed for the operation and restored afterward. The GUI keeps
running during delayed server replies. A controller accepts one remote mutation
at a time. Navigation generations suppress stale entry-created/renamed UI events;
closing the controller cancels without waiting on its remote worker. Cancellation
does not roll back an operation the server has already performed.
The panel footer exposes Cancel during these operations, using the same native
cancellation token; inline rename cancellation also reaches the worker.

Downloads maintain at most 16 outstanding 32 KiB AIO reads. Short reads fetch
the missing range before later replies are consumed. A tail probe rejects a
source that grows; EOF before the expected size rejects a shrinking source.
QSaveFile preserves an existing destination on cancellation, disconnect or size
change. This preserves the existing atomic download contract.

Directory symlinks retain their symlink flag while following metadata for
navigation. Broken links and loops remain visible. Recursive copy refuses links;
recursive delete unlinks them and preserves their targets. Explicitly selecting
a directory reached through a link as a transfer destination is allowed.

Preview/Quick Look materialize through the existing provider loader and cleanup,
with its 40 MiB limit. Background thumbnail requests do not download SFTP files.
No shared preview cache identity is supplied for SFTP. Disconnecting authorization
prevents reuse of a previous preview on a subsequent request.

Opening a regular SFTP file downloads it into a separately leased staging
directory and hands a read-only local copy to the normal opener. It does not
upload edits back automatically. Explicit opening is not subject to the preview
size limit; it streams to disk. Cancellation or navigation suppresses launch and
schedules staging cleanup. Successful launch retains the lease because the
consumer may still use it; startup cleanup handles leases from dead processes.

## Automated checks

`sftp_client_test` uses a real loopback backend, validates SHA256 and acknowledged
progress, and delays the first READ/WRITE response to prove multiple outstanding
requests. It also injects a short first DATA packet, download connection loss,
cancellation, shrinking/growing sources, broken/looping links, and recursive
deletion with an external target that must survive.

`sftp_transfer_regression` loads the actual provider module and FM controllers.
It checks delayed mutation replies with a GUI heartbeat, unchanged/default and
changed names, permission errors, cancellation, navigation and controller close,
text/image preview, the preview size guard, and authorization revocation. A
QDesktopServices URL handler records external launch without starting an editor
and checks staged bytes and read-only permissions. Cancelled/stale opens must
never reach that handler. It also checks cut/paste, panel move, remote-to-remote
staged copy/rename, symlink navigation and non-following recursive copy/delete.

`sftp_creation_ui_test` exercises the actual menus, inline editors, F2 and Ctrl+X
in Details/Grid/Brief against both SFTP and a local temporary folder. It confirms
default-name Enter without a false collision, numbered duplicates and changed
names. Separate connection-form and Places UI regressions cover shared settings,
authorization-dependent visibility and sidebar scroll preservation.
The real GUI test also clicks the footer Cancel button during delayed creation
and verifies that no inline rename starts afterward.

Full build and the complete CTest suite passed: **93/93 in 288.57 seconds**
(`/tmp/fmqml-sftp-completion-full.log`). After the final footer Cancel and
explicit-path move-capability changes, all seven affected SFTP/Places/hover/
navigation GUI regressions passed again: **7/7 in 101.25 seconds**
(`/tmp/fmqml-sftp-completion-final-focused.log`). The actual-plugin probe passes
18 QtTest checks; the standalone client passes 25, including 16 READ requests
before the first reply for both normal and shortened packets, and 16 WRITE
requests before the first acknowledgement. `git diff --check` is clean.

The same creation/Cancel/default Enter/F2/Ctrl+X workflow also passed on native
Wayland with `org.kde.desktop` and an isolated profile
(`/tmp/fmqml-sftp-completion-wayland.log`). The test explicitly activates its
window before checking application shortcuts; sending key events to an inactive
Wayland window does not exercise those shortcuts. The final harness also passed
offscreen again in 15.25 seconds (`/tmp/fmqml-sftp-completion-ui.log`).

## Remaining live checks and scope

Saved-login state and reconnect are tested with an isolated credential-store
fixture. Production Secret Service store/read/delete was separately probed in
the earlier saved-connections stage. Actual laptop restart/reconnect, the new
commands and real download throughput need a live check when it is powered on.
The user has already confirmed upload/download and upload speed comparable to
Dolphin after the earlier WRITE pipeline fix.

Windows packaging/build, universal QIODevice openRead/openWrite, copying links,
remote editing with writeback, chmod/chown, resumable transfers and server-side
remote-to-remote copy are outside this Linux first version.
