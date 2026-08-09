#!/usr/bin/env python3
"""Offline project-invariant checks; Windows CI compile remains authoritative."""
from __future__ import annotations
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
layout = (ROOT / "src/LayoutMirror.cpp").read_text(encoding="utf-8")
spout = (ROOT / "src/SpoutSender.cpp").read_text(encoding="utf-8")
overlay = (ROOT / "src/PresentationOverlay.cpp").read_text(encoding="utf-8")
mod = json.loads((ROOT / "mod.json").read_text(encoding="utf-8"))
blob = main + "\n" + layout + "\n" + overlay + "\n" + spout

checks = {
    "PlayLayer::init": r"bool init\(GJGameLevel\* level, bool useReplay, bool dontCreateObjects\)",
    "PlayLayer::addObject": r"void addObject\(GameObject\* object\)",
    "PlayLayer::onQuit": r"void onQuit\(\)",
    "ShaderLayer::visit": r"void visit\(\)[\s\S]*isRenderingLayout\(\)[\s\S]*m_inShaderObjectLayer[\s\S]*rawLayer->visit\(\)",
    "CCEGLView::swapBuffers": r"void swapBuffers\(\)",
    "capture before local redraw": r"sendDefaultFramebuffer\(\);[\s\S]*renderPlayerView\(director, real\);[\s\S]*CCEGLView::swapBuffers\(\)",
    "XDBot full transform used": r"LayoutMode::getModifiedString\(std::string\(level->m_levelString\)\)",
    "XDBot serialized plan": r"canonicalWithoutHidden[\s\S]*m_pendingByObjectID",
    "authoritative object binding": r"PlayLayer::addObject\(object\);\s*LayoutMirror::get\(\)\.observeObject\(this, object\);",
    "actual render-node index": r"registerRenderNodes[\s\S]*m_renderNodes\.find\(node\)",
    "CCNode visit mask": r"beginNodeVisit\(this\)[\s\S]*NodeVisitAction::Skip\) return;[\s\S]*CCNode::visit\(\)[\s\S]*endNodeVisit\(this\)",
    "stable spatial camera mask": r"applyCameraOverrides[\s\S]*convertToNodeSpace[\s\S]*std::lower_bound[\s\S]*m_spatialEntries",
    "batched camera mutations": r"touchCameraEntry[\s\S]*getBatchNode\(\)",
    "separate layout opacity": r"observeOpacity\(this, opacity\)[\s\S]*setSpriteOpacity\(object, entry\.layoutOpacity\)",
    "XDBot actual object colors": r"m_isObjectBlack \? kLayoutBlack[\s\S]*m_isColorSpriteBlack \? kLayoutBlack[\s\S]*sprite->setColor\(target\)",
    "HackMega relative capture order": r"setHookPriorityAfterPre\([\s\S]*cocos2d::CCEGLView::swapBuffers[\s\S]*absolllute\.hackmega",
    "HackMega local overlay replay": r"captureSceneBaseline[\s\S]*capturePresentedFrame[\s\S]*replayDifference[\s\S]*glCopyTexSubImage2D",
    "stable-scene transition guard": r"isStableGameplayScene[\s\S]*while \(root->getParent\(\)\)[\s\S]*return root == scene",
    "overlay GL attribute restoration": r"glGetVertexAttribPointerv[\s\S]*restoreAttrib\(kCCVertexAttrib_Position[\s\S]*restoreAttrib\(kCCVertexAttrib_TexCoords",
    "foreign framebuffer clear guard": r"GL_FRAMEBUFFER_BINDING[\s\S]*if \(framebuffer != 0\)[\s\S]*glClear\(",
    "XDBot full special palette": r"kBackgroundChannel[\s\S]*kGround1Channel[\s\S]*kGround2Channel[\s\S]*kLineChannel[\s\S]*kMG1Channel[\s\S]*kMG2Channel[\s\S]*splitView\(newColors, '\|'\)",
    "layout state restored": r"beginLayoutPass\(director, real\);[\s\S]*scene->visit\(\);[\s\S]*endLayoutPass\(\);",
    "Spout default FBO": r"SendFbo\(0, 0, 0, invert\)",
    "Spout force texture": r"SetShareMode\(0\)",
    "Spout CPU mode off": r"SetCPUmode\(false\)",
    "Spout memory mode off": r"SetMemoryShareMode\(false\)",
    "Spout auto fallback off": r"SetAutoShare\(false\)",
    "Spout compatibility retest": r"SetCPUshare\(false\)",
}

failed = [name for name, pattern in checks.items() if not re.search(pattern, blob)]

# A second gameplay world is forbidden after the practice-mode crash regression.
for forbidden in [
    "PlayLayer::create(", "m_mirror->update(", "m_mirror->resetLevel(",
    "m_mirror->markCheckpoint(", "m_mirror->handleButton(", "GameManagerScope",
]:
    if forbidden in blob:
        failed.append(f"second gameplay world forbidden: {forbidden}")

for forbidden_hook in [
    '"PlayLayer::resetLevel"', '"PlayLayer::markCheckpoint"',
    '"PlayLayer::removeCheckpoint"', '"PlayLayer::removeAllCheckpoints"',
    '"GJBaseGameLayer::update"', '"GJBaseGameLayer::handleButton"',
]:
    if forbidden_hook in main:
        failed.append(f"practice/physics hook must not be duplicated: {forbidden_hook}")

if mod.get("gd", {}).get("win") != "2.2081": failed.append("GD target 2.2081")
if mod.get("geode") != "5.8.2": failed.append("Geode target 5.8.2")
if len(mod.get("description", "")) > 45: failed.append("mod description <=45 chars")
if mod.get("version") != "v0.2.3": failed.append("mod version v0.2.3")

resources = mod.get("resources", {}).get("files", [])
for required in [
    "resources/spout/SpoutLibrary.dll",
    "resources/licenses/Spout2-LICENSE.txt",
    "resources/licenses/XDBotFork-CREDITS.txt",
]:
    if required not in resources: failed.append(f"packaged resource {required}")

cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
if "vendor/spout" not in cmake: failed.append("CMake full Spout header include path")
if re.search(r"target_link_libraries\(\$\{PROJECT_NAME\}\s+(?:PRIVATE|PUBLIC|INTERFACE)\b", cmake):
    failed.append("CMake link signature must remain plain")
if not re.search(r"target_link_libraries\(\$\{PROJECT_NAME\}\s+opengl32\)", cmake):
    failed.append("plain opengl32 link")

workflow = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
if "sdk: v5.8.2" not in workflow or "target: Win64" not in workflow:
    failed.append("GitHub Actions Win64 Geode v5.8.2")

if failed:
    print("FAILED:")
    for item in failed: print(" -", item)
    sys.exit(1)
print("Offline project invariant checks OK (single-world layout render / Spout GPU forcing)")
