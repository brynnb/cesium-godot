#!/usr/bin/env python3
"""Offline checks for the cross-platform build contract."""

from __future__ import annotations

import importlib.util
import contextlib
import io
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
BUILD_SCRIPT = ROOT / "tools" / "build_extension.py"
SPEC = importlib.util.spec_from_file_location("build_extension", BUILD_SCRIPT)
assert SPEC is not None and SPEC.loader is not None
BUILD_EXTENSION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILD_EXTENSION)


class BuildConfigurationTests(unittest.TestCase):
    def test_locked_build_tools_match_ci_and_documentation(self) -> None:
        lock = json.loads(
            (ROOT / "dependencies.lock.json").read_text(encoding="utf-8")
        )
        self.assertEqual(lock["toolchain"]["scons"], "4.11.1")
        self.assertEqual(lock["toolchain"]["cmake"], "4.4.3")
        expected_install = "scons==4.11.1 cmake==4.4.3 ninja==1.13.0"
        for relative_path in (
            ".github/workflows/build-matrix.yml",
            "docs/BUILD_MACOS.md",
            "docs/BUILD_VISUAL_STUDIO.md",
            "docs/REPRODUCIBLE_BUILDS.md",
        ):
            self.assertIn(
                expected_install,
                (ROOT / relative_path).read_text(encoding="utf-8"),
                relative_path,
            )

    def test_lifecycle_demo_registers_the_extension_on_a_clean_checkout(self) -> None:
        extension_list = (
            ROOT / "examples/lifecycle_material_demo/.godot/extension_list.cfg"
        )
        self.assertEqual(
            extension_list.read_text(encoding="utf-8"),
            "res://Godot3DTiles.gdextension\n",
        )

    def test_editor_fixture_enables_the_packaged_plugin(self) -> None:
        fixture = ROOT / "tests/godot-editor"
        self.assertTrue((fixture / "addons/cesium_godot").is_symlink())
        self.assertEqual(
            (fixture / "addons/cesium_godot").resolve(),
            (ROOT / "godot3dtiles/addons/cesium_godot").resolve(),
        )
        project = (fixture / "project.godot").read_text(encoding="utf-8")
        self.assertIn('"res://addons/cesium_godot/plugin.cfg"', project)
        self.assertIn(
            '"res://addons/editor_smoke_probe/plugin.cfg"', project
        )
        smoke_test = (
            fixture / "addons/editor_smoke_probe/editor_smoke_probe.gd"
        ).read_text(
            encoding="utf-8"
        )
        self.assertIn('find_child("Cesium Ion Panel", true, false)', smoke_test)

    def test_editor_panels_do_not_require_imported_icons_to_register(self) -> None:
        addon = ROOT / "godot3dtiles/addons/cesium_godot"
        for relative_path in (
            "cesium_godot.gd",
            "panels/cesium_panel.tscn",
            "panels/token_panel.tscn",
        ):
            source = (addon / relative_path).read_text(encoding="utf-8")
            self.assertNotIn(".svg", source, relative_path)

    def test_expected_artifact_names_match_gdextension_manifest(self) -> None:
        expected = {
            ("linux", "x64"): "libGodot3DTiles.linux.template_release.x86_64.so",
            ("windows", "x64"): "Godot3DTiles.windows.template_release.x86_64.dll",
            ("macos", "arm64"): "libGodot3DTiles.macos.template_release.arm64.dylib",
            ("android", "arm64"): "libGodot3DTiles.android.template_release.arm64.so",
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
            "Godot_v4.7.2-stable_linux.x86_64.zip",
            "Godot_v$version-stable_win64.exe.zip",
            "Test packaged addon on Godot 4.6.3 for Windows",
            "Test Cesium for Godot on Godot 4.7.2",
            "Test editor dock on Godot 4.7.2",
            "tests/run_editor_tests.sh",
            "tests/run_editor_tests.ps1",
            "tests/run_2dog_tests.ps1",
            "--smoke-test",
            "tools/bootstrap_dependencies.py --verify-only",
            "undefined symbol:",
        ):
            self.assertIn(required, workflow)
        self.assertNotIn("Godot_v4.2.2", workflow)
        self.assertNotIn("precision: double", workflow)

    def test_windows_static_link_closure_matches_current_dependencies(self) -> None:
        build = (ROOT / "cesium_godot/SCsub").read_text(encoding="utf-8")
        windows = build[
            build.index('if target_platform == "windows":') :
            build.index('elif target_platform == "macos":')
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
            '"user32"',
        ):
            self.assertIn(required, windows)
        self.assertNotIn('"zlib",', windows)

    def test_android_build_is_target_aware_and_excludes_desktop_html_archives(self) -> None:
        build_utils = (ROOT / "CesiumBuildUtils.py").read_text(encoding="utf-8")
        extension_build = (ROOT / "cesium_godot/SCsub").read_text(encoding="utf-8")
        orchestrator = (ROOT / "SConstruct.py").read_text(encoding="utf-8")
        wrapper = BUILD_SCRIPT.read_text(encoding="utf-8")
        lock = json.loads((ROOT / "dependencies.lock.json").read_text(encoding="utf-8"))

        self.assertEqual(lock["toolchain"]["android_ndk"], "28.1.13356709")
        self.assertIn('return "arm64-android"', build_utils)
        self.assertIn("get_android_ndk_root()", build_utils)
        self.assertIn('"-DANDROID_PLATFORM=android-24"', build_utils)
        self.assertIn("get_compile_flags(ARGUMENTS)", orchestrator)
        self.assertIn("install_additional_libs(ARGUMENTS)", orchestrator)
        self.assertIn('env["platform"] in ("linux", "android")', orchestrator)
        self.assertIn('target_platform == "android"', extension_build)
        self.assertIn("CESIUM_GODOT_NO_LITEHTML", extension_build)
        self.assertIn('(\"android\", \"arm64\")', wrapper)

        network = (
            ROOT
            / "cesium_godot/Runtime/Private/Networking/NetworkAssetAccessor.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("get_system_ca_certificates()", network)
        self.assertIn("options.certificateFile = getAndroidCertificateFile()", network)

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

    def test_posix_static_archives_use_a_response_file_when_needed(self) -> None:
        orchestrator = (ROOT / "SConstruct.py").read_text(encoding="utf-8")
        self.assertIn('if env["platform"] in ("linux", "android"):', orchestrator)
        self.assertIn('env["ARCOM_RESPONSE"] = env["ARCOM"]', orchestrator)
        self.assertIn('env["ARCOM"] = "${TEMPFILE(ARCOM_RESPONSE)}"', orchestrator)

    def test_visual_studio_solution_is_a_front_end_to_the_canonical_build(self) -> None:
        generator = (ROOT / "SConstruct.visual_studio").read_text(encoding="utf-8")
        bridge = (ROOT / "tools/build_visual_studio.py").read_text(encoding="utf-8")
        wrapper = (ROOT / "tools/generate_visual_studio.py").read_text(
            encoding="utf-8"
        )
        workflow = (ROOT / ".github/workflows/build-matrix.yml").read_text(
            encoding="utf-8"
        )

        self.assertIn('MSVC_VERSION="14.5"', generator)
        self.assertIn('variant=["Debug|x64", "Release|x64"]', generator)
        self.assertIn("tools\\\\build_visual_studio.py", generator)
        self.assertIn('ROOT / "tools" / "build_extension.py"', bridge)
        self.assertIn('"debug_symbols=yes"', bridge)
        self.assertIn('"SConstruct.visual_studio"', wrapper)
        self.assertIn("<PlatformToolset>v145</PlatformToolset>", wrapper)
        self.assertIn('if compiled_sources != 86:', wrapper)
        self.assertIn(
            "Validate Visual Studio 2026 solution generation",
            workflow,
        )


if __name__ == "__main__":
    unittest.main()
