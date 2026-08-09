#!/usr/bin/env python3
"""Inspect a built .geode and validate the bridge-only package."""
from __future__ import annotations
import pathlib
import sys
import zipfile

if len(sys.argv) != 2:
    raise SystemExit("usage: verify_geode_package.py <build-output-directory-or-geode>")
root = pathlib.Path(sys.argv[1])
candidates = [root] if root.is_file() and root.suffix == ".geode" else list(root.rglob("*.geode"))
if not candidates:
    raise SystemExit(f"No .geode found under {root}")

for package in candidates:
    with zipfile.ZipFile(package) as archive:
        names = [pathlib.PurePosixPath(name).name for name in archive.namelist()]
        for required in ("mod.json", "XDBotFork-CREDITS.txt"):
            if required not in names:
                raise SystemExit(f"{package.name}: missing embedded {required}")
        for forbidden in ("SpoutLibrary.dll", "Spout2-LICENSE.txt"):
            if forbidden in names:
                raise SystemExit(f"{package.name}: obsolete {forbidden} is still packaged")
    print(f"Package OK: {package} (bridge + XDBot credits, no Spout runtime)")
