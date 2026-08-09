#!/usr/bin/env python3
from __future__ import annotations
import argparse
import hashlib
import json
import pathlib
import re
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
CFG = json.loads((ROOT / "tools" / "deps.json").read_text(encoding="utf-8"))


def fetch(url: str) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": "layout-companion-bootstrap/1"})
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
    # Do NOT use arbitrary byte-size thresholds here. The pinned upstream cpp is
    # intentionally compact, and the preserved implementation suffix is smaller
    # than an earlier 4.5 KiB guard even though it is complete. Validate semantic
    # structure instead.
    required_hpp = [
        "const std::string newColors",
        "const std::unordered_set<int> robtopLevelIDs",
        "const std::unordered_set<int> excludedTriggerIDs",
        "const std::map<int, std::vector<int>> importantTriggerIDs",
        "const std::unordered_set<int> decoObjectIDs",
        "const std::unordered_set<int> solidObjectIDs",
    ]
    required_cpp = [
        "std::string LayoutMode::getModifiedString",
        "ZipUtils::decompressString",
        "Utils::splitByChar(decompString, ';')",
        "levelSettings[1] = newColors",
        "importantTriggerIDs.contains",
        "decoObjectIDs.contains",
        "props.contains(121)",
        "props.contains(57)",
        "solidObjectIDs.contains",
        "props.contains(129)",
        "props.contains(135)",
        'return firstPart + newString + ";"',
        "std::string LayoutMode::mergeVector",
    ]
    for token in required_hpp:
        if token not in adapted_hpp:
            raise RuntimeError(f"XDBot header missing required structure: {token}")
    for token in required_cpp:
        if token not in adapted_cpp:
            raise RuntimeError(f"XDBot cpp missing required structure: {token}")

    # These declarations are single complete definitions in the pinned header.
    # Requiring their terminators catches truncated downloads without guessing a
    # minimum source size.
    for name in ("robtopLevelIDs", "excludedTriggerIDs", "importantTriggerIDs", "decoObjectIDs", "solidObjectIDs"):
        declaration = re.search(rf"\b{name}\b\s*=.*?;", adapted_hpp, re.S)
        if declaration is None:
            raise RuntimeError(f"XDBot header definition appears truncated: {name}")

    # The preserved implementation must contain both functions and finish at the
    # closing brace of mergeVector. This catches a partial cpp response while
    # allowing the exact compact upstream implementation.
    if not re.search(r"return\s+result\s*;\s*}\s*$", pristine_logic, re.S):
        raise RuntimeError("XDBot layout_mode.cpp appears truncated after mergeVector")

    # This equality is the important guarantee: no statement inside the actual
    # XDBot LayoutMode implementation is rewritten, filtered or regenerated.
    if adapted_cpp[len('#include "layout_mode.hpp"\n\n'):] != pristine_logic:
        raise RuntimeError("Internal error: XDBot LayoutMode implementation was modified")

    return adapted_hpp, adapted_cpp, pristine_logic


def validate_xdbot_addobject(raw_cpp: str) -> None:
    """Prove companion export preserves pinned XDBot visual decisions."""
    start_token = "obj->m_activeMainColorID = -1;"
    end_token = "obj->setVisible(obj->m_objectID != 2065);"
    start = raw_cpp.find(start_token)
    if start < 0:
        raise RuntimeError("Upstream XDBot addObject visual block start not found")
    end = raw_cpp.find(end_token, start)
    if end < 0:
        raise RuntimeError("Upstream XDBot addObject visual block end not found")
    layout_cpp = (ROOT / "src" / "LayoutMirror.cpp").read_text(encoding="utf-8")
    required_local_semantics = (
        "if (excludedTriggerIDs.contains(object->m_objectID)) keep = false",
        "if (!entry.keep || entry.forceHidden) return",
        "entry.object->m_isObjectBlack ? kLayoutBlack : kLayoutWhite",
        "entry.object->m_isColorSpriteBlack ? kLayoutBlack : kLayoutWhite",
        "appendNode(",
        "entry.object->m_colorSprite",
        "QuadKind::ObjectMain",
        "QuadKind::ObjectDetail",
    )
    missing = [token for token in required_local_semantics if token not in layout_cpp]
    if missing:
        raise RuntimeError(f"Companion export lost XDBot addObject semantics: {missing}")


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
    print(
        "XDBot Layout Mode fetched OK: "
        f"hpp={len(hpp_bytes)} bytes, cpp={len(cpp_bytes)} bytes, "
        f"preserved implementation={len(pristine_logic.encode('utf-8'))} bytes"
    )

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


def validate() -> None:
    paths = [
        ROOT / "vendor/xdbot/layout_mode.hpp",
        ROOT / "vendor/xdbot/layout_mode.cpp",
        ROOT / "vendor/xdbot/upstream/layout_mode.hpp",
        ROOT / "vendor/xdbot/upstream/layout_mode.cpp",
        ROOT / "vendor/xdbot/UPSTREAM.txt",
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

    print("Dependency validation OK (audited XDBot transform)")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--validate-only", action="store_true")
    args = ap.parse_args()
    if not args.validate_only:
        sync_xdbot()
        write_xdbot_credit()
    validate()


if __name__ == "__main__":
    main()
