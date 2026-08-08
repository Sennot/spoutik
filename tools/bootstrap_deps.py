#!/usr/bin/env python3
from __future__ import annotations
import argparse
import hashlib
import json
import pathlib
import re
import tempfile
import urllib.request
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
CFG = json.loads((ROOT / "tools" / "deps.json").read_text(encoding="utf-8"))


def fetch(url: str) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": "spout-layout-dualview-bootstrap/1"})
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read()


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def transform_xdbot(raw_hpp: str, raw_cpp: str) -> tuple[str, str, str]:
    """Adapt XDBot Layout Mode without editing its actual LayoutMode logic.

    Header: only replace XDBot's umbrella include with our compatibility include.
    Cpp: remove only XDBot's global Geode hook wrapper (lines before the concrete
    LayoutMode implementation). The implementation suffix itself is byte-for-byte
    preserved after newline normalization performed by Python's text decoding.
    """
    include_line = '#include "../includes.hpp"'
    if include_line not in raw_hpp:
        raise RuntimeError("Upstream layout_mode.hpp changed: XDBot umbrella include not found")
    adapted_hpp = raw_hpp.replace(include_line, '#include "xdbot_compat.hpp"', 1)

    marker = "std::string LayoutMode::getModifiedString"
    pos = raw_cpp.find(marker)
    if pos < 0:
        raise RuntimeError("Upstream layout_mode.cpp changed: LayoutMode marker not found")
    pristine_logic = raw_cpp[pos:]
    adapted_cpp = '#include "layout_mode.hpp"\n\n' + pristine_logic

    # Fail instead of silently integrating a partial or materially changed source.
    required_hpp = [
        "excludedTriggerIDs", "importantTriggerIDs", "decoObjectIDs",
        "solidObjectIDs", "newColors",
    ]
    required_cpp = ["LayoutMode::getModifiedString", "LayoutMode::mergeVector", "importantGroups"]
    for token in required_hpp:
        if token not in adapted_hpp:
            raise RuntimeError(f"XDBot header missing required token: {token}")
    for token in required_cpp:
        if token not in adapted_cpp:
            raise RuntimeError(f"XDBot cpp missing required token: {token}")
    if len(adapted_hpp.encode()) < 18_000:
        raise RuntimeError("XDBot layout_mode.hpp unexpectedly small; refusing partial integration")
    if len(adapted_cpp.encode()) < 4_500:
        raise RuntimeError("XDBot layout_mode.cpp unexpectedly small; refusing partial integration")

    # This equality is the important guarantee: no statement inside the actual
    # XDBot LayoutMode implementation is rewritten, filtered or regenerated.
    if adapted_cpp[len('#include "layout_mode.hpp"\n\n'):] != pristine_logic:
        raise RuntimeError("Internal error: XDBot LayoutMode implementation was modified")

    return adapted_hpp, adapted_cpp, pristine_logic


def validate_xdbot_addobject(raw_cpp: str) -> None:
    """Prove the mirror-side post-add mutation block matches pinned XDBot.

    The upstream hook has one project-specific gate (`Global::layoutMode`) that
    we intentionally replace with LayoutMirror::layoutContext. Everything from
    the excluded-trigger test through setVisible must remain statement-identical.
    """
    start_token = "if (excludedTriggerIDs.contains(obj->m_objectID)) return;"
    end_token = "obj->setVisible(obj->m_objectID != 2065);"
    start = raw_cpp.find(start_token)
    if start < 0:
        raise RuntimeError("Upstream XDBot addObject start block not found")
    end = raw_cpp.find(end_token, start)
    if end < 0:
        raise RuntimeError("Upstream XDBot addObject end block not found")
    upstream = raw_cpp[start:end + len(end_token)]
    upstream = re.sub(r"\bobj\b", "object", upstream)

    main_cpp = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
    squash = lambda x: re.sub(r"\s+", "", x)
    if squash(upstream) not in squash(main_cpp):
        raise RuntimeError("Mirror addObject block no longer matches pinned XDBot behavior")


def sync_xdbot() -> None:
    info = CFG["xdbot"]
    base = f"https://raw.githubusercontent.com/{info['repo']}/{info['commit']}/"
    out = ROOT / "vendor" / "xdbot"
    pristine = out / "upstream"
    out.mkdir(parents=True, exist_ok=True)
    pristine.mkdir(parents=True, exist_ok=True)

    files = info.get("files", [])
    if files != ["src/hacks/layout_mode.hpp", "src/hacks/layout_mode.cpp"]:
        raise RuntimeError(f"Unexpected XDBot file manifest: {files!r}")
    hpp_path, cpp_path = files
    hpp_bytes = fetch(base + hpp_path)
    cpp_bytes = fetch(base + cpp_path)
    raw_hpp = hpp_bytes.decode("utf-8")
    raw_cpp = cpp_bytes.decode("utf-8")
    validate_xdbot_addobject(raw_cpp)
    adapted_hpp, adapted_cpp, pristine_logic = transform_xdbot(raw_hpp, raw_cpp)

    # Keep the exact downloaded upstream files in the source artifact for audit.
    (pristine / "layout_mode.hpp").write_bytes(hpp_bytes)
    (pristine / "layout_mode.cpp").write_bytes(cpp_bytes)
    (out / "layout_mode.hpp").write_text(adapted_hpp, encoding="utf-8", newline="\n")
    (out / "layout_mode.cpp").write_text(adapted_cpp, encoding="utf-8", newline="\n")

    (out / "UPSTREAM.txt").write_text(
        f"Source: https://github.com/{info['repo']}\n"
        f"Commit: {info['commit']}\n"
        f"Files: {hpp_path}, {cpp_path}\n"
        f"layout_mode.hpp SHA256: {sha256(hpp_bytes)}\n"
        f"layout_mode.cpp SHA256: {sha256(cpp_bytes)}\n"
        f"Preserved implementation suffix SHA256: {sha256(pristine_logic.encode('utf-8'))}\n"
        "Adaptation: header umbrella include replaced; cpp Geode hook wrapper removed; "
        "LayoutMode implementation suffix preserved verbatim.\n",
        encoding="utf-8",
    )


def pe_machine(data: bytes) -> int | None:
    if len(data) < 0x40 or data[:2] != b"MZ":
        return None
    pe_offset = int.from_bytes(data[0x3C:0x40], "little")
    if pe_offset + 6 > len(data) or data[pe_offset:pe_offset + 4] != b"PE\0\0":
        return None
    return int.from_bytes(data[pe_offset + 4:pe_offset + 6], "little")


def sync_spout() -> None:
    info = CFG["spout"]
    url = f"https://github.com/leadedge/Spout2/releases/download/{info['tag']}/{info['asset']}"
    data = fetch(url)
    with tempfile.TemporaryDirectory() as td:
        zpath = pathlib.Path(td) / "spout.zip"
        zpath.write_bytes(data)
        with zipfile.ZipFile(zpath) as zf:
            names = zf.namelist()
            dlls = [n for n in names if pathlib.PurePosixPath(n).name.lower() == "spoutlibrary.dll"]
            if not dlls:
                raise RuntimeError("Spout release archive has no SpoutLibrary.dll")

            # Do not trust archive folder names such as x64/Win32. Inspect every
            # candidate PE header and select the actual AMD64 image (0x8664).
            chosen = None
            chosen_bytes = None
            for name in dlls:
                blob = zf.read(name)
                if pe_machine(blob) == 0x8664:
                    chosen = name
                    chosen_bytes = blob
                    break
            if chosen is None or chosen_bytes is None:
                raise RuntimeError("Spout release archive has no x64 SpoutLibrary.dll")

            out = ROOT / "resources" / "spout" / "SpoutLibrary.dll"
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_bytes(chosen_bytes)

    license_url = f"https://raw.githubusercontent.com/leadedge/Spout2/{info['tag']}/LICENSE"
    try:
        license_bytes = fetch(license_url)
    except Exception:
        header = fetch(
            f"https://raw.githubusercontent.com/leadedge/Spout2/{info['tag']}/SPOUTSDK/SpoutLibrary/SpoutLibrary.h"
        ).decode("utf-8")
        m = re.search(r"/\*\s*(Copyright.*?THIS SOFTWARE.*?DAMAGE\.\s*)\*/", header, re.S)
        if not m:
            raise
        license_bytes = m.group(1).encode("utf-8")
    (ROOT / "resources" / "licenses" / "Spout2-LICENSE.txt").write_bytes(license_bytes)


def write_xdbot_credit() -> None:
    info = CFG["xdbot"]
    text = f"""XDBot / XDBotFork Layout Mode credits

Original project: Zilko/xdBot
Integration source: https://github.com/{info['repo']}
Pinned commit: {info['commit']}
Integrated files: src/hacks/layout_mode.hpp and src/hacks/layout_mode.cpp

Layout preprocessing, complete object classification tables, trigger exclusions and the LayoutMode implementation are taken from the pinned XDBotFork source. Preserve the exact license/permission notice you received from the upstream authors when redistributing this mod.
"""
    (ROOT / "resources" / "licenses" / "XDBotFork-CREDITS.txt").write_text(text, encoding="utf-8")


def validate_pe_x64(dll: pathlib.Path) -> None:
    if dll.stat().st_size < 100_000:
        raise SystemExit("SpoutLibrary.dll looks truncated")
    data = dll.read_bytes()
    machine = pe_machine(data)
    if machine is None:
        raise SystemExit("SpoutLibrary.dll is not a valid PE image")
    if machine != 0x8664:
        raise SystemExit(f"SpoutLibrary.dll is not x64 (PE machine=0x{machine:04X})")


def validate() -> None:
    paths = [
        ROOT / "vendor/xdbot/layout_mode.hpp",
        ROOT / "vendor/xdbot/layout_mode.cpp",
        ROOT / "vendor/xdbot/upstream/layout_mode.hpp",
        ROOT / "vendor/xdbot/upstream/layout_mode.cpp",
        ROOT / "vendor/xdbot/UPSTREAM.txt",
        ROOT / "resources/spout/SpoutLibrary.dll",
        ROOT / "resources/licenses/Spout2-LICENSE.txt",
        ROOT / "resources/licenses/XDBotFork-CREDITS.txt",
    ]
    missing = [str(p.relative_to(ROOT)) for p in paths if not p.exists()]
    if missing:
        raise SystemExit("Missing bootstrapped dependencies: " + ", ".join(missing))

    raw_hpp = (ROOT / "vendor/xdbot/upstream/layout_mode.hpp").read_text(encoding="utf-8")
    raw_cpp = (ROOT / "vendor/xdbot/upstream/layout_mode.cpp").read_text(encoding="utf-8")
    validate_xdbot_addobject(raw_cpp)
    expected_hpp, expected_cpp, _ = transform_xdbot(raw_hpp, raw_cpp)
    actual_hpp = (ROOT / "vendor/xdbot/layout_mode.hpp").read_text(encoding="utf-8")
    actual_cpp = (ROOT / "vendor/xdbot/layout_mode.cpp").read_text(encoding="utf-8")
    if actual_hpp != expected_hpp or actual_cpp != expected_cpp:
        raise SystemExit("Adapted XDBot files do not exactly match the audited transform")

    validate_pe_x64(ROOT / "resources/spout/SpoutLibrary.dll")
    print("Dependency validation OK (audited XDBot transform + x64 SpoutLibrary.dll)")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--validate-only", action="store_true")
    args = ap.parse_args()
    if not args.validate_only:
        sync_xdbot()
        sync_spout()
        write_xdbot_credit()
    validate()


if __name__ == "__main__":
    main()
