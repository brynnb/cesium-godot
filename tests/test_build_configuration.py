#!/usr/bin/env python3
"""Offline checks for the cross-platform build contract."""

from __future__ import annotations

import importlib.util
import contextlib
import io
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
BUILD_SCRIPT = ROOT / "tools" / "build_extension.py"
SPEC = importlib.util.spec_from_file_location("build_extension", BUILD_SCRIPT)
assert SPEC is not None and SPEC.loader is not None
BUILD_EXTENSION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILD_EXTENSION)


class BuildConfigurationTests(unittest.TestCase):
    def test_expected_artifact_names_match_gdextension_manifest(self) -> None:
        expected = {
            ("linux", "x64"): "libGodot3DTiles.linux.template_release.x86_64.so",
            ("windows", "x64"): "Godot3DTiles.windows.template_release.x86_64.dll",
            ("macos", "arm64"): "libGodot3DTiles.macos.template_release.arm64.dylib",
        }
        for target, filename in expected.items():
            self.assertEqual(BUILD_EXTENSION._expected_output(*target).name, filename)
        self.assertEqual(
            BUILD_EXTENSION._expected_output(
                "linux", "x64", "template_debug"
            ).name,
            "libGodot3DTiles.linux.template_debug.x86_64.so",
        )

    def test_ci_matrix_covers_supported_artifacts_and_current_runtime(self) -> None:
        workflow = (ROOT / ".github/workflows/build-matrix.yml").read_text(
            encoding="utf-8"
        )
        for required in (
            "ubuntu-24.04",
            "windows-2022",
            "macos-15",
            "Godot_v4.6.3-stable_linux.x86_64.zip",
            "tools/bootstrap_dependencies.py --verify-only",
            "undefined symbol:",
        ):
            self.assertIn(required, workflow)
        self.assertNotIn("Godot_v4.2.2", workflow)
        self.assertNotIn("precision: double", workflow)

    def test_windows_static_link_closure_matches_current_dependencies(self) -> None:
        build = (ROOT / "cesium_godot/SCsub").read_text(encoding="utf-8")
        windows = build[
            build.index("if (os.name == cesium_build_utils.OS_WIN)") :
            build.index("elif sys.platform == cesium_build_utils.PLATFORM_MACOS")
        ]
        for required in (
            '"absl_demangle_rust"',
            '"absl_decode_rust_punycode"',
            '"absl_utf8_for_code_point"',
            '"absl_tracing_internal"',
            '"zs"',
            '"libcrypto"',
            '"iphlpapi"',
            '"secur32"',
        ):
            self.assertIn(required, windows)
        self.assertNotIn('"zlib",', windows)

    def test_only_the_locked_single_precision_godot_abi_is_buildable(self) -> None:
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                BUILD_EXTENSION.parse_arguments(["--precision", "double"])

    def test_default_generated_data_is_inside_visible_build_directory(self) -> None:
        build_root = ROOT / "build"
        for path in (
            ROOT / "build/dependencies/sources/godot-cpp",
            ROOT / "build/dependencies/sources/cesium-native",
            ROOT / "build/dependencies/native-build",
            ROOT / "build/dependencies/vcpkg",
            ROOT / "build/test-engines",
        ):
            self.assertTrue(path.is_relative_to(build_root))
            self.assertNotIn("/tmp/", str(path))

    def test_native_configuration_is_fresh_and_out_of_source(self) -> None:
        build_utils = (ROOT / "CesiumBuildUtils.py").read_text(encoding="utf-8")
        self.assertIn('"--fresh"', build_utils)
        native_build_root = build_utils[
            build_utils.index("def get_native_build_root") :
            build_utils.index("def get_native_library_config_subdir")
        ]
        self.assertIn("DEPENDENCY_BUILD_ROOT", native_build_root)
        self.assertNotIn("return native_root_override", native_build_root)

    def test_locked_vcpkg_is_bootstrapped_before_use(self) -> None:
        bootstrap = (ROOT / "tools/bootstrap_dependencies.py").read_text(
            encoding="utf-8"
        )
        self.assertIn("bootstrap-vcpkg", bootstrap)
        self.assertIn("-disableMetrics", bootstrap)
        self.assertIn('"version"', bootstrap)

    def test_direct_vcpkg_link_dependencies_are_installed(self) -> None:
        build_utils = (ROOT / "CesiumBuildUtils.py").read_text(encoding="utf-8")
        installer = build_utils[
            build_utils.index("def install_additional_libs") :
            build_utils.index("def find_ezvcpkg_path")
        ]
        self.assertIn('f"--x-manifest-root={get_root_dir_native()}"', installer)
        self.assertIn('f"--x-install-root={installed_root}"', installer)
        self.assertIn('f"--triplet={triplet}"', installer)
        for package in ("uriparser", "ada-url", "abseil", "brotli"):
            self.assertIn(f'f"{package}:{{triplet}}"', installer)

        extension_build = (ROOT / "cesium_godot/SCsub").read_text(encoding="utf-8")
        self.assertNotIn('get_native_build_root() + "/libs/absl"', extension_build)

        for obsolete_library in (
            "absl_bad_any_cast_impl",
            "absl_bad_optional_access",
            "absl_bad_variant_access",
            "absl_random_internal_pool_urbg",
            "absl_string_view",
        ):
            self.assertNotIn(f'"{obsolete_library}"', extension_build)

        self.assertIn('"-framework", "CFNetwork"', extension_build)

    def test_native_patches_avoid_msvc_shadow_errors(self) -> None:
        cancellation_patch = (
            ROOT
            / "dependencies/cesium-native-patches/0009-Cancel-stale-tile-requests-across-loading-stages.patch"
        ).read_text(encoding="utf-8")
        self.assertNotIn("+            auto asyncSystem = tileLoadInfo.asyncSystem;", cancellation_patch)
        self.assertNotIn("+            auto tileAsyncSystem = tileLoadInfo.asyncSystem;", cancellation_patch)

    def test_windows_ci_uses_a_short_vcpkg_root(self) -> None:
        workflow = (ROOT / ".github/workflows/build-matrix.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn('"CESIUM_GODOT_VCPKG_ROOT=$env:RUNNER_TEMP/cg-vcpkg"', workflow)
        self.assertIn("runner.temp", workflow)
        self.assertIn("cg-vcpkg", workflow)

    def test_linux_static_archive_uses_a_response_file_when_needed(self) -> None:
        orchestrator = (ROOT / "SConstruct.py").read_text(encoding="utf-8")
        self.assertIn('if env["platform"] == "linux":', orchestrator)
        self.assertIn('env["ARCOM_RESPONSE"] = env["ARCOM"]', orchestrator)
        self.assertIn('env["ARCOM"] = "${TEMPFILE(ARCOM_RESPONSE)}"', orchestrator)


if __name__ == "__main__":
    unittest.main()
