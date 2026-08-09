#!/usr/bin/env python3
import json
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class ModMetadataTests(unittest.TestCase):
    def test_geode_sdk_version_has_no_git_tag_prefix(self):
        mod = json.loads((ROOT / "mod.json").read_text(encoding="utf-8"))
        target = mod["geode"]
        self.assertEqual(target, "5.8.2")
        self.assertFalse(target.startswith("v"))

    def test_geodefile_major_minor_comparison_matches_582_sdk(self):
        # Mirrors Geode v5.8.2 GeodeFile.cmake lines 83-89 closely enough
        # to catch the exact v5.8.2 -> v5.8 mismatch that broke CI.
        mod = json.loads((ROOT / "mod.json").read_text(encoding="utf-8"))
        def short(v: str) -> str:
            return re.sub(r"([0-9]+\.[0-9]+)\.[0-9]+", r"\1", v)
        self.assertEqual(short(mod["geode"]), short("5.8.2"))
        self.assertEqual(short(mod["geode"]), "5.8")

    def test_action_sdk_tag_keeps_v_prefix(self):
        workflow = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
        self.assertIn("sdk: v5.8.2", workflow)

    def test_companion_architecture_version_and_resources(self):
        mod = json.loads((ROOT / "mod.json").read_text(encoding="utf-8"))
        self.assertEqual(mod["version"], "v0.4.1")
        self.assertEqual(mod["name"], "Layout Companion Bridge")
        resources = mod["resources"]["files"]
        self.assertIn("resources/licenses/XDBotFork-CREDITS.txt", resources)
        self.assertFalse(any("Spout" in resource for resource in resources))


if __name__ == "__main__":
    unittest.main()
