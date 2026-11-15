#!/usr/bin/env python3
"""Root-only integration test on a private bus; runs all clients as the user.

No system service, policy, mount or filesystem configuration is installed.
Fixtures and the private bus are removed afterward. Use --user explicitly.
"""

import argparse
import fcntl
import json
import os
from pathlib import Path
import pwd
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--user", required=True)
    parser.add_argument("--build", type=Path, default=Path("build"))
    parser.add_argument("--report", type=Path, default=Path("/tmp/fm-space-helper-proof.json"))
    args = parser.parse_args()
    if os.geteuid() != 0:
        raise SystemExit("Root is required only for the metadata service in this test")
    account = pwd.getpwnam(args.user)
    if account.pw_uid == 0:
        raise SystemExit("The test client must be unprivileged")
    build = args.build.resolve()
    executable = str(build / "file_space_usage_test")
    report = {"clientUid": account.pw_uid, "serviceUid": os.geteuid(), "cases": {}}

    def drop_privileges():
        os.initgroups(account.pw_name, account.pw_gid)
        os.setgid(account.pw_gid)
        os.setuid(account.pw_uid)

    if subprocess.check_output(["findmnt", "-T", str(build), "-no", "FSTYPE"], text=True).strip() != "btrfs":
        raise SystemExit("The integration fixtures require a Btrfs build directory with compression enabled")
    with tempfile.TemporaryDirectory(prefix="fm-space-helper-", dir=build) as temporary:
        directory = Path(temporary)
        directory.chmod(0o755)
        config = directory / "bus.conf"
        config.write_text(f'''<busconfig><type>system</type>
<listen>unix:path={directory}/bus</listen><auth>EXTERNAL</auth>
<policy context="default"><allow user="*"/><allow send_destination="*"/><allow receive_sender="*"/></policy>
<policy user="root"><allow own="io.fmqml.fm.Space"/></policy>
</busconfig>''')
        bus = subprocess.Popen(["dbus-daemon", "--nofork", "--config-file=" + str(config), "--print-address=1"],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        helper = None
        try:
            address = bus.stdout.readline().strip()
            if not address:
                raise RuntimeError("Private bus did not start: " + bus.stderr.read())
            environment = {**os.environ, "DBUS_SYSTEM_BUS_ADDRESS": address,
                           "HOME": account.pw_dir, "USER": account.pw_name, "LOGNAME": account.pw_name}
            helper = subprocess.Popen([str(build / "fm-space-helper")], env=environment,
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

            def client(*arguments, expect_json=True):
                completed = subprocess.run([executable, *map(str, arguments)], cwd=build, env=environment,
                                           preexec_fn=drop_privileges, text=True, capture_output=True, timeout=30)
                if completed.returncode:
                    raise RuntimeError(f"Client failed {arguments}: {completed.stdout} {completed.stderr}")
                return json.loads(completed.stdout) if expect_json else completed.stdout.strip()

            # Root-owned, readable fixture. The client itself must open it before
            # the service can receive its descriptor.
            compressed = directory / "compressed"
            with compressed.open("wb") as file:
                for _ in range(64):
                    file.write(b"x" * 1024**2)
                file.flush()
                os.fsync(file.fileno())
            compressed.chmod(0o644)
            for _ in range(100):
                first = client("--inspect", compressed)
                if first["bytes"] >= 0:
                    break
                if helper.poll() is not None:
                    raise RuntimeError("Helper exited: " + helper.stderr.read())
                time.sleep(0.02)
            if first["bytes"] != 2 * 1024**2:
                raise RuntimeError("Incorrect compressed size: " + repr(first))
            report["cases"]["compressed64MiB"] = first
            report["unitTests"] = client(expect_json=False)
            report["benchmark"] = client("--bench", compressed)

            reflink = directory / "reflink"
            with compressed.open("rb") as source, reflink.open("wb") as destination:
                fcntl.ioctl(destination.fileno(), 0x40049409, source.fileno())
                os.fsync(destination.fileno())
            reflink.chmod(0o644)
            together = client("--inspect", compressed, reflink)
            if together["bytes"] != first["bytes"]:
                raise RuntimeError("Reflinks counted twice: " + repr(together))
            report["cases"]["reflinksTogether"] = together

            with compressed.open("r+b") as file:
                file.seek(256 * 1024)
                file.write(os.urandom(4096))
                file.flush()
                os.fsync(file.fileno())
            overwritten = client("--inspect", compressed, reflink)
            if overwritten["bytes"] != first["bytes"] + 4096:
                raise RuntimeError("Incorrect partial compressed overwrite: " + repr(overwritten))
            report["cases"]["partialCompressedOverwrite"] = overwritten

            inline = directory / "inline"
            with inline.open("wb") as file:
                file.write(b"x" * 100)
                file.flush()
                os.fsync(file.fileno())
            inline.chmod(0o644)
            inline_usage = client("--inspect", inline)
            if not 0 < inline_usage["bytes"] <= 100:
                raise RuntimeError("Incorrect inline payload accounting: " + repr(inline_usage))
            report["cases"]["inlinePayload"] = inline_usage

            protected = directory / "protected"
            protected.write_text("root-only contents must stay unreadable")
            protected.chmod(0o600)
            report["security"] = client("--helper-security", protected)
            if not report["security"]["success"]:
                raise RuntimeError("Descriptor permission checks failed")
            if protected.read_text() != "root-only contents must stay unreadable":
                raise RuntimeError("Security fixture changed")

            denied_stop = subprocess.run([str(build / "fm-space-helper"), "--stop-running"],
                                         env=environment, preexec_fn=drop_privileges, capture_output=True)
            if denied_stop.returncode == 0 or helper.poll() is not None:
                raise RuntimeError("Unprivileged clients must not stop the service")
            subprocess.run([str(build / "fm-space-helper"), "--stop-running"],
                           env=environment, check=True, timeout=5)
            helper.wait(timeout=5)
            report["maintenanceStop"] = {"userDenied": True, "rootSucceeded": True}
            missing = client("--inspect", compressed)
            if missing["bytes"] >= 0:
                raise RuntimeError("Absent helper must not produce a guessed number")
            report["missingService"] = missing
        finally:
            for process in (helper, bus):
                if process and process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)
    report["fixturesRemoved"] = not directory.exists()
    args.report.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
