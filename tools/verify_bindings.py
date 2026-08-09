#!/usr/bin/env python3
"""Offline safety and architecture checks; Windows CI compile is authoritative."""
from __future__ import annotations
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
layout = (ROOT / "src/LayoutMirror.cpp").read_text(encoding="utf-8")
bridge = (ROOT / "src/CompanionBridge.cpp").read_text(encoding="utf-8")
viewer = (ROOT / "companion/main.cpp").read_text(encoding="utf-8")
protocol = (ROOT / "include/CompanionProtocol.hpp").read_text(encoding="utf-8")
mod = json.loads((ROOT / "mod.json").read_text(encoding="utf-8"))
runtime = main + "\n" + layout + "\n" + bridge

failed: list[str] = []
for required in (
    "LayoutMode::getModifiedString", "canonicalWithoutHidden",
    "CompanionBridge::get().publish", "CreateFileMappingW",
    "InterlockedIncrement64", "writeCompanionFrame", "std::lower_bound",
):
    if required not in runtime:
        failed.append(f"runtime missing {required}")

for forbidden in (
    "PlayLayer::create(", "scene->visit()", "glClear(", "glBlitFramebuffer(",
    "glReadPixels(", "FrameCompositor", "SpoutSender", "SendFbo(", "SendTexture(",
):
    if forbidden in runtime:
        failed.append(f"unsafe in-process render path present: {forbidden}")

for required in (
    "SDL_CreateWindow", "SDL_GL_CreateContext", "glBegin(GL_QUADS)",
    "OpenFileMappingW(FILE_MAP_READ", "kProtocolVersion",
):
    if required not in viewer + protocol:
        failed.append(f"companion missing {required}")

if mod.get("gd", {}).get("win") != "2.2081": failed.append("GD target 2.2081")
if mod.get("geode") != "5.8.2": failed.append("Geode target 5.8.2")
if mod.get("version") != "v0.4.1": failed.append("mod version v0.4.1")
if len(mod.get("description", "")) > 45: failed.append("mod description <=45 chars")

resources = mod.get("resources", {}).get("files", [])
if "resources/licenses/XDBotFork-CREDITS.txt" not in resources:
    failed.append("XDBot credits packaged")
if any("Spout" in item for item in resources):
    failed.append("Spout must not be packaged by bridge")

workflow = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
if "sdk: v5.8.2" not in workflow or "target: Win64" not in workflow:
    failed.append("GitHub Actions Win64 Geode v5.8.2")
if "cmake -S companion" not in workflow:
    failed.append("GitHub Actions companion build")

if failed:
    print("FAILED:")
    for item in failed: print(" -", item)
    sys.exit(1)
print("Offline checks OK (read-only Geode bridge + SDL3/OpenGL companion)")
