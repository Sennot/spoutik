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

    def test_spout_does_not_treat_gldx_false_as_cpu_fallback(self):
        self.assertIn("SendFbo(0, 0, 0, invert)", self.spout)
        self.assertRegex(self.spout, r"if \(m_spout->GetCPU\(\)\)")
        self.assertNotRegex(
            self.spout,
            r"GetCPU\(\)\s*\|\|\s*!m_spout->GetGLDX\(\)",
            "GetGLDX is compatibility information, not the sender sharing method",
        )

    def test_transient_spout_send_failure_retries(self):
        self.assertIn("will retry", self.spout)
        self.assertNotIn("m_cpuFallbackRejected = true;\n        return false;\n    }\n    return sent", self.spout)

    def test_mirror_root_hooks_run_very_late(self):
        for name in (
            "PlayLayer::init",
            "PlayLayer::startGame",
            "PlayLayer::resetLevel",
            "GJBaseGameLayer::update",
            "GJBaseGameLayer::handleButton",
        ):
            self.assertIn(f'"{name}"', self.main)
        self.assertGreaterEqual(self.main.count("Priority::VeryLate"), 2)

    def test_mirror_guards_remain_after_root_priority(self):
        for name in (
            "PlayLayer::prepareMusic",
            "PlayLayer::startMusic",
            "PlayLayer::addObject",
            "PlayLayer::levelComplete",
            "PlayLayer::updateAttempts",
            "PlayLayer::onQuit",
        ):
            self.assertIn(f'"{name}"', self.main)
        self.assertIn("Priority::Last", self.main)

    def test_hidden_playlayer_has_no_autonomous_scheduler_callbacks(self):
        self.assertIn("unscheduleAllForTarget(m_mirror)", self.layout)
        release = re.search(
            r"void LayoutMirror::releaseMirror\(\) \{(?P<body>[\s\S]*?)\n\}", self.layout
        )
        self.assertIsNotNone(release)
        self.assertLess(
            release.group("body").find("quiesceMirrorScheduler();"),
            release.group("body").find("m_mirror->release();"),
        )


if __name__ == "__main__":
    unittest.main()
