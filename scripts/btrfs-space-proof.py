#!/usr/bin/env python3
"""Controlled Btrfs allocation/deletion experiment; touches only its own fixtures.

Run outside the sandbox on the target filesystem, for example:
  python3 scripts/btrfs-space-proof.py --parent /home/tankred --report /tmp/btrfs-proof.json
No root, snapshots, mount changes, defrag, or filesystem-wide cleanup required.
Filesystem counters include background activity; they are observations, not
per-file attribution. st_blocks and FIEMAP lengths are recorded as raw evidence.
Running as root additionally verifies predictions from compressed extent sizes
and all referencing inodes. It still modifies only its own fixtures.
"""

import argparse
import ctypes
import datetime
import fcntl
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import time


MIB = 1024 * 1024
MAX_U64 = (1 << 64) - 1
libc = ctypes.CDLL(None, use_errno=True)
libc.syncfs.argtypes = [ctypes.c_int]
libc.syncfs.restype = ctypes.c_int


def settle(directory):
    fd = os.open(directory, os.O_RDONLY | os.O_DIRECTORY)
    try:
        if libc.syncfs(fd):
            raise OSError(ctypes.get_errno(), "syncfs failed")
    finally:
        os.close(fd)


def counters(directory):
    settle(directory)
    usage = subprocess.run(
        ["btrfs", "filesystem", "usage", "-b", str(directory)],
        text=True, capture_output=True, check=True,
        env={**os.environ, "LC_ALL": "C"},
    )
    data_used = sum(int(value) for value in re.findall(
        r"^Data(?:\+Metadata)?,[^\n]*Used:\s*(\d+)", usage.stdout, re.MULTILINE))
    if not data_used:
        raise RuntimeError("Cannot parse Btrfs data usage: " + usage.stdout)
    fs = os.statvfs(directory)
    return {"data_used": data_used, "available": fs.f_bavail * fs.f_frsize}


def inspect(path):
    stat = path.stat()
    result = {"logical": stat.st_size, "stat_blocks_bytes": stat.st_blocks * 512,
              "links": stat.st_nlink, "inode": stat.st_ino}
    with path.open("rb") as file:
        # Linux FS_IOC_FIEMAP, batches of 128 extents, no sync side effect.
        start = 0
        extents = []
        while True:
            buffer = bytearray(32 + 56 * 128)
            struct.pack_into("=QQIIII", buffer, 0, start, MAX_U64 - start, 0, 0, 128, 0)
            fcntl.ioctl(file.fileno(), 0xC020660B, buffer, True)
            count = struct.unpack_from("=I", buffer, 20)[0]
            for index in range(count):
                logical, physical, length, _, _, flags, _, _, _ = struct.unpack_from(
                    "=QQQQQIIII", buffer, 32 + index * 56)
                extents.append((logical, physical, length, flags))
            if not count or extents[-1][3] & 1:
                break
            next_start = extents[-1][0] + extents[-1][2]
            if next_start <= start:
                raise RuntimeError("FIEMAP did not advance")
            start = next_start
        result.update({"extent_count": len(extents),
                       "fiemap_length_sum": sum(e[2] for e in extents),
                       "encoded": any(e[3] & 8 for e in extents),
                       "shared": any(e[3] & 0x2000 for e in extents)})
        # Probe whether exact compressed extent sizes are accessible without root.
        buffer = bytearray(4096)
        struct.pack_into("=7Q4I4Q", buffer, 0, 0, stat.st_ino, stat.st_ino,
                         0, MAX_U64, 0, MAX_U64, 108, 108, 1, 0, 0, 0, 0, 0)
        try:
            fcntl.ioctl(file.fileno(), 0xD0009411, buffer, True)
            result["tree_search"] = "available"
        except OSError as error:
            result["tree_search"] = {"errno": error.errno, "message": error.strerror}
    return result


def extent_prediction(paths, measurements):
    """Prototype, whole regular files on one Btrfs filesystem, data profile single.

    Counts each full physical extent once, including compressed extents and
    unused tails. Backreferences include other subvolumes/snapshots. Metadata
    and inline payloads are reported separately, not promised as freed blocks.
    Not a transaction: concurrent writes/snapshots/open handles remain relevant.
    """
    extents = {}
    selected = {}
    inline_bytes = 0
    with paths[0].open("rb") as first:
        for path in paths:
            stat = path.stat()
            with path.open("rb") as file:
                lookup = bytearray(4096)
                struct.pack_into("=QQ", lookup, 0, 0, 256)
                fcntl.ioctl(file.fileno(), 0xD0009412, lookup, True)  # INO_LOOKUP
                root = struct.unpack_from("=Q", lookup)[0]
                identity = (root, stat.st_ino)
                if identity in selected:
                    selected[identity][0] += 1
                    continue
                selected[identity] = [1, stat.st_nlink]
                start = 0
                while True:
                    buffer = bytearray(4096)
                    struct.pack_into("=7Q4I4Q", buffer, 0, 0, stat.st_ino, stat.st_ino,
                                     start, MAX_U64, 0, MAX_U64, 108, 108, 100, 0, 0, 0, 0, 0)
                    fcntl.ioctl(file.fileno(), 0xD0009411, buffer, True)  # TREE_SEARCH
                    count = struct.unpack_from("=I", buffer, 64)[0]
                    if not count:
                        break
                    offset = 104
                    for _ in range(count):
                        _, inode, key_offset, item_type, length = struct.unpack_from("=QQQII", buffer, offset)
                        offset += 32
                        if inode != stat.st_ino or item_type != 108 or length < 21 or offset + length > len(buffer):
                            raise RuntimeError("Unexpected extent search result")
                        kind = buffer[offset + 20]
                        if kind == 0:
                            inline_bytes += length - 21
                        elif kind in (1, 2) and length >= 53:
                            bytenr, disk_size = struct.unpack_from("<QQ", buffer, offset + 21)
                            if bytenr:
                                extents[bytenr] = disk_size
                        else:
                            raise RuntimeError("Unsupported extent item")
                        offset += length
                    if key_offset == MAX_U64:
                        break
                    start = key_offset + 1

        selected_complete = {key for key, (count, links) in selected.items() if count == links}
        reclaimable = 0
        retained = 0
        shared = any(value["shared"] for value in measurements.values())
        for bytenr, size in extents.items():
            if shared:
                # LOGICAL_INO_V2 with IGNORE_OFFSET returns *all* extent refs,
                # including refs to other portions of a partially reused extent.
                output = ctypes.create_string_buffer(65536)
                request = bytearray(struct.pack("=7Q", bytenr, len(output), 0, 0, 0, 1, ctypes.addressof(output)))
                fcntl.ioctl(first.fileno(), 0xC038943B, request, True)
                _, missing_bytes, count, missed = struct.unpack_from("=4I", output.raw)
                if missing_bytes or missed or count % 3 or not count:
                    raise RuntimeError("Incomplete Btrfs backreference result")
                owners = set()
                for index in range(0, count, 3):
                    inode, _, root = struct.unpack_from("=3Q", output.raw, 16 + index * 8)
                    owners.add((root, inode))
                released = owners <= selected_complete
            else:
                # No shared extents, but external hardlinks can still retain data.
                released = len(selected_complete) == len(selected)
            if released:
                reclaimable += size
            else:
                retained += size
        return {"extent_data_bytes": sum(extents.values()), "inline_payload_bytes": inline_bytes,
                "reclaimable_data_bytes": reclaimable, "retained_data_bytes": retained}


def write_fixture(path, kind, size):
    with path.open("xb") as file:
        if kind == "sparse":
            file.truncate(size)
            file.write(b"fixture\n")
        else:
            for _ in range(size // MIB):
                file.write(os.urandom(MIB) if kind == "random" else b"Btrfs proof\n" * (MIB // 12) + b"!" * (MIB % 12))
        file.flush()
        os.fsync(file.fileno())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--extent-metadata", action="store_true",
                        help="Require privileged Btrfs extent/backreference inspection")
    args = parser.parse_args()
    fs_type = subprocess.check_output(
        ["findmnt", "-T", str(args.parent), "-no", "FSTYPE"], text=True).strip()
    if fs_type != "btrfs":
        raise SystemExit("Target is not Btrfs: " + fs_type)
    if os.statvfs(args.parent).f_bavail * os.statvfs(args.parent).f_frsize < 2 * 1024**3:
        raise SystemExit("At least 2 GiB available required")
    report = {"time": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "mount": subprocess.check_output(
                  ["findmnt", "-T", str(args.parent), "-no", "SOURCE,FSTYPE,OPTIONS"], text=True).strip(),
              "cases": [], "notes": [
                  "Global filesystem deltas include background writes and metadata changes.",
                  "Data-used deltas exclude separately allocated metadata.",
                  "No snapshot is created; existing snapshots may affect reclamation."]}
    # TemporaryDirectory removes only this new private directory, including on failure.
    with tempfile.TemporaryDirectory(prefix=".fm-btrfs-space-proof-", dir=args.parent) as temporary:
        directory = Path(temporary)
        report["fixture_directory"] = str(directory)
        baseline = counters(directory)
        time.sleep(0.3)
        sample = counters(directory)
        report["background_sample_delta"] = {k: sample[k] - baseline[k] for k in baseline}

        def remove_case(name, paths):
            before = counters(directory)
            measurements = {p.name: inspect(p) for p in paths}
            prediction = extent_prediction(paths, measurements) if args.extent_metadata else None
            for path in paths:
                path.unlink()
            after = counters(directory)
            row = {"name": name, "files": measurements, "before": before, "after": after,
                   "data_reclaimed_observed": before["data_used"] - after["data_used"],
                   "available_increase_observed": after["available"] - before["available"]}
            if prediction is not None:
                row["prediction"] = prediction
                row["prediction_error_bytes"] = row["data_reclaimed_observed"] - prediction["reclaimable_data_bytes"]
            report["cases"].append(row)
            print(json.dumps(row), flush=True)

        for kind in ("compressible", "random", "sparse"):
            path = directory / (kind + ".bin")
            before = counters(directory)
            write_fixture(path, kind, 256 * MIB if kind == "compressible" else 128 * MIB)
            after = counters(directory)
            report.setdefault("creation", {})[kind] = {
                "data_increase_observed": after["data_used"] - before["data_used"],
                "available_decrease_observed": before["available"] - after["available"]}
            remove_case(kind, [path])

        for kind in ("random", "compressible"):
            source = directory / "source.bin"
            copy = directory / "reflink.bin"
            write_fixture(source, kind, 128 * MIB)
            with source.open("rb") as src, copy.open("xb") as dst:
                fcntl.ioctl(dst.fileno(), 0x40049409, src.fileno())  # FICLONE
            remove_case(kind + "_reflink_first", [source])
            remove_case(kind + "_reflink_last", [copy])
            write_fixture(source, kind, 128 * MIB)
            with source.open("rb") as src, copy.open("xb") as dst:
                fcntl.ioctl(dst.fileno(), 0x40049409, src.fileno())
            remove_case(kind + "_reflink_together", [source, copy])

        source = directory / "source.bin"
        copy = directory / "hardlink.bin"
        write_fixture(source, "random", 128 * MIB)
        os.link(source, copy)
        remove_case("hardlink_first", [source])
        remove_case("hardlink_last", [copy])
    report["fixtures_removed"] = not directory.exists()
    args.report.write_text(json.dumps(report, indent=2) + "\n")
    print("Report:", args.report, "fixtures removed:", report["fixtures_removed"], flush=True)


if __name__ == "__main__":
    main()
