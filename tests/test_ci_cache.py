import copy
import json
from pathlib import Path
import unittest
from tools.ci_cache import namespaces
from tools.download_test_engine import engine_asset


class CacheKeysTest(unittest.TestCase):
    def setUp(self):
        self.lock = json.loads((Path(__file__).resolve().parents[1] / "dependencies.lock.json").read_text())

    def keys(self, lock=None, platform="linux", arch="x64", image="image1"):
        return namespaces(lock or self.lock, platform, arch, image)

    def test_native_patch_does_not_discard_unrelated_cache(self):
        changed = copy.deepcopy(self.lock)
        changed["dependencies"]["cesium_native"]["patched_tree"] = "different"
        self.assertEqual(self.keys(), self.keys(changed))

    def test_vcpkg_change_only_changes_package_namespace(self):
        changed = copy.deepcopy(self.lock)
        changed["dependencies"]["vcpkg"]["baseline"] = "different"
        self.assertNotEqual(self.keys()["vcpkg"], self.keys(changed)["vcpkg"])
        self.assertEqual(self.keys()["compiler"], self.keys(changed)["compiler"])

    def test_toolchain_and_target_isolation(self):
        changed = copy.deepcopy(self.lock)
        changed["toolchain"]["scons"] = "different"
        for keys in [self.keys(changed), self.keys(platform="android"), self.keys(arch="arm64"), self.keys(image="image2")]:
            self.assertNotEqual(self.keys()["compiler"], keys["compiler"])

    def test_engine_download_targets(self):
        self.assertEqual(engine_asset("4.6.3", "Linux")[0], "Godot_v4.6.3-stable_linux.x86_64.zip")
        self.assertEqual(engine_asset("4.7.2", "Windows")[1], "Godot_v4.7.2-stable_win64_console.exe")

    def test_saves_precede_tests_and_clean_runs_skip_cache(self):
        workflow = (Path(__file__).resolve().parents[1] / ".github/workflows/build-matrix.yml").read_text()
        self.assertLess(workflow.index("Save Cesium Native compiler cache"), workflow.index("Run Cesium Native tests"))
        self.assertIn("inputs.clean_build != true", workflow)
        self.assertIn("schedule:", workflow)
        self.assertIn("needs: [build, runtime, managed]", workflow)
