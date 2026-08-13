#!/usr/bin/env python3
"""Offline integrity tests for the reproducible dependency inputs."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]
LOCK_PATH = ROOT / "dependencies.lock.json"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


class DependencyLockTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.lock = json.loads(LOCK_PATH.read_text(encoding="utf-8"))

    def test_lock_has_supported_schema_and_compatibility_floor(self) -> None:
        self.assertEqual(self.lock["schema_version"], 1)
        compatibility = self.lock["godot_compatibility"]
        self.assertEqual(compatibility["minimum"], "4.6.3")
        self.assertEqual(compatibility["api_version"], "4.6")
        self.assertEqual(compatibility["bindings_version"], "4.6.0")
        self.assertEqual(compatibility["tested_versions"], ["4.6.3"])

    def test_all_vendored_files_match_their_hashes(self) -> None:
        dependencies = self.lock["dependencies"]
        locked_files: list[tuple[str, str]] = []
        locked_files.extend(
            (entry["path"], entry["sha256"])
            for entry in dependencies["cesium_native"]["patches"]
        )
        for name in ("mikktspace", "bundled_litehtml"):
            locked_files.extend(dependencies[name]["files"].items())
        self.assertEqual(len(locked_files), len({path for path, _ in locked_files}))
        for relative_path, expected_hash in locked_files:
            path = ROOT / relative_path
            self.assertTrue(path.is_file(), relative_path)
            self.assertEqual(sha256(path), expected_hash, relative_path)

    def test_native_patch_series_is_ordered_and_complete(self) -> None:
        patches = self.lock["dependencies"]["cesium_native"]["patches"]
        self.assertEqual(len(patches), 13)
        self.assertEqual(
            [Path(entry["path"]).name[:4] for entry in patches],
            [f"{index:04d}" for index in range(1, 14)],
        )
        native = self.lock["dependencies"]["cesium_native"]
        self.assertEqual(native["ref"], "v0.63.0")
        self.assertNotEqual(native["base_tree"], native["patched_tree"])

    def test_all_git_dependencies_lock_commits_and_trees(self) -> None:
        dependencies = self.lock["dependencies"]
        self.assertRegex(dependencies["godot_cpp"]["commit"], r"^[0-9a-f]{40}$")
        self.assertRegex(dependencies["godot_cpp"]["tree"], r"^[0-9a-f]{40}$")
        self.assertRegex(dependencies["cesium_native"]["base_commit"], r"^[0-9a-f]{40}$")
        self.assertRegex(dependencies["cesium_native"]["base_tree"], r"^[0-9a-f]{40}$")
        self.assertRegex(dependencies["cesium_native"]["patched_tree"], r"^[0-9a-f]{40}$")
        self.assertRegex(dependencies["vcpkg"]["baseline"], r"^[0-9a-f]{40}$")
        self.assertRegex(dependencies["vcpkg"]["tree"], r"^[0-9a-f]{40}$")

    def test_bundled_only_verifier_is_offline(self) -> None:
        result = subprocess.run(
            [
                "python3",
                str(ROOT / "tools" / "bootstrap_dependencies.py"),
                "--root",
                str(ROOT),
                "--only",
                "bundled",
                "--verify-only",
                "--json",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        report = json.loads(result.stdout)
        self.assertEqual(report["mode"], "verify")
        self.assertEqual(report["bundled"]["files"], 8)

    def test_example_benchmark_copy_matches_the_distributable_addon(self) -> None:
        addon_script = (
            ROOT
            / "godot3dtiles/addons/cesium_godot/scripts/cesium_streaming_benchmark.gd"
        )
        example_script = (
            ROOT / "examples/lifecycle_material_demo/cesium_streaming_benchmark.gd"
        )
        self.assertEqual(example_script.read_bytes(), addon_script.read_bytes())


if __name__ == "__main__":
    unittest.main()
