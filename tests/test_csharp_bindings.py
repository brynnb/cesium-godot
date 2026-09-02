#!/usr/bin/env python3
"""Offline structure checks for the committed generated C# facade."""

from __future__ import annotations

import json
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
LOCK_PATH = ROOT / "dependencies/csharp-bindgen.lock.json"
OUTPUT = ROOT / "godot3dtiles/addons/cesium_godot/csharp"


class CSharpBindingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.lock = json.loads(LOCK_PATH.read_text(encoding="utf-8"))
        cls.files = sorted(OUTPUT.glob("*.cs"))

    def test_generator_is_exactly_pinned_to_the_project_fork(self) -> None:
        self.assertEqual(self.lock["schema_version"], 1)
        self.assertEqual(
            self.lock["repository"],
            "https://github.com/brynnb/godot-csharp-gdextension-bindgen.git",
        )
        self.assertRegex(self.lock["commit"], r"^[0-9a-f]{40}$")
        self.assertRegex(self.lock["tree"], r"^[0-9a-f]{40}$")
        self.assertEqual(self.lock["godot_version"], "4.6.3")

    def test_expected_wrapper_set_is_committed(self) -> None:
        self.assertEqual(len(self.files), self.lock["expected_class_count"])
        names = {path.name for path in self.files}
        self.assertIn("Cesium3DTileset.cs", names)
        self.assertIn("CesiumGeoreference.cs", names)
        self.assertIn("CesiumRasterOverlay.cs", names)

    def test_generated_code_excludes_broken_inherited_api_copies(self) -> None:
        combined = "\n".join(path.read_text(encoding="utf-8") for path in self.files)
        self.assertNotIn("#region Inherited", combined)
        self.assertNotRegex(combined, re.compile(r"^\s*[0-9][A-Za-z0-9_]*\s*=", re.MULTILINE))
        self.assertIn("public Node3D NativeObject => _object;", combined)
        self.assertIn("public enum ScaleEnum", combined)
        self.assertIn("Value2x = 1L", combined)

    def test_lifecycle_callbacks_are_language_neutral(self) -> None:
        receiver = (
            OUTPUT / "Cesium3DTilesetLifecycleEventReceiver.cs"
        ).read_text(encoding="utf-8")
        self.assertIn("public Godot.Callable MaterialSelector", receiver)
        self.assertIn("public event Action<GodotObject, Material> MaterialCustomizing", receiver)
        self.assertIn("public event Action<GodotObject> TileLoaded", receiver)
        self.assertIn("public event Action<GodotObject, bool> TileVisibilityChanged", receiver)

        lifecycle_source = (
            ROOT
            / "cesium_godot/Runtime/Private/Cesium3DTilesetLifecycleEventReceiver.cpp"
        ).read_text(encoding="utf-8")
        excluder_source = (
            ROOT / "cesium_godot/Godot/Nodes/CesiumTileExcluder.cpp"
        ).read_text(encoding="utf-8")
        for hidden_hook in (
            "_create_material",
            "_customize_material",
            "_on_tile_loaded",
            "_on_tile_unloading",
            "_should_exclude",
        ):
            self.assertNotIn(hidden_hook, lifecycle_source + excluder_source)


if __name__ == "__main__":
    unittest.main()
