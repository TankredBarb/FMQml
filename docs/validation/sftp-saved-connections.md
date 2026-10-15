# SFTP saved connections and Cloud UI

Linux implementation, 2026-10-14.

- SFTP uses the same Cloud group as the web storage providers and a distinct
  terminal/lock icon. Its root and server entries appear only while at least one
  account has active authorization and in-memory credentials. Disconnect removes
  that server from Places; the last disconnect also removes the SFTP root.
  Saved profiles remain in the connection form after disconnect and application
  restart. Their name, URL, port, login and initial folder are stored in QSettings
  without passwords. A stored password alone does not add a Places entry.
- The plugin-owned connection form is shared by Settings and the existing
  plugin UI host. Opening an unauthenticated `sftp://` path from the file panel
  opens that form. Saved entries support Connect/Open, Edit, Disconnect and
  Forget. Host-key confirmation still precedes sending a password.
- Remember password is optional. Production storage uses libsecret/Secret
  Service, keyed by the normalized account/host/port identity. All secret
  operations run on workers with cancellation and a 30-second request deadline;
  GUI profile reads do not take the secret-operation mutex.
- Disconnect revokes session credentials and active workers, retaining the
  saved profile/password. Forget removes the profile and its stored password.
  Actual SSH/SFTP sessions already close at the end of each operation; retaining
  a signed-in profile does not keep an idle SSH socket open.
- Closing either host clears the password field and cancels unfinished trust
  confirmation. A password-storage error leaves the connection usable in memory
  and reports that the requested settings were not saved.
- Authentication errors appear beside Connect and in the fixed dialog footer,
  using the error color. They remain visible when the form is scrolled. Starting
  another attempt clears the previous error state.

## Evidence

The standalone client test uses isolated QSettings and an in-memory credential
backend, never the user's keyring. The saved-connection check verifies profile
metadata, no password in the settings file, reconnect through a new controller,
account isolation, cancellation preserving the old secret, disabling password
storage, disconnect retaining the profile and Forget removing it. A wrong
password on the loopback SSH server sets the controller's visible error state
without emitting a successful connection or retaining authorization.

`sftp_connections_ui_test` loads the real plugin through the real application,
opens an unauthenticated URL through FilePanel, checks form geometry and error
propagation into the form and dialog footer, then fills and closes the password
field and verifies it was cleared. Upload/download and
the real OperationQueue remain covered by the existing SFTP transfer regression.

The full application build and all 91 CTest checks passed (246.00 seconds) before
the final authentication-error presentation change. After that change, the
client, connection UI, transfer and navigation smoke checks passed again: 4/4,
39.85 seconds (`/tmp/fmqml-sftp-error-tests.log`).

The production Secret Service backend separately passed store/read/delete of a
random temporary identity and secret. The probe removed its own entry; no user
password was read. Source/binary: `/tmp/fmqml-sftp-secret-probe/`.

The connection form was rendered and inspected on native Wayland with GPU
rendering. Software/offscreen runs establish component/input behavior only.
Initial screenshot: `/tmp/fmqml-sftp-connections-wayland.png` (before preserving
the multicolor header icon and checking Save connection by default).

The user previously confirmed transfers to/from the old laptop. Reconnecting
that laptop after restarting FM using a remembered password still needs the
user's live check; automated password authentication uses a loopback SSH server.
