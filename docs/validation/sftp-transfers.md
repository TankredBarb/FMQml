# SFTP transfers validation

The Linux provider uses password authentication and the existing OperationQueue
for local-to-SFTP uploads and SFTP-to-local downloads. No user SSH key is required.

## Original transfer-stage automated proof

These counts describe the initial transfer stage, before saved connections and
the pipelined-upload change below.

Final build: `cmake --build build -j12` passed. Full CTest run: **85/85 passed**
in 210.82 seconds, including both SFTP tests and the existing local-transfer,
navigation, provider, UI and shutdown regressions. `git diff --check` is clean.
After the final GUI capability-query fix, both SFTP CTest entries were rebuilt
and repeated successfully: 2/2 in 26.61 seconds. The remaining sources were unchanged.

`sftp_client_test`: 15 checks pass. The client/provider checks cover password
authentication and host-key verification, cancellation, stale scans, real file
uploads/downloads, SHA256 round trips, Unicode and whitespace in names, empty
files, private file permissions, special-file refusal, cancellation cleanup,
mid-upload disconnect and refusal to overwrite without atomic rename support.
The copy-capability query used by the GUI opens no SSH connections; source-type
and permission checks are performed by the transfer worker.

`fm --sftp-transfer-regression`: 10 checks pass. This loads the actual provider
module and exercises the actual OperationQueue, including Replace/Skip/Keep both,
recursive folder copy/merge, upload/download, successful moves, failed moves
preserving both files, a read-only OpenSSH backend, upload/download cancellation
and cancellation while estimating a directory before the transfer starts.

The fixture authenticates with a test password on loopback and bridges SFTP
packets to the system OpenSSH `sftp-server`. Each fixture uses temporary paths
and its own `known_hosts`; user credentials and user host-key files are untouched.
The fixture can disconnect during WRITE or omit advertised atomic-rename support.

The queue probe and its internal plugin action exist only with BUILD_TESTING on
Linux. They are registered as `sftp_transfer_regression` in CTest. They do not
appear in the product action list. Run from the checkout:

```sh
cmake --build build -j12
./build/sftp_client_test
QT_QPA_PLATFORM=offscreen ./build/fm --sftp-transfer-regression
ctest --test-dir build --output-on-failure
git diff --check
```

The tests need permission to create loopback sockets and an installed OpenSSH
`sftp-server`. Restricted filesystem-only sandboxes cannot run the network fixture.

## Pipelined upload, 2026-10-14

The user reported about 4 MB/s in FM and an average 12.3 MiB/s in Dolphin when
uploading the same video files (roughly 20–202 MB each) over Wi-Fi. Dolphin's
screenshot showed an instantaneous 19.0 MiB/s; the comparison baseline is its
final average, not that peak. An eight-packet ping measured 1.015 ms mean RTT
with no loss; this does not establish available bandwidth or disk throughput.

The old upload loop called synchronous `sftp_write()` for each 32 KiB block.
The replacement maintains at most 16 outstanding AIO writes (512 KiB of payload),
handles capped submissions without skipping source bytes, and advances progress
only after acknowledgements. All acknowledgements precede fsync/close/rename.
Failure/cancellation frees outstanding AIO handles before temporary-file cleanup.
The transfer/atomic-replacement and download policies are otherwise unchanged.
API reference: [libssh SFTP asynchronous I/O](https://api.libssh.org/stable/libssh_tutor_sftp_aio.html).

The new `pipelinedUpload` regression withholds the first write acknowledgement
for 50 ms. The old implementation sent one request and failed; the new one sent
16 requests before that acknowledgement and passed. It also verifies SHA256 of
a non-aligned payload with distinct block contents and monotonic acknowledged
progress. This is a deterministic concurrency/validity check, not a measurement
of Wi-Fi performance on the laptop.

The client and plugin rebuilt successfully. Four focused CTest entries passed
in 27.63 seconds: client (17 QtTest checks), real OperationQueue transfer probe
(10 checks), provider transfer engine and local file commit. These include
cancellation with writes outstanding, disconnect, atomic replacement refusal,
source preservation during failed moves and temporary-file cleanup.
`git diff --check` passed. Logs: `/tmp/fmqml-sftp-pipeline-before.log`,
`/tmp/fmqml-sftp-pipeline-tests.log`; detailed successful test output is in
`build/Testing/Temporary/LastTest.log` until the next CTest run.

The user subsequently confirmed that FM's upload speed on the actual laptop is
roughly comparable to Dolphin after the pipelining change. No exact new FM
average was supplied, so this is a live qualitative comparison, not a measured
claim of exactly 12.3 MiB/s.

## Completion stage

Downloads now also use a bounded 16-request AIO pipeline. Normal and deliberately
shortened DATA replies preserve byte order and SHA256; cancellation, disconnect
and a source growing/shrinking preserve the existing local destination. Actual
panel cut/paste/move and staged remote-to-remote copy are covered by the loaded
provider regression. See [SFTP completion](sftp-completion.md) for current test
counts and GUI/native-Wayland evidence.

## Limits of this proof

- Login and transfers in both directions on the old laptop were confirmed by
  the user. Drag-and-drop and the new saved-profile login were not separately
  confirmed by that report.
- File replacement uses `posix-rename@openssh.com` when replacing an existing
  remote file. The minimum client dependency is libssh 0.12. Its implementation
  selects that extension when advertised by the server:
  https://gitlab.com/libssh/libssh-mirror/-/raw/libssh-0.12.0/src/sftp.c
- A lost connection before commit preserves the old destination. A lost reply
  during rename can leave the final state unknown; the error requests verification
  before retrying. The source of a failed move remains available.
- Folder operations commit files individually and merge existing directories;
  they do not roll back earlier successful files on cancellation.
- Cleanup removes only the temporary file created by that upload. If the socket
  is no longer usable, the error/status identifies a possible remote remainder.
- Symbolic links and special files cannot be copied in this slice. Directory
  deletion does not traverse symbolic links. Owner/ACL/xattr/timestamp preservation
  and universal QIODevice fallback are outside the current transfer validation.
