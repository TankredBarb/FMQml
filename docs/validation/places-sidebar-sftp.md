# Places: one Cloud group and stable scrolling

2026-10-14, Qt 6.12.0, Linux.

The original GUI regression reproduced both reports:

```text
cloudHeaders=2 periodicResets=1 idleScroll=754.0/0.0 refreshScroll=754.0/0.0
```

SFTP provider rows were appended after local folders, separate from the built-in
Cloud providers. Provider-place signatures also differed between `refresh()`
and the asynchronous poll: only the former appended `canUnmount`. Every
five-second poll therefore reset an unchanged model and the ListView scroll.

Cloud provider rows now share one contiguous group. Both refresh paths use the
same signature. Existing rows update through `dataChanged` instead of a model
reset; topology changes preserve the ListView's offset relative to its origin,
then apply the normal bounds when the list becomes shorter. Plugin refresh
requests also schedule asynchronous provider-place collection immediately.

SFTP Places entries require active in-memory authorization/credentials. Stored
profile metadata or a remembered password alone does not show SFTP. Disconnect
removes that server; the last disconnect also removes the root entry. Saved
profiles remain available in the connection form.

`places_sidebar_ui_test` runs the actual application with isolated XDG paths,
12 temporary profiles and test-only in-memory session fixtures in the plugin.
It verifies a single Cloud header, hidden disconnected profiles, no model reset
during a full polling interval, stable top/bottom positions during refresh, and
removing a disconnected account without losing the scrolled position. This GUI
fixture does not claim network authentication; the standalone client test uses
a real loopback SSH server to check the Places login/disconnect/reconnect state,
including a remembered password without active credentials.

The regression passed offscreen and on native Wayland:

```text
cloudHeaders=1 periodicResets=0 idleScroll=754.0/754.0
refreshScroll=754.0/754.0 topScroll=0.0/0.0
disconnectedHidden=1 disconnectScroll=377.0/377.0 success=1
```

Logs: `/tmp/fmqml-places-before.log`,
`/tmp/fmqml-places-auth-tests.log`, `/tmp/fmqml-places-wayland-test.log`.
The focused application checks also passed for SFTP UI, navigation, plugin
settings, Linux devices and MEGA. The sandbox denied the SSH fixture its local
listening socket; that client check and the transfer regression were rerun with
loopback access and both passed in 27.20 seconds
(`/tmp/fmqml-places-auth-network-tests.log`). In total, all eight distinct
focused checks passed, plus the native Wayland GUI run. The application, SFTP
plugin and standalone client test were rebuilt; `git diff --check` passed.
