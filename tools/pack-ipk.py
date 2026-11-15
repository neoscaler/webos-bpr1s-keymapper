#!/usr/bin/env python3
"""Packs the staged app into a webOS IPK.

webOS 5 pkgverifier only accepts plain ustar members with non-zero timestamps.
GNU tar output is rejected with "-5 ipk verified failed", so the archives are
written here directly.

Member names carry no "./" prefix: the Homebrew Channel's central repository
reads appinfo.json with tarfile.getmember("usr/palm/applications/<id>/..."),
which does not match "./usr/...". Packages built with ares-cli, which install
fine on webOS 5, use the same prefix-free layout.
"""
import gzip
import hashlib
import io
import os
import sys
import tarfile

MTIME = 1700000000


def tar_bytes(entries):
    buf = io.BytesIO()
    with gzip.GzipFile(fileobj=buf, mode="wb", mtime=MTIME) as gz:
        with tarfile.open(fileobj=gz, mode="w", format=tarfile.USTAR_FORMAT) as tar:
            for name, data, mode in entries:
                info = tarfile.TarInfo(name)
                info.uid = info.gid = 0
                info.uname = info.gname = "root"
                info.mtime = MTIME
                info.mode = mode
                if data is None:
                    info.type = tarfile.DIRTYPE
                else:
                    info.size = len(data)
                tar.addfile(info, None if data is None else io.BytesIO(data))
    return buf.getvalue()


def data_entries(stage):
    entries, dirs = [], set()

    def add_dir(path):
        parts = path.split("/")
        for i in range(1, len(parts) + 1):
            prefix = "/".join(parts[:i])
            if prefix not in dirs:
                dirs.add(prefix)
                entries.append((prefix, None, 0o755))

    for here, subdirs, files in os.walk(stage):
        subdirs.sort()
        rel = os.path.relpath(here, stage)
        if rel != ".":
            add_dir(rel)
        for name in sorted(files):
            path = name if rel == "." else rel + "/" + name
            mode = 0o755 if os.access(os.path.join(here, name), os.X_OK) else 0o644
            entries.append((path, open(os.path.join(here, name), "rb").read(), mode))
    return entries


def control_entries(ctrl):
    return [
        ("control", open(os.path.join(ctrl, "control"), "rb").read(), 0o644),
    ]


def check_data_archive(blob):
    """Guard the layout the central Homebrew repository depends on."""
    with tarfile.open(fileobj=io.BytesIO(blob), mode="r:gz") as tar:
        names = tar.getnames()
        appinfo = [n for n in names if n.startswith("usr/palm/applications/")
                   and n.endswith("/appinfo.json")]
        if not appinfo:
            raise SystemExit("no usr/palm/applications/<id>/appinfo.json in payload")
        tar.getmember(appinfo[0])
    prefixed = [n for n in names if n.startswith("./")]
    if prefixed:
        raise SystemExit("tar member %s starts with ./ - the central Homebrew "
                         "repository cannot read the package" % prefixed[0])


def main():
    stage, ctrl, out = sys.argv[1:4]
    data = tar_bytes(data_entries(stage))
    check_data_archive(data)
    parts = [
        ("debian-binary", b"2.0\n"),
        ("control.tar.gz", tar_bytes(control_entries(ctrl))),
        ("data.tar.gz", data),
    ]
    with open(out, "wb") as f:
        f.write(b"!<arch>\n")
        for name, blob in parts:
            header = "%-16s%-12d%-6d%-6d%-8s%-10d`\n" % (
                name, MTIME, 0, 0, "100644", len(blob))
            f.write(header.encode())
            f.write(blob + (b"\n" if len(blob) % 2 else b""))
    digest = hashlib.sha256(open(out, "rb").read()).hexdigest()
    print(digest + "  " + out)


if __name__ == "__main__":
    main()
