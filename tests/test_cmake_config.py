#!/usr/bin/env python3
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class CMakeConfigTests(unittest.TestCase):
    def test_mod_target_uses_plain_link_signature_for_geode_compatibility(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertRegex(cmake, r"target_link_libraries\(\$\{PROJECT_NAME\}\s+opengl32\)")
        self.assertIsNone(
            re.search(
                r"target_link_libraries\(\$\{PROJECT_NAME\}\s+(?:PRIVATE|PUBLIC|INTERFACE)\b",
                cmake,
            ),
            "setup_geode_mod() uses the plain target_link_libraries signature; do not mix keyword form",
        )


if __name__ == "__main__":
    unittest.main()
