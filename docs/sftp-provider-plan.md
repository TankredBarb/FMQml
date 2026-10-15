# SFTP provider plan

Status: the Linux implementation includes the plugin, URLs, password login,
browsing, downloads, uploads, basic file actions and saved connections.

## Current implementation

- `fm_sftp_provider` builds when libssh is available and loads through the standard
  provider registry. Its form is in Settings → Files → Plugin settings → SFTP.
- The plugin owns the form, asynchronous login and SHA256 fingerprint confirmation.
  It uses `known_hosts`; a changed key blocks login before the password is sent.
- Authorized servers appear under SFTP in the shared Cloud section with a dedicated
  icon. Without active authorization, SFTP is hidden from Places; saved disconnected
  profiles remain available in the connection form. Disconnect removes the server
  from Places; disconnecting the last server also hides the SFTP root entry.
  Cloud, the context menu, unauthorized URLs and settings use the same form.
  Saved metadata includes name, address, port, login and initial folder. Optional
  passwords use the system Secret Service, separately from the settings file.
  Otherwise passwords remain only in memory for the current application run.
- Disconnect revokes authorization, including active requests, while retaining
  the profile and password-storage preference. Forget removes the profile and its
  password. Actual SSH sessions close after operations; there is no idle pool.
- Listing, hidden files, metadata, Unicode/URL encoding, symlink flags,
  generation/cancellation and connection limits are implemented. Workers open
  their own SSH sessions; QML does not perform network I/O.
- Enabled capabilities include `Transfer`, `Create`, `Rename`, `Remove` and
  `PreserveDestinationOnTransfer`. The last flag lets the queue retain an existing
  destination until a direct transfer commits, without changing the plugin virtual API.
- Manual creation supports New Folder, New File and New Text File, folder-creation
  shortcuts and the command palette. The created entry appears in the panel and
  immediately enters inline rename, matching local behavior. Name collisions use
  `New Folder (1)` / `New Text File (1).txt`, preserving the existing item.
  Creation and rename use the provider for the current URL. Accepting the default
  name unchanged with Enter succeeds without a rename request or a false
  "already exists" error. Creation, rename and download-for-open run in a worker;
  cancellation reaches the network client. Completion after navigation does not
  start inline rename in another folder. F2, context Rename, Ctrl+X and moves
  between panels use the controller's available operations.
- Downloads and uploads use the standard OperationQueue with 32 KiB blocks,
  without loading the entire file into memory. Progress, cancellation, conflicts,
  recursive folder copy and moves that delete the source after success are supported.
- Uploads keep up to 16 asynchronous WRITE requests outstanding (512 KiB in flight),
  instead of waiting for a response after every block. Progress counts acknowledged
  writes. All requests finish before fsync/close/rename; errors or cancellation do
  not publish the temporary file as the completed destination.
- Downloads use QSaveFile without direct-write fallback. Uploads create their own
  `.fm-upload-<uuid>.part` beside the destination and publish it with rename after
  writing, fsync where available, and successful close. Downloads also keep up to
  16 outstanding 32 KiB READ requests; short responses are completed without gaps
  or reordering. Cancellation, disconnect and source-size changes preserve the
  existing destination.
- Replacing an existing remote file requires `posix-rename@openssh.com` and
  libssh 0.12 or newer. Without the extension, the existing file is preserved and
  the operation reports an error.
- Folder/empty-file creation, rename and deletion work. Directory symlinks can be
  opened while retaining their symlink flag. Copying symlinks and special files
  is disabled; recursive deletion removes the link itself, never its target.
- Preview/Quick Look use the existing ProviderPreviewMaterializer with its 40 MiB
  limit and cleanup. Background thumbnails do not download full files. Ordinary
  opening downloads a file to separate staging and hands a read-only local copy
  to the standard opener; edits are not uploaded automatically. Cancelled or stale
  opens do not launch an external application. Preview caches are not reused
  across authorizations.
- Ordinary rwx permissions are copied for new files; replacement retains destination
  permissions. Ownership, ACLs, extended attributes and timestamps are not copied.
- Replace merges existing folders with new content. Atomicity applies to individual
  files, not a whole folder transaction. Cancellation leaves completed files copied.
- `fm`/`fm_sftp_provider` build successfully. `sftp_client_test` uses real loopback
  SSH/SFTP and a separate temporary known_hosts to check login, errors, key
  confirmation/changes, cancellation, the login controller, cache and stale listings.
- At the transfer stage, focused validation passed 15/15 client checks and 10/10
  checks of the real OperationQueue through the dynamically loaded plugin. Fixtures
  use the system OpenSSH sftp-server and a test password on loopback. Coverage
  includes SHA256, Unicode/spaces, empty files, permissions, Replace/Skip/Keep both,
  directories, successful/failed moves, write denial and cancellation both ways.
- Before profiles were added, the full build and CTest passed 85/85, with a clean
  `git diff --check`. Both SFTP tests passed again after the final Copy-availability
  change. Details and validation limits are in `docs/validation/sftp-transfers.md`.
- The form was checked offscreen using real FmButton/FmTextField controls and
  `org.kde.desktop`. This validates the component, not a manual laptop session.
- Universal QIODevice fallback through openRead/openWrite and the Windows build
  are deferred. Direct copyTo/FromLocalFile transfers work; unauthorized URLs
  explain where to log in through settings.

OperationQueueCopy preserves destinations for providers that guarantee safe
replacement and downloads directly through copyToLocalFile with QSaveFile.
Listing errors are not treated as empty folders. The watchdog receives a copy
of the worker cancellation callback rather than reading thread-local state.
If a temporary file cannot be removed after disconnect or cancellation, the
message includes its URL; a cancelled queue retains that message in statusMessage.
Losing the rename response reports an uncertain result: inspect the destination
before retrying; there is no automatic retry.

The user confirmed live login and transfers in both directions on the laptop.
Reconnecting through a saved profile/password still needs a live laptop check.
System Secret Service separately passed store/read/delete using a temporary test
entry, without accessing the user's password.

## Goal and agreed requirements

Open the old laptop with its existing SSH server directly in FMQml panels through
`sftp://`, browse folders, download files and upload them back. Uploading to the
server is mandatory for the first usable version.

The primary authentication method is **login and password**. User SSH keys,
ssh-agent and key-authentication setup are not required for this version.
Key authentication can be considered later as a separate task.

The initial validation assumes password authentication and the SFTP subsystem
are enabled on the laptop. A working SSH server alone does not guarantee either;
check both before the integration run.

## Existing project facilities

- `FileProviderPlugin` and `FileProviderPluginRegistry`: separate loadable provider
  plugins, with provider selection by URL scheme.
- `FileProvider`: browsing, metadata, creation, rename, deletion,
  `copyToLocalFile`, `copyFromLocalFile`, `openRead` and `openWrite`.
- `OperationQueue`: local ↔ provider copying, directory traversal, conflicts and
  progress. Direct transfers use `OperationQueueCopy.cpp`; separate batch paths
  are in `OperationQueueProviderTransfers.cpp`.
- `PathSemantics`: an unknown explicit scheme is already classified as a provider.
  Audit consumers of string types and lists of known schemes instead of adding
  `sftp://` checks throughout QML.
- `PluginSettingsUi`, `FileActionPlugin`, `PlacesProviderPlugin`: existing extension
  points for settings, actions and connections.
- MEGA/GDrive demonstrate secret storage through libsecret on Linux and Windows
  Credential Manager on Windows.
- The current FTP plugin is read-only and rejects URLs containing authentication.
  It is not a basis for the SSH transport.
- When this plan was prepared, the development machine exposed libssh 0.12.2 and
  libssh2 1.11.1 through `pkg-config`.

## Proposed design

Use a separate `fm_sftp_provider`, id `fm.sftp-provider`, under
`src/plugins/sftp/`. The libssh backend provides SSH password authentication,
server verification and the SFTP API. Do not parse directory listings through
shell commands or pass passwords in process arguments.

The first target is Linux FMQml with the real old laptop. Build the plugin when
its dependency is available. Validate installation and the Windows build
separately; a Linux run does not establish Windows support.

Networking, authentication, connection state and transfers belong in C++.
Plugin QML handles the connection form and state presentation. Do not create
a separate copy queue or file manager.

URLs use `sftp://user@host/path`, with an optional `:port`, defaulting to 22.
A URL without a username requests it in the connection form. Root `/` is the root
visible to that SFTP session, including chroot. Passwords are entered separately
and never stored in URLs, history, bookmarks or logs. In the first version,
reject embedded passwords before saving navigation. Check this ordering for the
address bar, history and session restoration.

Preserve case, spaces and backslashes in remote names. Use QUrl for URL encoding;
check Unicode, `%`, `#`, `?`, `@`, IPv6 and nonstandard ports separately.

The server SSH key is distinct from a user's authentication key. On first
connection, show the host, port and SHA256 fingerprint for user confirmation.
Check existing `known_hosts`; save a new key only after confirmation. A changed
key blocks login with a clear error and is never replaced automatically.
Send the password only after server verification.

By default, keep the password in memory until disconnect/application close.
Remember password is an explicit option backed by platform secret storage.
If that storage is unavailable, retain session login without writing the password
to configuration. Connection identity includes host, port and user; different
accounts and authorization changes must not share metadata or preview caches.

## Stages and acceptance criteria

### 1. Plugin and URLs

Add the build target, registration, URL parsing and file-path semantics.
Settings and connection components belong to the plugin. Use existing interfaces
without adding general architecture for a single consumer.

Validate plugin presence/absence, invalid URLs, parent/child paths and name
encoding. An unsupported `sftp://` URL must not become a local path.

### 2. Login and password authentication

The form contains host, port, login, password, initial folder and password storage.
Add fingerprint verification and asynchronous connection states. If needed,
support limited keyboard-interactive authentication with a password prompt;
arbitrary MFA is outside the MVP. Cancelling the dialog interrupts the request
and suppresses stale results. Operation-triggered authorization requests return
through the UI thread; workers never show dialogs or block the interface.

Validate correct/incorrect passwords, user rejection, unknown/changed keys,
unreachable servers, missing SFTP subsystems and two users on one host.
Secrets must not enter persisted public data.

### 3. Browsing and metadata

Use asynchronous listings with generation/cancellation, parent/child navigation,
hidden files, size, time, permissions and symlink flags. Do not share a libssh
session between concurrent threads: each worker owns its session and the total
number of connections is limited. Bound network waits, support cancellation
and shut workers down correctly.

Validate large folders, rapid navigation and network loss. Old listings must
not be published; application close must not hang.

### 4. Downloads and mandatory uploads

Connect `copyToLocalFile` and `copyFromLocalFile` to the standard OperationQueue.
Transfer single files, multiple selections and directories in both directions
through ordinary copy/paste actions and existing drag-and-drop. Do not buffer
whole files in RAM. Progress reflects transferred bytes; provide cancellation,
timeouts and clear permission, connectivity and disk-full errors.

Transfer to a temporary file beside the destination and publish the final name
after successful writing/close. Coordinate with the queue's existing `.part`
contract to avoid double staging. Check the server's rename/overwrite capabilities;
promise atomic replacement only when the operation is actually supported.
Without safe replacement, preserve the existing file and report an explicit error.
Do not delete the old file before an unprotected rename.

Cancellation removes only the operation's own temporary file. If a disconnect
prevents server cleanup, report the possible remainder and its path. Do not
automatically retry an unfinished write after connectivity returns.

Use standard overwrite/skip/rename conflict handling. Recursive traversal must
not follow symlinks; align copying links and their targets with existing queue
policy and validate it before enabling those operations. Moves between local
storage and SFTP remove the source only after successful copying, following
the queue's existing contract.

Validate SHA256 after transfers both ways, empty/large files, nested folders,
Unicode, conflicts, cancellation and mid-transfer disconnect. Failed transfers
must not damage an existing destination file.

### 5. File actions and integration

Support folder creation, rename and deletion with the existing confirmation.
Advertise capabilities only for implemented operations. Check read-only servers
and access restrictions. Saved connections are accessible in Places/bookmarks
without passwords in public paths; disconnect closes the session. Preview uses
the existing ProviderPreviewMaterializer and cleanup. Do not download full files
for background thumbnails.

Validate panel updates after actions, errors without false results and reconnect
after restart. Test password storage separately from login with an in-memory password.

### 6. Integration validation on the laptop

Supplement semantic/state tests with an isolated SSH/SFTP server covering
passwords, host keys, permission denial, cancellation, disconnect and integrity.
Then perform a manual FMQml session on the real laptop:
login → browse → download → upload → verify contents → cancel.

Explicitly build `fm` and `fm_sftp_provider`; run focused tests, provider/transfer
regressions, relevant general checks and `git diff --check`. Record automated
results separately from live-session results. A test server does not establish
the behavior of the specific laptop.

## Outside the first version

Key authentication, general MFA, tunnels/ProxyJump, SSHFS mounts, folder sync,
resume after disconnect, permission/chmod/chown settings, server-side
remote-to-remote copying and bulk parallelism optimization.

## Backend references

- https://api.libssh.org/stable/libssh_tutor_authentication.html
- https://api.libssh.org/stable/libssh_tutor_guided_tour.html

Stages 1–5 are implemented for Linux within the scope above. Automated completion
validation is recorded in `docs/validation/sftp-completion.md`. Reconnect through
a saved profile after restart and a live check of the new commands remain on the
real laptop; loopback tests do not establish the behavior of that specific server.
