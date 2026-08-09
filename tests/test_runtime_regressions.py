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
        cls.compositor = (ROOT / "src/FrameCompositor.cpp").read_text(encoding="utf-8")
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
        self.assertIn("SendFbo(framebuffer, width, height, invert)", self.spout)
        self.assertIn("m_spout->SendTexture(", self.spout)
        self.assertIn("GL_TEXTURE_2D", self.spout)

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
        for token in ("object", "object->m_colorSprite", "object->m_glowSprite"):
            self.assertIn(token, block)
        # Claimed particle systems are pooled and may change owners at runtime;
        # a permanent pointer decision would hide an unrelated later object.
        self.assertNotIn("object->m_particle", block)
        self.assertIn("RenderNodeKind::Main", block)
        self.assertIn("RenderNodeKind::Detail", block)
        self.assertIn("RenderNodeKind::Suppress", block)

    def test_hot_path_masks_actual_ccnode_visit_without_full_level_scan(self):
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
        self.assertIn("Layout stable mask frame", self.layout)
        camera = re.search(
            r"void LayoutMirror::applyCameraOverrides\(CCDirector\* director, PlayLayer\* real\) \{([\s\S]*?)\n\}",
            self.layout,
        ).group(1)
        self.assertNotRegex(camera, r"for \(auto const& entry : m_entries\)")

    def test_batched_objects_use_stable_spatial_camera_candidates(self):
        self.assertIn('#include <Geode/modify/GameObject.hpp>', self.main)
        self.assertNotIn('"GameObject::setVisible"', self.main)
        self.assertNotIn("observeVisibility", self.main + self.layout + self.header)
        batch = re.search(
            r"void LayoutMirror::touchCameraEntry\(LayoutEntry& entry\) \{([\s\S]*?)\n\}",
            self.layout,
        )
        self.assertIsNotNone(batch)
        self.assertIn("getBatchNode()", batch.group(1))
        self.assertIn("entry.keep", batch.group(1))
        camera = re.search(
            r"void LayoutMirror::applyCameraOverrides\(CCDirector\* director, PlayLayer\* real\) \{([\s\S]*?)\n\}",
            self.layout,
        ).group(1)
        for token in (
            "m_calcNonEffectObjects",
            "m_calcNonEffectObjectsSize",
            "m_visibleObjects",
            "m_visibleObjectsCount",
            "m_visibleObjects2",
            "m_visibleObjects2Count",
            "convertToNodeSpace",
            "std::lower_bound",
            "m_spatialEntries",
            "m_frameSpatialCandidateCount",
            "m_nonEffectObjects",
            "m_sections",
        ):
            self.assertIn(token, camera)
        self.assertIn("m_spatialIndexReady", self.header)
        self.assertIn("Stable layout spatial index ready", self.layout)

    def test_xdbot_opacity_has_separate_layout_state(self):
        self.assertIn("unsigned char layoutOpacity", self.header)
        self.assertIn("observeOpacity", self.header)
        self.assertIn('"GameObject::setOpacity"', self.main)
        self.assertRegex(
            self.main,
            r"GameObject::setOpacity\(opacity\);\s*LayoutMirror::get\(\)\.observeOpacity\(this, opacity\);",
        )
        self.assertIn("setSpriteVisible(object, true, true)", self.layout)
        self.assertIn("setSpriteOpacity(object, entry.layoutOpacity)", self.layout)
        self.assertIn("m_isGroupDisabled", self.layout)

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
        self.assertIn("restoreOpacity", self.layout)

    def test_local_layout_mutations_are_restored_same_frame(self):
        self.assertRegex(
            self.layout,
            r"renderPlayerViewToFramebuffer[\s\S]*beginLayoutPass\(director, real\);[\s\S]*scene->visit\(\);[\s\S]*endLayoutPass\(\);",
        )
        self.assertIn("SavedVisualState", self.header)

    def test_shader_pass_is_bypassed_only_during_layout_rerender(self):
        self.assertIn("LayoutMirror::get().isRenderingLayout()", self.main)
        self.assertIn("m_gameLayer->m_inShaderObjectLayer", self.main)
        self.assertIn("rawLayer->visit()", self.main)
        self.assertNotIn("m_gameLayer->m_inShaderParent->visit()", self.main)
        self.assertNotIn("cocos2d::CCLayer::visit()", self.main)
        self.assertIn("ShaderLayer::visit();", self.main)

    def test_layout_render_happens_in_drawscene_not_swapbuffers(self):
        draw = self.main[self.main.index("void drawScene()") : self.main.index("class $modify(SpoutLayoutEGLView")]
        swap = self.main[self.main.index("void swapBuffers()") :]
        self.assertLess(draw.index("CCDirector::drawScene();"), draw.index("prepareLocalFrame(this, real)"))
        self.assertIn("sendPreparedSpoutFrame(director, real)", swap)
        for forbidden in ("renderPlayerView", "scene->visit", "glClear(", "drawFullscreen"):
            self.assertNotIn(forbidden, swap)
        self.assertLess(swap.index("sendPreparedSpoutFrame"), swap.index("CCEGLView::swapBuffers();"))

    def test_hackmega_overlay_is_preserved_on_both_outputs(self):
        self.assertIn("setHookPriorityAfterPre", self.main)
        self.assertIn('"absolllute.hackmega"', self.main)
        self.assertIn("captureDefaultTo(m_decoratedTexture)", self.compositor)
        self.assertIn("captureDefaultTo(m_presentedTexture)", self.compositor)
        self.assertIn("presentedPixel.rgb - baselinePixel.rgb", self.compositor)
        self.assertNotIn("delta.a", self.compositor)
        self.assertNotIn("glReadPixels", self.compositor)

    def test_compositor_owns_isolated_gpu_targets(self):
        for token in (
            "m_decoratedTexture",
            "m_layoutTexture",
            "m_presentedTexture",
            "m_spoutTexture",
            "m_layoutFramebuffer",
            "m_spoutFramebuffer",
            "GL_DEPTH_STENCIL_ATTACHMENT",
            "m_readyGeneration != m_generation",
            "m_frameScene != director->getRunningScene()",
            "m_frameLayer != real",
        ):
            self.assertIn(token, self.compositor + self.header)
        self.assertIn("sendFramebuffer(m_spoutFramebuffer", self.compositor)
        self.assertIn("sendTexture(m_decoratedTexture", self.compositor)

    def test_transition_scenes_are_never_rerendered(self):
        self.assertIn("isStableGameplayScene", self.main + self.layout + self.header)
        self.assertIn("while (root->getParent())", self.layout)
        self.assertIn("return root == scene", self.layout)
        self.assertIn("typeinfo_cast<cocos2d::CCTransitionScene*>(scene)", self.layout)
        self.assertIn("director->getNextScene()", self.layout)
        self.assertIn("kStableDrawsBeforeLayout = 3", self.main)
        self.assertIn("advancePresentationGate", self.main)
        self.assertIn("willSwitchToScene", self.main)
        self.assertRegex(
            self.main,
            r"advancePresentationGate\(this, real\);[\s\S]*if \(stable\) compositor\.prepareLocalFrame\(this, real\);[\s\S]*else compositor\.invalidate\(\);",
        )
        self.assertGreaterEqual(self.main.count("FrameCompositor::get().invalidate();"), 2)

    def test_compositor_restores_vertex_attribute_state(self):
        for token in (
            "GL_VERTEX_ATTRIB_ARRAY_ENABLED",
            "GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING",
            "glGetVertexAttribPointerv",
            "restoreAttrib(kCCVertexAttrib_Position",
            "restoreAttrib(kCCVertexAttrib_TexCoords",
        ):
            self.assertIn(token, self.compositor)
        self.assertNotIn("ccGLEnableVertexAttribs", self.compositor)

    def test_layout_redraw_only_clears_its_private_framebuffer(self):
        render = re.search(
            r"bool LayoutMirror::renderPlayerViewToFramebuffer\([\s\S]*?\) \{([\s\S]*?)\n\}",
            self.layout,
        ).group(1)
        self.assertLess(render.index("previousFramebuffer != 0"), render.index("glClear("))
        self.assertLess(render.index("glBindFramebuffer(GL_FRAMEBUFFER, framebuffer)"), render.index("glClear("))
        self.assertIn("glCheckFramebufferStatus(GL_FRAMEBUFFER)", render)

    def test_local_redraw_keeps_cocos_projection_setup(self):
        self.assertIn("director->setProjection(director->getProjection());", self.layout)
        self.assertNotIn("glStencilMask(~0u)", self.layout)
        self.assertIn("previousViewport", self.layout)
        self.assertIn("previousClearColor", self.layout)


if __name__ == "__main__":
    unittest.main()
