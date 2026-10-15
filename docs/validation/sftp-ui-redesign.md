# SFTP connection UI redesign, 2026-10-14

The previous card mixed saved servers, four actions per server and a flat form
inside one settings block. Several labels and checkboxes inherited the system
font, while FM fields used Theme typography.

The shared SFTP card now separates Connections from the connection editor.
Saved/active profiles use selectable rows with a name, server address, folder
and connection state; actions apply to the selected profile. New connection
resets only the editor. Host/Port, Login/Password and Folder/Name form three
aligned rows. Save options, server trust, status and submit actions have their
own space below. All text uses Theme.fontFamily and a small consistent set of
Theme font sizes. Existing FM buttons, fields and checkboxes retain their native
visuals. Profile rows support Tab focus and Enter/Space selection.

At widths below 680 px the Connections section stacks above the editor. The
saved list has a bounded scrolling area, so adding servers does not make a long
sequence of repeated action buttons. The normal Cloud form fits its fields and
Connect action without scrolling. The same component remains embedded in
Files settings, with its SFTP heading shown there rather than repeated under
the Cloud dialog's SFTP header.

SftpConnectionController, authentication, host trust, saved-profile storage,
password storage and transfer code are unchanged. Existing connection/open/
disconnect/forget/sign-in/cancel callbacks and sensitive-input clearing remain.

The actual-app `sftp_connections_ui_test` now checks:

- Initial URL population and visible error reporting in both card and dialog.
- A saved, authenticated profile from a temporary real loopback SSH fixture.
- Separate panels and field bounds/font consistency at 888 and 440 px widths.
- Clicking New connection, selecting a profile row and Disconnect; the saved
  profile remains, while its action changes from Open to Connect.
- Availability of Forget and clearing password input on Cloud dialog close.
- Loading the shared card in Files settings and clearing its password on close.

Dynamic profile rows and settings-provider delegates are traversed through the
Qt Quick visual tree; their QObject ownership tree does not reliably follow
visual parenting.

Build passed, and all three focused CTest entries passed in 26.94 seconds:
connection UI, Places sidebar UI and the 25-check standalone SFTP client suite.
Log: `/tmp/fmqml-sftp-redesign-tests.log`. The complete UI flow also passed in
native Wayland with `org.kde.desktop` and an isolated profile:
`/tmp/fmqml-sftp-redesign-wayland.log`.

Visual capture: `/tmp/fmqml-sftp-redesign-wayland.png`. Its red message is the
deliberately generated invalid-address feedback tested before the saved-profile
steps. It uses the test profile's light theme; the actual user's palette remains
controlled by Theme. No user passwords or laptop files are used by these checks.

## Compact height and theme surfaces, 2026-10-15

StorageView's Cloud cards previously resolved `sftp` through the generic
`assets/icons/sftp.svg` fallback, which does not exist. They now use the plugin
resource `qrc:/sftp/settings/sftp.svg`. QuickAccessCard preserves this multi-color
icon, matching the Sidebar and SFTP dialog header rather than flattening its
layers through recoloring.

The SFTP wrapper reports its content's preferred height. PluginUiDialog uses
that optional hint plus its header/footer heights, capped to the viewport; other
plugins retain the original maximum-height behavior. Long lists and narrow
layouts still scroll when their content exceeds the cap. Connections/editor
fills and borders now use the same panelSurfaceStrong/panelBorder alpha tokens
as SettingsDialog sections, with separate light/dark values.

The GUI regression checks the actual unused vertical space, resource existence,
and IconTile Image.Ready both through the SVG recolor provider and with original
colors. It also compares the rendered section color/border properties against
SettingsDialog in Light and Dark modes for CatppuccinLatte and GraphiteSage,
in both the Cloud and Settings instances.

Build and diff check passed. Connection UI and Places UI: **2/2 in 19.46 s**,
`/tmp/fmqml-sftp-surface-tests.log`. Native Wayland also passed:
`/tmp/fmqml-sftp-surface-wayland.log`; capture:
`/tmp/fmqml-sftp-surface-wayland.png`.
