#!/usr/bin/env python3
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class RuntimeRegressionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.spout = (ROOT / "src/SpoutSender.cpp").read_text(encoding="utf-8")
        cls.main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
        cls.layout = (ROOT / "src/LayoutMirror.cpp").read_text(encoding="utf-8")
        cls.header = (ROOT / "include/LayoutMirror.hpp").read_text(encoding="utf-8")

    def test_spout_forces_texture_mode_before_sending(self):
        for call in (
            "SetMemoryShareMode(false)",
            "SetCPUmode(false)",
            "SetShareMode(0)",
            "SetAutoShare(false)",
            "SetCPUshare(false)",
        ):
            self.assertIn(call, self.spout)
        self.assertLess(self.spout.index("forceGpuTextureSharing();"), self.spout.index("refreshName();"))
        self.assertIn("SendFbo(0, 0, 0, invert)", self.spout)

    def test_cpu_mode_does_not_destroy_sender(self):
        cpu_block = re.search(r"if \(m_spout->GetCPU\(\)\) \{([\s\S]*?)\n    \}", self.spout)
        self.assertIsNotNone(cpu_block)
        self.assertNotIn("ReleaseSender", cpu_block.group(1))
        self.assertNotIn("return false", cpu_block.group(1))
        self.assertIn("Sender remains live for diagnosis", cpu_block.group(1))

    def test_no_second_gameplay_world_exists(self):
        blob = self.layout + "\n" + self.main
        for forbidden in (
            "PlayLayer::create(",
            "m_mirror->update(",
            "m_mirror->resetLevel(",
            "m_mirror->markCheckpoint(",
            "m_mirror->handleButton(",
            "GameManagerScope",
            "unscheduleAllForTarget(m_mirror)",
        ):
            self.assertNotIn(forbidden, blob)

    def test_practice_checkpoint_and_physics_hooks_are_not_duplicated(self):
        for hook in (
            '"PlayLayer::resetLevel"',
            '"PlayLayer::markCheckpoint"',
            '"PlayLayer::removeCheckpoint"',
            '"PlayLayer::removeAllCheckpoints"',
            '"GJBaseGameLayer::update"',
            '"GJBaseGameLayer::handleButton"',
        ):
            self.assertNotIn(hook, self.main)

    def test_xdbot_output_drives_live_object_render_mask(self):
        self.assertIn("LayoutMode::getModifiedString", self.layout)
        self.assertIn("buildLayoutMap(real)", self.layout)
        self.assertIn("entry.keep", self.layout)
        self.assertIn("m_startPosition", self.layout)

    def test_xdbot_addobject_visual_mutation_is_preserved(self):
        sequence = r"object->m_activeMainColorID = -1;\s*object->m_activeDetailColorID = -1;\s*object->m_detailUsesHSV = false;\s*object->m_baseUsesHSV = false;\s*object->m_hasNoGlow = true;\s*object->m_isHide = object->m_objectID == 2065;\s*object->setOpacity\(object->m_objectID == 2065 \? 0 : 255\);\s*object->setVisible\(object->m_objectID != 2065\);"
        self.assertRegex(self.layout, sequence)

    def test_local_layout_mutations_are_restored_same_frame(self):
        self.assertRegex(self.layout, r"applyLayoutOverrides\(real\);[\s\S]*scene->visit\(\);[\s\S]*restoreLayoutOverrides\(\);")
        self.assertIn("SavedVisualState", self.header)

    def test_shader_pass_is_bypassed_only_during_layout_rerender(self):
        self.assertIn("LayoutMirror::get().isRenderingLayout()", self.main)
        self.assertIn("cocos2d::CCLayer::visit()", self.main)
        self.assertIn("ShaderLayer::visit();", self.main)

    def test_spout_capture_precedes_layout_rerender(self):
        self.assertRegex(
            self.main,
            r"sendDefaultFramebuffer\(\);[\s\S]*renderPlayerView\(director, real\);[\s\S]*CCEGLView::swapBuffers\(\)",
        )


if __name__ == "__main__":
    unittest.main()
