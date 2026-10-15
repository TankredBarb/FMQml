# SFTP review fixes, 2026-10-15

Two review findings are covered by automated regressions.

SSH connection establishment now uses nonblocking `ssh_connect` with a 25 ms
socket wait and cancellation checks. The existing ten-second connection deadline
remains; authentication and file operations retain their blocking worker and
socket watchdog. A loopback SSH fixture can accept TCP without responding to the
handshake. Before the fix, `handshakeCancellation` failed after 10,015 ms. It now
requires cancellation within one second. `closeProviderDuringHandshake` checks
that destroying a scanning provider also returns within one second.

The queue now selects the preservation guarantee of the provider performing the
final write: the source for a direct download to a local file, and the destination
for an upload, including a staged remote-to-remote transfer. A source's safe
download no longer suppresses destination removal for other cloud providers.
The same policy is used by Copy and Move. Existing SFTP replacement protection
remains covered by cancellation and failure tests.

`replaceOnOtherProvider` exercises the real queue and a loopback SFTP source with
a test cloud destination that requires a new item path, matching Google Drive's
upload contract. Both Copy and Move failed before the fix. Both now verify one
destination removal, the uploaded bytes, and preservation/removal of the source
as appropriate. The test accesses the queue's provider cache through a private
test friend; no production provider registration or account credentials are used.

Build passed without compiler warnings. The two affected CTest targets passed
in 48.44 seconds (`/tmp/fmqml-review-fixes-focused.log`). The initial failing
regressions are recorded in `/tmp/fmqml-handshake-before.log` and
`/tmp/fmqml-cloud-replace-before.log`.

The remaining 91 CTest targets passed in 250.92 seconds
(`/tmp/fmqml-review-fixes-suite.log`), including scrolling, hover previews, resize,
Places, connection UI and other providers. Together with the two focused targets,
the complete set passed: **93/93**. `git diff --check` is clean. No live Google
Drive account or physical laptop was used for this validation.
