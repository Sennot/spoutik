#!/usr/bin/env python3
"""Offline project-invariant checks.

This does NOT pretend to replace generated Geode bindings or a real Windows compile. CI also runs
verify_upstream_bindings.py and then performs the authoritative Win64 compile.
"""
from __future__ import annotations
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
layout = (ROOT / "src/LayoutMirror.cpp").read_text(encoding="utf-8")
spout = (ROOT / "src/SpoutSender.cpp").read_text(encoding="utf-8")
spout_abi = (ROOT / "include/SpoutMini.hpp").read_text(encoding="utf-8")
xdbot_compat = (ROOT / "vendor/xdbot/xdbot_compat.hpp").read_text(encoding="utf-8")
mod = json.loads((ROOT / "mod.json").read_text(encoding="utf-8"))
blob = main + "\n" + layout + "\n" + spout + "\n" + spout_abi + "\n" + xdbot_compat

checks = {
    "PlayLayer::init(GJGameLevel*, bool, bool)": r"bool init\(GJGameLevel\* level, bool useReplay, bool dontCreateObjects\)",
    "PlayLayer::addObject(GameObject*)": r"void addObject\(GameObject\* object\)",
    "XDBot addObject mutation sequence": r"if \(excludedTriggerIDs\.contains\(object->m_objectID\)\) return;\s*PlayLayer::addObject\(object\);\s*object->m_activeMainColorID = -1;\s*object->m_activeDetailColorID = -1;\s*object->m_detailUsesHSV = false;\s*object->m_baseUsesHSV = false;\s*object->m_hasNoGlow = true;\s*object->m_isHide = object->m_objectID == 2065;\s*object->setOpacity\(object->m_objectID == 2065 \? 0 : 255\);\s*object->setVisible\(object->m_objectID != 2065\);",
    "PlayLayer::startGame()": r"void startGame\(\)",
    "PlayLayer::startGameDelayed()": r"void startGameDelayed\(\)",
    "GJBaseGameLayer::update(float)": r"void update\(float dt\)",
    "PlayLayer::resetLevel()": r"void resetLevel\(\)",
    "mirror levelComplete blocked": r"void levelComplete\(\)[\s\S]*if \(mirrors\.isCreating\(\) \|\| mirrors\.isMirror\(this\)\)[\s\S]*return;[\s\S]*PlayLayer::levelComplete\(\)",
    "mirror jump commit blocked": r"void commitJumps\(\)[\s\S]*if \(mirrors\.isCreating\(\) \|\| mirrors\.isMirror\(this\)\)[\s\S]*return;[\s\S]*PlayLayer::commitJumps\(\)",
    "mirror attempt bookkeeping blocked": r"void updateAttempts\(\)[\s\S]*if \(mirrors\.isCreating\(\) \|\| mirrors\.isMirror\(this\)\)[\s\S]*return;[\s\S]*PlayLayer::updateAttempts\(\)",
    "PlayLayer::markCheckpoint()": r"CheckpointObject\* markCheckpoint\(\)",
    "PlayLayer::removeCheckpoint(bool)": r"void removeCheckpoint\(bool first\)",
    "PlayLayer::removeAllCheckpoints()": r"void removeAllCheckpoints\(\)",
    "PlayLayer::prepareMusic(bool)": r"void prepareMusic\(bool dontWait\)",
    "PlayLayer::startMusic()": r"void startMusic\(\)",
    "PlayLayer::onQuit()": r"void onQuit\(\)",
    "mirror scene exit blocked": r"void onQuit\(\)[\s\S]*if \(mirrors\.isCreating\(\) \|\| mirrors\.isMirror\(this\)\) return;[\s\S]*mirrors\.destroyFor\(this\);[\s\S]*PlayLayer::onQuit\(\)",
    "LevelTools::verifyLevelIntegrity(gd::string,int)": r"static bool verifyLevelIntegrity\(gd::string levelString, int objectCount\)",
    "GJBaseGameLayer::handleButton(bool,int,bool)": r"void handleButton\(bool down, int button, bool player1\)",
    "CCEGLView::swapBuffers()": r"void swapBuffers\(\)",
    "last pre-swap presentation hook": r"setHookPriorityPre\(\"cocos2d::CCEGLView::swapBuffers\", Priority::Last\)",
    "capture happens before local layout redraw": r"sendDefaultFramebuffer\(\);[\s\S]*renderPlayerView\(director, real\);[\s\S]*CCEGLView::swapBuffers\(\)",
    "Spout default FBO": r"SendFbo\(0, width, height, invert\)",
    "Spout CPU fallback rejected": r"m_spout->GetCPU\(\).*m_spout->GetGLDX\(\)",
    "private mirror GJGameLevel": r"GJGameLevel::create\(\)[\s\S]*copyLevelInfo\(level\)[\s\S]*m_levelString = m_modifiedString",
    "mirror start is synchronized": r"mirrors\.startFromReal\(this\)",
    "mirror autonomous delayed start blocked": r"void startGameDelayed\(\)[\s\S]*mirrors\.isMirror\(this\)[\s\S]*return;[\s\S]*PlayLayer::startGameDelayed\(\)",
    "mirror scheduler double tick blocked": r"mirrors\.isMirror\(play\)[\s\S]*mirrors\.isStepping\(\)[\s\S]*return;[\s\S]*stepFromReal\(play, dt\)",
    "master enable stops mirror tick": r"stepFromReal\(PlayLayer\* real, float dt\)[\s\S]*getSettingValue<bool>\(\"enabled\"\)",
    "master enable stops local redraw": r"renderPlayerView\((?:cocos2d::)?CCDirector\* director, PlayLayer\* real\)[\s\S]*getSettingValue<bool>\(\"enabled\"\)",
    "mirror update unscheduled": r"m_mirror->unscheduleUpdate\(\)",
    "mirror audio hard-suppressed": r"m_isSilent = true;[\s\S]*m_musicPrepared = true",
    "practice runtime flags synced": r"m_mirror->m_isPracticeMode = real->m_isPracticeMode;[\s\S]*m_mirror->m_practiceMusicSync = real->m_practiceMusicSync",
    "StartPos mapped inside mirror graph": r"typeinfo_cast<StartPosObject\*>[\s\S]*m_mirror->m_startPosObject = best",
    "XDBot-compatible split preserves final field": r"parts\.emplace_back\(input\.substr\(start\)\);[\s\S]*return parts;",
}

failed = [name for name, pattern in checks.items() if not re.search(pattern, blob)]
if re.search(r"m_mirror->m_startPosObject\s*=\s*real->m_startPosObject", blob):
    failed.append("StartPos pointer must never be copied across PlayLayer object graphs")
if re.search(r"stepFromReal\(PlayLayer\* real, float dt\)[\s\S]*real->m_playerDied", layout):
    failed.append("mirror should not freeze death-animation ticks before authoritative reset")
if mod.get("gd", {}).get("win") != "2.2081":
    failed.append("mod target GD 2.2081")
if mod.get("geode") != "v5.8.2":
    failed.append("mod target Geode v5.8.2")
if len(mod.get("description", "")) > 45:
    failed.append("mod.json description must be <=45 characters")
resources = mod.get("resources", {}).get("files", [])
for required in [
    "resources/spout/SpoutLibrary.dll",
    "resources/licenses/Spout2-LICENSE.txt",
    "resources/licenses/XDBotFork-CREDITS.txt",
]:
    if required not in resources:
        failed.append(f"packaged resource {required}")
workflow = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
if "sdk: v5.8.2" not in workflow or "target: Win64" not in workflow:
    failed.append("GitHub Actions pinned Win64 Geode build")
if "verify_upstream_bindings.py" not in workflow or "verify_geode_package.py" not in workflow:
    failed.append("CI upstream-binding/package verification")
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
if "CMAKE_CXX_STANDARD 23" not in cmake or "vendor/xdbot/layout_mode.cpp" not in cmake:
    failed.append("CMake C++23/full XDBot translation unit")
# Geode's setup_geode_mod() links the SDK with CMake's plain target_link_libraries
# signature. Mixing that with PRIVATE/PUBLIC/INTERFACE on the same target is a
# hard CMake configure error. Keep our explicit opengl32 link plain too.
if re.search(r"target_link_libraries\(\$\{PROJECT_NAME\}\s+(?:PRIVATE|PUBLIC|INTERFACE)\b", cmake):
    failed.append("CMake link signature must stay plain for setup_geode_mod compatibility")
if not re.search(r"target_link_libraries\(\$\{PROJECT_NAME\}\s+opengl32\)", cmake):
    failed.append("Win32 opengl32 link must use plain target_link_libraries signature")

abi_order = [
    "SetSenderName", "SetSenderFormat", "ReleaseSender", "SendFbo", "SendTexture",
    "SendImage", "IsInitialized", "GetName", "GetWidth", "GetHeight", "GetFps",
    "GetFrame", "GetHandle", "GetCPU", "GetGLDX",
]
positions = [spout_abi.find(name) for name in abi_order]
if any(pos < 0 for pos in positions) or positions != sorted(positions):
    failed.append("Spout 2.007.017 ABI prefix order")

if failed:
    print("FAILED:")
    for x in failed:
        print(" -", x)
    sys.exit(1)
print("Offline project invariant checks OK (GD 2.2081 / Geode v5.8.2 target)")
