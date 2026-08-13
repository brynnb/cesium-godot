#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path


REPOSITORY = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "validate_tileset",
    REPOSITORY / "tools" / "validate_tileset.py",
)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class TilesetValidatorTest(unittest.TestCase):
    def test_known_good_local_example(self) -> None:
        report = MODULE.validate_tileset(
            REPOSITORY / "examples/lifecycle_material_demo/fixtures/tileset.json"
        )
        self.assertTrue(report["valid"], report)
        self.assertEqual(report["errors"], 0)
        self.assertEqual(report["summary"]["tilesets"], 1)
        self.assertEqual(report["summary"]["tiles"], 2)
        self.assertEqual(report["summary"]["gltf_models"], 1)
        self.assertEqual(report["summary"]["files"], 2)

    def test_reports_each_preflight_failure_category(self) -> None:
        report = MODULE.validate_tileset(
            REPOSITORY / "tests/validator_fixtures/invalid/tileset.json",
            maximum_payload_bytes=64,
        )
        self.assertFalse(report["valid"])
        codes = {issue["code"] for issue in report["issues"]}
        self.assertTrue(
            {
                "invalid_bounding_volume",
                "inconsistent_lod_error",
                "missing_file",
                "missing_texture",
                "broken_child_link",
                "broken_material_link",
                "external_tileset_cycle",
                "oversized_payload",
            }.issubset(codes),
            report,
        )


if __name__ == "__main__":
    unittest.main()
