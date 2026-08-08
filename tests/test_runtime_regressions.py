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
        self.assertIn("buildLayoutPlan(level)", self.layout)
        self.assertIn("canonicalWithoutHidden", self.layout)
        self.assertIn("m_pendingByObjectID", self.layout)
        self.assertIn("observeObject", self.layout)
        self.assertIn("entry.keep", self.layout)
        self.assertNotIn("m_startPosition", self.layout)
        self.assertNotIn("ObjectKey", self.layout)

    def test_layout_map_avoids_windows_near_macro(self):
        # windows.h retains `near` as an empty compatibility macro.
        self.assertNotRegex(self.layout, r"\bObjectKey\s+near\b")
        self.assertNotIn("consume(near)", self.layout)

    def test_objects_are_bound_during_authoritative_addobject(self):
        self.assertRegex(
            self.main,
            r"void addObject\(GameObject\* object\) \{\s*PlayLayer::addObject\(object\);\s*LayoutMirror::get\(\)\.observeObject\(this, object\);",
        )
        self.assertLess(self.main.index("layout.prepareFor(this, level)"), self.main.index("PlayLayer::init(level"))
        self.assertLess(self.main.index("PlayLayer::init(level"), self.main.index("layout.finishFor(this)"))

    def test_full_xdbot_special_palette_is_applied(self):
        for token in (
            "splitView(newColors, '|')",
            "kBackgroundChannel = 1000",
            "kGround1Channel = 1001",
            "kGround2Channel = 1009",
            "kLineChannel = 1002",
            "kMG1Channel = 1013",
            "kMG2Channel = 1014",
            "real->m_groundLayer",
            "real->m_groundLayer2",
            "real->m_middleground",
        ):
            self.assertIn(token, self.layout)

    def test_render_nodes_cover_reparented_object_sprites(self):
        register = re.search(
            r"void LayoutMirror::registerRenderNodes\(LayoutEntry const& entry\) \{([\s\S]*?)\n\}",
            self.layout,
        )
        self.assertIsNotNone(register)
        block = register.group(1)
        for token in ("object", "object->m_colorSprite", "object->m_glowSprite", "object->m_particle"):
            self.assertIn(token, block)
        self.assertIn("RenderNodeKind::Main", block)
        self.assertIn("RenderNodeKind::Detail", block)
        self.assertIn("RenderNodeKind::Suppress", block)

    def test_hot_path_masks_the_actual_ccnode_visit_without_camera_scan(self):
        self.assertIn('#include <Geode/modify/CCNode.hpp>', self.main)
        self.assertIn('"cocos2d::CCNode::visit"', self.main)
        self.assertRegex(
            self.main,
            r"beginNodeVisit\(this\);[\s\S]*NodeVisitAction::Skip\) return;[\s\S]*CCNode::visit\(\);[\s\S]*endNodeVisit\(this\)",
        )
        begin = re.search(
            r"LayoutMirror::NodeVisitAction LayoutMirror::beginNodeVisit\(cocos2d::CCNode\* node\) \{([\s\S]*?)\n\}",
            self.layout,
        )
        self.assertIsNotNone(begin)
        self.assertIn("m_renderNodes.find(node)", begin.group(1))
        self.assertIn("Layout hybrid mask active", self.layout)
        for old_path in ("m_sections", "m_nonEffectObjects", "m_visibleObjects", "m_visibleObjects2"):
            self.assertNotIn(old_path, self.layout)

    def test_batched_objects_use_a_visibility_tracked_active_set(self):
        self.assertIn('#include <Geode/modify/GameObject.hpp>', self.main)
        self.assertIn('"GameObject::setVisible"', self.main)
        self.assertRegex(
            self.main,
            r"GameObject::setVisible\(visible\);\s*LayoutMirror::get\(\)\.observeVisibility\(this, visible\);",
        )
        batch = re.search(
            r"void LayoutMirror::applyBatchedOverrides\(\) \{([\s\S]*?)\n\}",
            self.layout,
        )
        self.assertIsNotNone(batch)
        self.assertIn("m_visibleEntries", batch.group(1))
        self.assertIn("getBatchNode()", batch.group(1))
        self.assertNotRegex(batch.group(1), r"for \(auto const& entry : m_entries\)")

    def test_uninstantiated_removed_deco_does_not_mark_map_incomplete(self):
        self.assertIn("pendingKeep", self.layout)
        self.assertRegex(self.layout, r"if \(m_classifiedKeepCount != m_transformedRecordCount \|\| pendingKeep != 0\)")
        self.assertIn("m_boundKeepCount", self.layout)

    def test_visit_mask_preserves_trigger_visibility_and_opacity(self):
        begin = re.search(
            r"LayoutMirror::NodeVisitAction LayoutMirror::beginNodeVisit\(cocos2d::CCNode\* node\) \{([\s\S]*?)\n\}",
            self.layout,
        ).group(1)
        self.assertNotIn("setVisible", begin)
        self.assertNotIn("setOpacity", begin)
        self.assertNotIn("m_activeMainColorID", begin)
        self.assertIn("sprite->setColor(target)", begin)
        self.assertIn("sprite->setColor(state.color)", self.layout)

    def test_local_layout_mutations_are_restored_same_frame(self):
        self.assertRegex(self.layout, r"beginLayoutPass\(real\);[\s\S]*scene->visit\(\);[\s\S]*endLayoutPass\(\);")
        self.assertIn("SavedVisualState", self.header)

    def test_shader_pass_is_bypassed_only_during_layout_rerender(self):
        self.assertIn("LayoutMirror::get().isRenderingLayout()", self.main)
        self.assertIn("m_gameLayer->m_inShaderParent->visit()", self.main)
        self.assertNotIn("cocos2d::CCLayer::visit()", self.main)
        self.assertIn("ShaderLayer::visit();", self.main)

    def test_spout_capture_precedes_layout_rerender(self):
        self.assertRegex(
            self.main,
            r"sendDefaultFramebuffer\(\);[\s\S]*renderPlayerView\(director, real\);[\s\S]*CCEGLView::swapBuffers\(\)",
        )

    def test_spout_capture_is_ordered_after_hackmega_overlay(self):
        self.assertIn("setHookPriorityAfterPre", self.main)
        self.assertIn('"absolllute.hackmega"', self.main)
        self.assertLess(self.main.index("setHookPriorityAfterPre"), self.main.index("sendDefaultFramebuffer();"))


if __name__ == "__main__":
    unittest.main()
