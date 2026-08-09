#!/usr/bin/env python3
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class CMakeConfigTests(unittest.TestCase):
    def test_geode_target_has_no_render_or_spout_dependencies(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("src/CompanionBridge.cpp", cmake)
        for forbidden in ("FrameCompositor", "SpoutSender", "vendor/spout", "opengl32"):
            self.assertNotIn(forbidden, cmake)

    def test_companion_pins_sdl3_and_opengl(self):
        cmake = (ROOT / "companion/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("GIT_TAG release-3.4.10", cmake)
        self.assertIn("SDL3::SDL3", cmake)
        self.assertIn("OpenGL::GL", cmake)
        self.assertIn("LayoutCompanion", cmake)


if __name__ == "__main__":
    unittest.main()
