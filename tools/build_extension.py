#!/usr/bin/env python3
"""Build the locked Cesium for Godot GDExtension on the current host."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform as host_platform
import shutil
import subprocess
import sys
from typing import Sequence


ROOT = Path(__file__).resolve().parents[1]


def _host_defaults() -> tuple[str, str]:
    system = host_platform.system().lower()
    machine = host_platform.machine().lower()
    if system == "windows":
        return "windows", "x64"
    if system == "darwin":
        return "macos", "arm64" if machine in ("arm64", "aarch64") else "x86_64"
    if system == "linux":
        return "linux", "arm64" if machine in ("arm64", "aarch64") else "x64"
    raise RuntimeError(f"unsupported build host: {system}/{machine}")


def _run(command: Sequence[str], environment: dict[str, str]) -> None:
    print("+", subprocess.list2cmdline(list(command)), flush=True)
    subprocess.run(list(command), cwd=ROOT, env=environment, check=True)


def _expected_output(
    platform_name: str,
    architecture: str,
    target: str = "template_release",
    precision: str = "single",
) -> Path:
    library_dir = ROOT / "addons" / "cesium_godot" / "lib"
    architecture_suffix = "x86_64" if architecture == "x64" else architecture
    precision_suffix = ".double" if precision == "double" else ""
    if platform_name == "windows":
        pattern = (
            f"CesiumGodot.windows.{target}{precision_suffix}."
            f"{architecture_suffix}.dll"
        )
    elif platform_name == "macos":
        pattern = (
            f"libCesiumGodot.macos.{target}{precision_suffix}."
            f"{architecture_suffix}.dylib"
        )
    elif platform_name == "android":
        pattern = (
            f"libCesiumGodot.android.{target}{precision_suffix}."
            f"{architecture_suffix}.so"
        )
    else:
        pattern = (
            f"libCesiumGodot.linux.{target}{precision_suffix}."
            f"{architecture_suffix}.so"
        )
    return library_dir / pattern


def parse_arguments(arguments: Sequence[str] | None = None) -> argparse.Namespace:
    default_platform, default_arch = _host_defaults()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--platform", choices=("linux", "windows", "macos", "android"), default=default_platform
    )
    parser.add_argument("--arch", default=default_arch)
    parser.add_argument(
        "--precision",
        choices=("single",),
        default="single",
        help="Godot 4.6.3 single precision; the only ABI locked and distributed by this repository",
    )
    parser.add_argument(
        "--target",
        choices=("editor", "template_debug", "template_release"),
        default="template_release",
    )
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    parser.add_argument("--skip-native", action="store_true")
    parser.add_argument(
        "--debug-symbols",
        action="store_true",
        help="retain native debug symbols while keeping the packaged release ABI",
    )
    parser.add_argument("--verify-only", action="store_true")
    parser.add_argument("--json", action="store_true", dest="json_output")
    return parser.parse_args(arguments)


def _resolve_android_ndk_root(environment: dict[str, str], version: str) -> Path:
    """Prefer the locked side-by-side SDK NDK over a host's stale default."""
    sdk_root = (
        environment.get("ANDROID_HOME", "").strip()
        or environment.get("ANDROID_SDK_ROOT", "").strip()
    )
    if sdk_root:
        locked_sdk_ndk = Path(sdk_root).expanduser() / "ndk" / version
        if (locked_sdk_ndk / "build/cmake/android.toolchain.cmake").is_file():
            return locked_sdk_ndk

    explicit_ndk = environment.get("ANDROID_NDK_ROOT", "").strip()
    if explicit_ndk:
        return Path(explicit_ndk).expanduser()
    if not sdk_root:
        raise SystemExit(
            "Android builds require ANDROID_HOME or ANDROID_NDK_ROOT; "
            f"install NDK {version}"
        )
    return Path(sdk_root).expanduser() / "ndk" / version


def main(arguments: Sequence[str] | None = None) -> int:
    args = parse_arguments(arguments)
    if args.jobs < 1:
        raise SystemExit("--jobs must be positive")
    host_name, _host_arch = _host_defaults()
    if args.platform != host_name and not (
        args.platform == "android" and host_name == "linux"
    ):
        raise SystemExit(
            f"cross-compilation is not configured: host is {host_name}, target is {args.platform}; "
            "Android arm64 and x86_64 may be cross-compiled from Linux"
        )
    supported_targets = {
        ("linux", "x64"),
        ("windows", "x64"),
        ("macos", "arm64"),
        ("android", "arm64"),
        ("android", "x86_64"),
    }
    if (args.platform, args.arch) not in supported_targets:
        raise SystemExit(
            f"unsupported extension target: {args.platform}/{args.arch}; "
            "supported targets are Linux x64, Windows x64, macOS arm64, "
            "Android arm64, and Android x86_64"
        )
    environment = os.environ.copy()
    if environment.get("CESIUM_GODOT_CLEAN_BUILD") == "true":
        environment.pop("SCONS_CACHE", None)
        environment["VCPKG_BINARY_SOURCES"] = "clear"
    environment["CESIUM_GODOT_BUILD_JOBS"] = str(args.jobs)
    lock = json.loads((ROOT / "dependencies.lock.json").read_text(encoding="utf-8"))
    if args.platform == "android":
        ndk_version = lock["toolchain"]["android_ndk"]
        ndk_root = _resolve_android_ndk_root(environment, ndk_version)
        if not (ndk_root / "build/cmake/android.toolchain.cmake").is_file():
            raise SystemExit(f"locked Android NDK {ndk_version} is missing at {ndk_root}")
        environment["ANDROID_NDK_ROOT"] = str(ndk_root)
        environment["ANDROID_NDK_HOME"] = str(ndk_root)

    bootstrap = [
        sys.executable,
        str(ROOT / "tools" / "bootstrap_dependencies.py"),
        "--root",
        str(ROOT),
    ]
    if args.verify_only:
        bootstrap.append("--verify-only")
    _run(bootstrap, environment)
    if args.verify_only:
        return 0

    scons = shutil.which("scons")
    if scons is None:
        raise SystemExit("scons is required; install the locked version from dependencies.lock.json")
    command = [
        scons,
        f"-j{args.jobs}",
        f"platform={args.platform}",
        f"arch={args.arch}",
        "compileTarget=extension",
        f"target={args.target}",
        f"buildCesium={'no' if args.skip_native else 'yes'}",
    ]
    if args.platform == "android":
        command.extend(
            (
                f"ndk_version={lock['toolchain']['android_ndk']}",
                "android_api_level=24",
            )
        )
    if args.debug_symbols:
        command.append("debug_symbols=yes")
    _run(command, environment)

    output = _expected_output(
        args.platform,
        args.arch,
        args.target,
        args.precision,
    )
    if not output.is_file():
        raise SystemExit(
            f"build completed without the expected GDExtension library: {output}"
        )
    report = {
        "platform": args.platform,
        "arch": args.arch,
        "precision": args.precision,
        "target": args.target,
        "outputs": [str(output)],
        "dependency_directory": str(ROOT / "build" / "dependencies"),
    }
    if args.json_output:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print("Locked GDExtension build completed:")
        print(f"  {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
