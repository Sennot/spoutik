#!/usr/bin/env python3
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class RuntimeRegressionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
        cls.layout = (ROOT / "src/LayoutMirror.cpp").read_text(encoding="utf-8")
        cls.bridge = (ROOT / "src/CompanionBridge.cpp").read_text(encoding="utf-8")
        cls.protocol = (ROOT / "include/CompanionProtocol.hpp").read_text(encoding="utf-8")
        cls.viewer = (ROOT / "companion/main.cpp").read_text(encoding="utf-8")
        cls.runtime = cls.main + "\n" + cls.layout + "\n" + cls.bridge

    def test_no_second_gameplay_world_or_render_pass(self):
        for forbidden in (
            "PlayLayer::create(", "scene->visit()", "rawLayer->visit()",
            "glClear(", "glBlitFramebuffer(", "glReadPixels(", "swapBuffers()",
            "FrameCompositor", "SpoutSender", "SendFbo(", "SendTexture(",
        ):
            self.assertNotIn(forbidden, self.runtime)

    def test_only_read_only_scene_publish_hooks_remain(self):
        self.assertIn('#include <Geode/modify/PlayLayer.hpp>', self.main)
        self.assertIn('#include <Geode/modify/CCDirector.hpp>', self.main)
        for forbidden in (
            "modify/ShaderLayer.hpp", "modify/CCEGLView.hpp",
            "modify/CCNode.hpp", "modify/GameObject.hpp",
        ):
            self.assertNotIn(forbidden, self.main)
        self.assertRegex(
            self.main,
            r"void drawScene\(\) \{\s*CCDirector::drawScene\(\);\s*CompanionBridge::get\(\)\.publish",
        )

    def test_xdbot_output_classifies_authoritative_objects(self):
        for token in (
            "LayoutMode::getModifiedString", "buildLayoutPlan(level)",
            "canonicalWithoutHidden", "m_pendingByObjectID", "entry.keep",
            "excludedTriggerIDs.contains", "observeObject(this, object)",
        ):
            self.assertIn(token, self.layout + self.main)
        self.assertNotIn("ObjectKey", self.layout)

    def test_invisible_decorated_state_does_not_hide_layout_structure(self):
        comment = "Deliberately ignore decorated-world visibility, opacity and toggle"
        self.assertIn(comment, self.layout)
        export = self.layout[self.layout.index(comment):]
        export = export[:export.index("auto exportObject")]
        for forbidden in (
            "isVisible", "getOpacity", "m_isGroupDisabled", "m_isDisabled",
            "setVisible", "setOpacity", "setColor",
        ):
            self.assertNotIn(forbidden, export)

    def test_camera_query_does_not_scan_full_level_each_frame(self):
        export = self.layout[self.layout.index("bool LayoutMirror::writeCompanionFrame"):]
        for token in (
            "m_calcNonEffectObjects", "m_visibleObjects", "convertToNodeSpace",
            "std::lower_bound", "m_spatialEntries", "m_nonEffectObjects", "m_sections",
        ):
            self.assertIn(token, export)
        self.assertNotRegex(export, r"for \(auto const& entry : m_entries\)")

    def test_protocol_is_bounded_versioned_seqlock(self):
        for token in (
            "kProtocolVersion = 1", "kMaximumQuads = 16384",
            "alignas(8) std::uint64_t sequence", "FrameTruncated",
            "std::is_trivially_copyable_v<SharedFrame>",
        ):
            self.assertIn(token, self.protocol)
        for token in (
            "InterlockedIncrement64(sequence)", "MemoryBarrier()",
            "CreateFileMappingW", "MapViewOfFile",
        ):
            self.assertIn(token, self.bridge)

    def test_companion_uses_sdl3_opengl_and_read_only_mapping(self):
        for token in (
            "#include <SDL3/SDL.h>", "SDL_CreateWindow", "SDL_GL_CreateContext",
            "SDL_GL_SwapWindow", "glOrtho", "glBegin(GL_QUADS)",
            "OpenFileMappingW(FILE_MAP_READ", "MapViewOfFile(",
            "InterlockedCompareExchange64", "--overlay", "--always-on-top",
        ):
            self.assertIn(token, self.viewer)
        self.assertNotIn("FILE_MAP_WRITE", self.viewer)

    def test_transition_and_quit_suspend_bridge(self):
        self.assertGreaterEqual(self.main.count("CompanionBridge::get().suspend();"), 2)
        self.assertIn("typeinfo_cast<cocos2d::CCTransitionScene*>(scene)", self.layout)
        self.assertIn("director->getNextScene()", self.layout)
        self.assertIn("while (root->getParent())", self.layout)


if __name__ == "__main__":
    unittest.main()
