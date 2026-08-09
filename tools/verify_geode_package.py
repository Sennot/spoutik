#!/usr/bin/env python3
"""Inspect a built .geode and prove Spout + credits are physically embedded."""
from __future__ import annotations
import pathlib
import sys
import zipfile

if len(sys.argv) != 2:
    raise SystemExit("usage: verify_geode_package.py <build-output-directory-or-geode>")
root = pathlib.Path(sys.argv[1])
if root.is_file() and root.suffix == ".geode":
    candidates = [root]
else:
    candidates = list(root.rglob("*.geode")) if root.exists() else []
if not candidates:
    raise SystemExit(f"No .geode found under {root}")
if len(candidates) > 1:
    print("Multiple .geode files found; validating all:", *candidates, sep="\n - ")

required_suffixes = [
    "mod.json",
    "SpoutLibrary.dll",
    "Spout2-LICENSE.txt",
    "XDBotFork-CREDITS.txt",
]

for package in candidates:
    with zipfile.ZipFile(package) as zf:
        names = zf.namelist()
        for suffix in required_suffixes:
            if not any(pathlib.PurePosixPath(n).name == suffix for n in names):
                raise SystemExit(f"{package.name}: missing embedded {suffix}")
        dll_name = next(n for n in names if pathlib.PurePosixPath(n).name == "SpoutLibrary.dll")
        data = zf.read(dll_name)
        if len(data) < 0x40 or data[:2] != b"MZ":
            raise SystemExit(f"{package.name}: embedded SpoutLibrary.dll is not PE")
        pe = int.from_bytes(data[0x3C:0x40], "little")
        if data[pe:pe+4] != b"PE\0\0" or int.from_bytes(data[pe+4:pe+6], "little") != 0x8664:
            raise SystemExit(f"{package.name}: embedded SpoutLibrary.dll is not x64")
    print(f"Package OK: {package} (SpoutLibrary.dll + notices embedded)")
